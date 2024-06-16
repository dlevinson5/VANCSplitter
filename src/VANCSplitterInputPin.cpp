////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VANCSplitter.h"
#include "VANCSplitterInputPin.h"

class CVANCSplitter;
class CVANCSplitterOutputPin;

#define VIDEO_BYTE_PER_PIXEL 4
#define VIDEO_VANC_LINES 22
#define VIDEO_VANC_COLUMNS 16

short ReverseShort(short b)
{
	short reverse = 0;
	int bits = (sizeof(short) * 8);
	for(int i = 0; i < bits; i++)
	{
		short bit = ((b >> i) & 1);
		reverse |= (bit << (bits - (i + 1)));
	}
	return reverse;
}

// Convert v210 frame line to VANC buffer
void Convert_UYVY_to_VANC(__int16* v210Buffer, DWORD dwTotalBytes, short* pOutputBuffer)
{
	bool t = false;
	int nBlockPos = 0;
	for(long i = 0; i < (long)(dwTotalBytes / sizeof(__int32)); i++)
	{ 
		unsigned short v210Block = v210Buffer[i];
		unsigned short byte = (v210Block << 8 | v210Block >> 8)  & 0x3ff;  
		pOutputBuffer[nBlockPos] = byte;
		nBlockPos++;
	}
}
  
// Convert v210 frame line to VANC buffer
void Convert_v210_to_BYTES(__int32* v210Buffer, DWORD dwTotalBytes, short* pOutputBuffer)
{
	bool t = false;
	int nBlockPos = 0;

	for(long i = 0; i < (long)(dwTotalBytes / sizeof(__int32)); i++)
	{ 
		__int32 v210Block = v210Buffer[i];
 
		for(int j = 0; j < 3; j++)
		{ 
			if (t)
			{
				__int16 byte = (__int16)(v210Block & 0x3ff);
				pOutputBuffer[nBlockPos] = byte;
				nBlockPos++;
				t = false;
			}
			else
				t = true;
				 
			v210Block = v210Block >> 10;
		}
	}
}

//
// CVANCSplitterInputPin constructor
//
CVANCSplitterInputPin::CVANCSplitterInputPin(TCHAR   *pName,
                           CVANCSplitter    *pTee,
                           HRESULT *phr,
                           LPCWSTR pPinName,
						   int PinNumber) :
    CBaseInputPin(pName, pTee, pTee, phr, pPinName),
    m_pTee(pTee),
    m_bInsideCheckMediaType(FALSE),
	m_nPinNumber(PinNumber), 
	m_pVANCData(NULL),
	m_hReceiveThread(NULL),
	m_nPacketStartPos(-1)
{
    ASSERT(pTee);
	FilterTrace("CVANCSplitterInputPin::CVANCSplitterInputPin\n");
}


//
// CVANCSplitterInputPin destructor
//
CVANCSplitterInputPin::~CVANCSplitterInputPin()
{
    FilterTrace("CVANCSplitterInputPin::~CVANCSplitterInputPin()\n");
    // ASSERT(m_pTee->m_pAllocator == NULL);

	if (m_pVANCData != NULL)
		free (m_pVANCData);

	m_pVANCData = NULL;
}

//
// CheckMediaType
//
HRESULT CVANCSplitterInputPin::CheckMediaType(const CMediaType *pmt)
{
	// FilterTrace("CVANCSplitterInputPin::CheckMediaType()\n");

    CAutoLock lock_it(m_pLock);

    // If we are already inside checkmedia type for this pin, return NOERROR
    // It is possble to hookup two of the tee filters and some other filter
    // like the video effects sample to get into this situation. If we don't
    // detect this situation, we will carry on looping till we blow the stack

    if (m_bInsideCheckMediaType == TRUE)
        return NOERROR;

    m_bInsideCheckMediaType = TRUE;
    HRESULT hr = NOERROR;

#ifdef DEBUG
    // Display the type of the media for debugging perposes
    DisplayMediaType(TEXT("Input Pin Checking"), pmt);
#endif

	if (pmt->formattype == FORMAT_VideoInfo2)
		m_bih = ((VIDEOINFOHEADER2*)pmt->pbFormat)->bmiHeader;
	else if (pmt->formattype == FORMAT_VideoInfo)
		m_bih = ((VIDEOINFOHEADER*)pmt->pbFormat)->bmiHeader;
	 
	m_connectedType.Set(*pmt);
	m_videoMediaType.Set(*pmt);

	//if (!(pmt->subtype == MEDIASUBTYPE_RGB24 || pmt->subtype == MEDIASUBTYPE_RGB32))
	//	return VFW_E_TYPE_NOT_ACCEPTED;
	 
    // Either all the downstream pins have accepted or there are none.
    m_bInsideCheckMediaType = FALSE;
    return NOERROR;

} // CheckMediaType


//
// SetMediaType
//
HRESULT CVANCSplitterInputPin::SetMediaType(const CMediaType *pmt)
{
	FilterTrace("CVANCSplitterInputPin::SetMediaType()\n");

    CAutoLock lock_it(m_pLock);
    HRESULT hr = NOERROR;

    // Make sure that the base class likes it
    if (FAILED(hr = CBaseInputPin::SetMediaType(pmt)))
        return hr;
  	  
    ASSERT(m_Connected != NULL);
    return NOERROR;

} // SetMediaType


//
// BreakConnect
//
HRESULT CVANCSplitterInputPin::BreakConnect()
{
	FilterTrace("CVANCSplitterInputPin::BreakConnect()\n");

	HRESULT hr;

	// Make sure that the base class likes it
    if (FAILED(hr = CBaseInputPin::BreakConnect()))
        return hr;
	  
    // Release any allocator that we are holding
    if (m_pTee != NULL && m_pTee->m_pAllocator)
    {
        m_pTee->m_pAllocator->Release();
        m_pTee->m_pAllocator = NULL;
    }
	
    // Release any allocator that we are holding
    if (m_pTee != NULL && m_pTee->m_pAllocator)
    {
        m_pTee->m_pAllocator2->Release();
        m_pTee->m_pAllocator2 = NULL;
    }

	EndReceiveThread();

    return NOERROR;
} // BreakConnect


//
// NotifyAllocator
//
STDMETHODIMP
CVANCSplitterInputPin::NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly)
{
	FilterTrace("CVANCSplitterInputPin::NotifyAllocator()\n");
	HRESULT hr = NOERROR;

    CheckPointer(pAllocator,E_FAIL);
    CAutoLock lock_it(m_pLock);
	 
    // Free the old allocator if any
    if (m_pTee->m_pAllocator)
        m_pTee->m_pAllocator->Release();
	 
    // Free the old allocator if any
    if (m_pTee->m_pAllocator2)
        m_pTee->m_pAllocator2->Release();

	// Create the primary allocator for video
	CreateMemoryAllocator(&m_pTee->m_pAllocator);
	
	// Create the allocator for captioning
	CreateMemoryAllocator(&m_pTee->m_pAllocator2);

	// Initialize the properties for the captioning
	ALLOCATOR_PROPERTIES propRequest, propResults;
	propRequest.cbAlign = 1;
	propRequest.cbBuffer = 2;
	propRequest.cbPrefix = 0;
	propRequest.cBuffers = 100;
	m_pTee->m_pAllocator2->SetProperties(&propRequest, &propResults);
	m_pTee->m_pAllocator2->Commit();

	// Get the current allocator properties for video
	ALLOCATOR_PROPERTIES props, result;
	pAllocator->GetProperties(&props);
	
	// Allocate 92MB of buffers 
	long nBuffers = (92160000 / props.cbBuffer);
	props.cBuffers = nBuffers;

	m_pTee->m_pAllocator->SetProperties(&props, &result);
	m_pTee->m_pAllocator->Commit();
	 
	pAllocator->SetProperties(&props, &result);
	pAllocator->Commit();

    // Notify the base class about the allocator
    return CBaseInputPin::NotifyAllocator(m_pTee->m_pAllocator,bReadOnly);

} // NotifyAllocator


//
// EndOfStream
//
HRESULT CVANCSplitterInputPin::EndOfStream()
{
	FilterTrace("CVANCSplitterInputPin::EndOfStream()\n");
	
	EndReceiveThread();

	return CBaseInputPin::EndOfStream();
} // EndOfStream


//
// BeginFlush
//
HRESULT CVANCSplitterInputPin::BeginFlush()
{
	FilterTrace("CVANCSplitterInputPin::BeginFlush()\n");

    return CBaseInputPin::BeginFlush();
} // BeginFlush


//
// EndFlush
//
HRESULT CVANCSplitterInputPin::EndFlush()
{
	FilterTrace("CVANCSplitterInputPin::EndFlush()\n");

	EndReceiveThread();
	 
    return CBaseInputPin::EndOfStream();

} // EndFlush

HRESULT CVANCSplitterInputPin::EndReceiveThread()
{
	// Release the VANC buffer 
	if (m_pVANCData != NULL)
		free (m_pVANCData);
	 
	m_pVANCData = NULL;

	DWORD dwExitCode = 0;
	m_bRunning = FALSE;

	FilterTrace("CVANCSplitterInputPin::EndFlush() ==> Closing _Receive Thread \n");

	// Check if the receive buffer is running and wait fot it to close or shut it down
	if (m_hReceiveThread != NULL && !GetExitCodeThread(m_hReceiveThread, &dwExitCode))
		if (m_hReceiveThread != NULL && GetLastError() == STILL_ACTIVE)
			if (WaitForSingleObject(m_hReceiveThread, 30000) != WAIT_OBJECT_0)
				TerminateThread(m_hReceiveThread, 0);
  
	CMediaSampleX *pSample = NULL;

	// Delete any pending samples not processed. 
	while (_sampleBuffer.size() > 0)
	{
		CMediaSampleX* pSample = _sampleBuffer.front();

		if (pSample != NULL)
			delete pSample;

		_sampleBuffer.pop();
	}

	m_hReceiveThread = NULL;
	_sampleBuffer.empty();

	m_nPacketStartPos = -1;
	return S_OK;
}

//
// NewSegment
//
                    
HRESULT CVANCSplitterInputPin::NewSegment(REFERENCE_TIME tStart,
                                 REFERENCE_TIME tStop,
                                 double dRate)
{
	FilterTrace("CVANCSplitterInputPin::NewSegment()\n");
    return CBaseInputPin::NewSegment(tStart, tStop, dRate);
} // NewSegment

 
//
// Receive
//
HRESULT CVANCSplitterInputPin::Receive(IMediaSample *pSample)
{
	HRESULT hr = NOERROR;

    ASSERT(pSample);
   	 
	// Receive the media sample
    if (FAILED(hr = CBaseInputPin::Receive(pSample)))
		return hr;
  
	DeliverSample(pSample);  
  
    return NOERROR;

} // Receive
 
HRESULT CVANCSplitterInputPin::DeliverSample(IMediaSample *pSample)
{
	BYTE line21Pair[2] = { 0, 0 };
	BYTE* pBuffer;
	REFERENCE_TIME timeStart, timeEnd;
	HRESULT hr = NOERROR;
	bool bVANCValid = false;

	// If the entry does not have a pointer then skip it 
	if (FAILED(hr = pSample->GetPointer(&pBuffer)))
		return S_OK;
     
	long cbVANCData = m_bih.biWidth * sizeof(__int16);

	// Allocate storage for VANC frame data
	if (m_pVANCData == NULL)
		m_pVANCData = (__int16*)malloc(cbVANCData);

	m_dwVANCLineOffset = m_dwBytesPerLine * m_pTee->m_nVANCLine;

	// Clear out the buffer
	memset(m_pVANCData, 0, cbVANCData);

	// Get a pointer to the v210 VANC data in the frame data
	__int32* v210Buffer = (__int32*)(pBuffer + m_dwVANCLineOffset);
  
	// Get VANC from v210 frame
	Convert_v210_to_BYTES(v210Buffer, m_dwBytesPerLine, m_pVANCData);
	 
	// Test if the vanc line is valid DTVCC line
	bVANCValid = m_vancParser.IsValidDTVCCPacket(m_pVANCData, cbVANCData / 2);

	// If the selected line is not a valid VANC line then find one 
	if (!bVANCValid)
	{
		FilterTrace("CVANCSplitterInputPin::DeliverSample() - **LINE AUTO DETECTION** [ START ] \n");

		for (int i = 0; i < 30; i++)
		{ 
			// Get a pointer to the frame line 
			v210Buffer = (__int32*)(pBuffer + (m_dwBytesPerLine * i));
			
			// Erase the current VANC packet
			memset(m_pVANCData, 0, m_bih.biWidth * sizeof(__int16));

			// Conver the v210 to a byte array of VANC data 
			Convert_v210_to_BYTES(v210Buffer, m_dwBytesPerLine, m_pVANCData);
			
			if (::IsLogging())
			{
				char buffer[1000];
				memset(buffer, 0, 1000);

				for(int j = 0; j < 200; j++)
					sprintf(&buffer[j * 4], "%03x ", m_pVANCData[j]);

				FilterTrace("LINE %02i > %s \n", i + 1, buffer);
			}
			
			// Test if the vanc line is valid
			bVANCValid = m_vancParser.IsValidDTVCCPacket(m_pVANCData, cbVANCData / 2);

			// If the line contains a VANC marker and contains DTVCC then update the VANC line entry
			if (bVANCValid)
			{
				// Store the VANC line for the next sample and move forward
				m_pTee->m_nVANCLine = i;
				FilterTrace("CVANCSplitterInputPin::DeliverSample() - **LINE %i DETECTED** \n", i + 1);
				break;
			}
		}

		FilterTrace("CVANCSplitterInputPin::DeliverSample() - **LINE AUTO DETECTION** [ COMPLETE ] \n");
	}

	if (::IsLogging())
	{
		char buffer[1000];
		memset(buffer, 0, 1000);
		 
		for(int i = 0; i < 200; i++)
			sprintf(&buffer[i * 4], "%03x ", m_pVANCData[i]);

		FilterTrace("%s \n", buffer);
	}

	// Get the line21 output pin
	CVANCSplitterOutputPin *pVidPin = m_pTee->GetPinNFromList(0);
 
	REFERENCE_TIME rtStart;
	REFERENCE_TIME rtEnd;
	REFERENCE_TIME tStart;
	REFERENCE_TIME tEnd;

	pSample->GetMediaTime(&rtStart, &rtEnd);
	pSample->GetTime(&tStart, &tEnd);
  
	pSample->SetMediaType(&m_pTee->GetPinNFromList(0)->m_mt);

	BYTE* pNewBuffer;
	pSample->GetPointer(&pNewBuffer);
	pSample->SetActualDataLength(pSample->GetActualDataLength() - m_dtvccStart);

	memcpy(pNewBuffer, pBuffer + m_dtvccStart, pSample->GetActualDataLength());
    
	pSample->AddRef();
	m_pTee->GetPinNFromList(0)->Deliver(pSample);

	// If the VANC pack is valid ....
	if (bVANCValid)
	{   
		if (m_pTee->GetPinNFromList(1)->IsConnected())
		{
			// if DTVCC packet start is undefined or the existing packet is not a DTVCC packet then find it
			if (m_nPacketStartPos < 0 || !m_vancParser.IsValidDTVCCPacketPos(m_pVANCData + m_nPacketStartPos))
				m_nPacketStartPos = m_vancParser.GetDTVCCPacketPos(m_pVANCData, m_dwBytesPerLine);

			// If the packet exists then process and deliver
			if (m_nPacketStartPos > -1)
			{
				bool bPacketValid = false;

				try
				{
					// Parse the VANC data
					m_vancParser.Parse(m_pVANCData + m_nPacketStartPos);

					// Get th 608 packet byte-pair from the VANC packet
					bPacketValid = m_vancParser.Get608Packet(line21Pair, m_pTee->GetPacketType());
				}
				catch (...)
				{
					FilterTrace("CVANCSplitterInputPin::DeliverSample() - **CRITICAL** failure while parsing packet\n");
					// critical failure. 
				}

				// If the default valid packet type is CC1 and no data then try CC2. This is a hack since 
				// we normally will expect captioning data on CC1 for the primary caption signal. We check
				// for 0x80 (128) 0x80 (128) which is no data. 
				//if (bPacketValid && m_pTee->GetPacketType() == NTSC_CC1 && line21Pair[0] == 0x80 && line21Pair[1] == 0x80)
				//	bPacketValid = m_vancParser.Get608Packet(line21Pair, NTSC_CC2);

				// Get th 608 packet byte-pair from the VANC packet
				if (bPacketValid)
				{
					// Get the line21 output pin
					CVANCSplitterOutputPin *pCCPin = m_pTee->GetPinNFromList(1);

					// Create a new media sample
					CComPtr<IMediaSample> pOutSample;

					// Get a new delivery buffer (blocks until one available)
					hr = pCCPin->GetDeliveryBuffer( &pOutSample, &timeStart, &timeEnd, 0);

					if (SUCCEEDED(hr))
					{
						pOutSample->GetPointer(&pBuffer);
						memcpy(pBuffer, line21Pair, 2);
						pOutSample->SetActualDataLength(2);
						pOutSample->SetMediaTime(&rtStart, &rtEnd);
						pOutSample->SetTime(&tStart, &tEnd);
						pCCPin->Deliver(pOutSample);
					}
				}	
			}
		}
	}
	  
	pSample->Release();
	return S_OK;
}

//
// Completed a connection to a pin
//
HRESULT CVANCSplitterInputPin::CompleteConnect(IPin *pReceivePin)
{
	FilterTrace("CVANCSplitterInputPin::CompleteConnect()\n");

    ASSERT(pReceivePin);
    HRESULT hr;

    if (FAILED(hr = CBaseInputPin::CompleteConnect(pReceivePin)))
        return hr;
 
	if (this->IsConnected())
	{
		m_vancParser.SetTrace(NULL);

		// Initialize logger (create .packets file)
		if (m_pTee->GetLogFileName()[0] != NULL)
		{
			TCHAR szPath[MAX_PATH];
			memset(szPath, 0, sizeof(szPath));
			_tcscpy_s<MAX_PATH>(szPath, m_pTee->GetLogFileName());
			_tcscat_s<MAX_PATH>(szPath, L".packets");
			m_vancParser.SetTrace(szPath);
		}
 
		// Get the first output pin
		POSITION pos = m_pTee->m_OutputPinsList.GetHeadPosition();
		CVANCSplitterOutputPin *pOutputPin = m_pTee->m_OutputPinsList.GetNext(pos);

		// m_videoMediaType.Set(m_mt);
		memcpy(m_videoMediaType.pbFormat, m_mt.pbFormat, m_mt.cbFormat);
		 
		// Get the total bytes per line
		m_dwBytesPerLine = (m_bih.biSizeImage / m_bih.biHeight);  
		  
		// Get the VANC offset
		m_dwVANCLineOffset = m_dwBytesPerLine * m_pTee->m_nVANCLine;
  
		// Get the start position of video (after the VANC)
		m_dtvccStart = (m_dwBytesPerLine * VIDEO_VANC_LINES);
		
		// Calc the new length of the video
		m_dwFrameLength = m_bih.biSizeImage - m_dtvccStart;

		// Remove any existing VANC buffer
		if (m_pVANCData != NULL)
			free (m_pVANCData);
		 
		if (m_videoMediaType.formattype == FORMAT_VideoInfo2)
		{ 
			// Adjust the target output video size to exclude the VANC content
			VIDEOINFOHEADER2* pVIH = (VIDEOINFOHEADER2*)m_videoMediaType.pbFormat;
			pVIH->bmiHeader.biHeight = m_bih.biHeight - VIDEO_VANC_LINES;
			pVIH->bmiHeader.biSizeImage = m_dwBytesPerLine * (m_bih.biHeight - VIDEO_VANC_LINES);
				
		}
		else if (m_videoMediaType.formattype == FORMAT_VideoInfo)
		{
			// Adjust the target output video size to exclude the VANC content
			VIDEOINFOHEADER* pVIH = (VIDEOINFOHEADER*)m_videoMediaType.pbFormat;
			pVIH->bmiHeader.biHeight = m_bih.biHeight - VIDEO_VANC_LINES;
			pVIH->bmiHeader.biSizeImage = m_dwBytesPerLine * (m_bih.biHeight - VIDEO_VANC_LINES);
		}

		pOutputPin->SetMediaType(&m_videoMediaType);
   
		// Reconnect the connected output pin
		if(pOutputPin != NULL)
		{
			// Check with downstream pin
			if(pOutputPin->m_Connected != NULL)
			{
				if(m_mt != pOutputPin->m_mt)
					m_pTee->ReconnectPin(pOutputPin, &m_mt);
			}
		}
		else
		{
			// We should have as many pins as the count says we have
			ASSERT(FALSE);
			FilterTrace("CVANCSplitterInputPin::_Receive() (Line21 pin not connected) \n");
		}
	}
	  
    return S_OK;
}
 

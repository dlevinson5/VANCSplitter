////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VANCSplitter.h"
#include "VANCSplitterPropertyPage.h"

#ifdef DEBUG
//
// DisplayMediaType -- (DEBUG ONLY)
//
void DisplayMediaType(TCHAR *pDescription, const CMediaType *pmt)
{
    ASSERT(pmt);
    if (!pmt)
        return; 
        
    // Dump the GUID types and a short description

    DbgLog((LOG_TRACE,2,TEXT("")));
    DbgLog((LOG_TRACE,2,TEXT("%s"),pDescription));
    DbgLog((LOG_TRACE,2,TEXT("")));
    DbgLog((LOG_TRACE,2,TEXT("Media Type Description")));
    DbgLog((LOG_TRACE,2,TEXT("Major type %s"),GuidNames[*pmt->Type()]));
    DbgLog((LOG_TRACE,2,TEXT("Subtype %s"),GuidNames[*pmt->Subtype()]));
    DbgLog((LOG_TRACE,2,TEXT("Subtype description %s"),GetSubtypeName(pmt->Subtype())));
    DbgLog((LOG_TRACE,2,TEXT("Format size %d"),pmt->cbFormat));

    // Dump the generic media types */

    DbgLog((LOG_TRACE,2,TEXT("Fixed size sample %d"),pmt->IsFixedSize()));
    DbgLog((LOG_TRACE,2,TEXT("Temporal compression %d"),pmt->IsTemporalCompressed()));
    DbgLog((LOG_TRACE,2,TEXT("Sample size %d"),pmt->GetSampleSize()));


} // DisplayMediaType
#endif

//
// CreateInstance
//
// Creator function for the class ID
//
CUnknown * WINAPI CVANCSplitter::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
    return new CVANCSplitter(NAME("VANC Splitter"), pUnk, phr);
}
 

//
// Constructor
//
CVANCSplitter::CVANCSplitter(TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr) :
    m_OutputPinsList(NAME("VANCSplitter Output Pins list")),
	m_InputPinsList(NAME("VANCSplitter Input Pins list")),
    m_lCanSeek(TRUE),
    m_pAllocator(NULL),
    m_NumOutputPins(0),
	m_NumInputPins(0),
    m_NextOutputPinNumber(0),
	m_NextInputPinNumber(0),
	m_nVANCLine(8),
	m_nPacketType(0),
	m_pAllocator2(NULL),
	CBaseFilter(NAME("VANC Splitter"), pUnk, this, CLSID_VANCSplitter)
{
	// Initialize log pointer (no logging)
	m_szLogFilePath[0] = NULL;

    ASSERT(phr);
	 
	InitInputPinsList();

    // Create a single output pin at this time
    InitOutputPinsList();

	// Create primary input pin 
	CVANCSplitterInputPin *pInputPin = CreateNextInputPin(this);

    if (pInputPin != NULL )
    {
        m_NumInputPins++;
        m_InputPinsList.AddTail(pInputPin);
    }

	// Create primary video output pin 
    CVANCSplitterOutputPin *pOutputPin1 = CreateNextOutputPin(this);

    if (pOutputPin1 != NULL )
    {
        m_NumOutputPins++;
        m_OutputPinsList.AddTail(pOutputPin1);
    }

	// Create primary caption output pin 
    CVANCSplitterOutputPin *pOutputPin2 = CreateNextOutputPin(this);
	 
    if (pOutputPin2 != NULL )
    {
		// Set the media type for the second line21 pin
		CMediaType mediaType;
		mediaType.SetType(&MEDIATYPE_AUXLine21Data);
		mediaType.SetSubtype(&MEDIASUBTYPE_Line21_BytePair);
		mediaType.SetFormatType(&FORMAT_None);
		mediaType.pbFormat = NULL;
		mediaType.cbFormat = 0;
		mediaType.lSampleSize = 2;
		mediaType.bFixedSizeSamples = TRUE;
		pOutputPin2->SetMediaType(&mediaType);
        
		m_NumOutputPins++;
        m_OutputPinsList.AddTail(pOutputPin2);
    }
}


//
// Destructor
//
CVANCSplitter::~CVANCSplitter()
{
	FilterTrace("CVANCSplitter::~CVANCSplitter()\n");
	 
    InitOutputPinsList();
	InitInputPinsList();

	if (m_pAllocator != NULL)
		m_pAllocator->Release();

	if (m_pAllocator2 != NULL)
		m_pAllocator2->Release();

	m_pAllocator = NULL;
	m_pAllocator2 = NULL;
}


//
// GetPinCount
//
int CVANCSplitter::GetPinCount()
{
    return (m_NumOutputPins + m_NumInputPins);
}


//
// GetPin
//
CBasePin *CVANCSplitter::GetPin(int n)
{
    if (n < 0)
        return NULL ;

    // Pin zero is the one and only input pin
	if (n < this->m_NumInputPins)
		return GetPinNFromInList(n);

    // return the output pin at position(n - 1) (zero based)
    return GetPinNFromList((n - this->m_NumInputPins));
}


//
// InitOutputPinsList
//
void CVANCSplitter::InitOutputPinsList()
{
    POSITION pos = m_OutputPinsList.GetHeadPosition();

    while(pos)
    {
        CVANCSplitterOutputPin *pOutputPin = m_OutputPinsList.GetNext(pos);
        ASSERT(pOutputPin->m_pOutputQueue == NULL);
        delete pOutputPin; // ->Release();
    }

    m_NumOutputPins = 0;
    m_OutputPinsList.RemoveAll();

} // InitOutputPinsList

//
// InitOutputPinsList
//
void CVANCSplitter::InitInputPinsList()
{
    POSITION pos = m_InputPinsList.GetHeadPosition();

    while(pos)
    { 
        CVANCSplitterInputPin *pInputPin = m_InputPinsList.GetNext(pos);
        delete pInputPin; // ->Release();
    }

    m_NumInputPins = 0;
    m_InputPinsList.RemoveAll();

} // InitOutputPinsList

//
// CreateNextOutputPin
//
CVANCSplitterOutputPin *CVANCSplitter::CreateNextOutputPin(CVANCSplitter *pTee)
{
    WCHAR szbuf[20];             // Temporary scratch buffer
    m_NextOutputPinNumber++;     // Next number to use for pin
    HRESULT hr = NOERROR;

    _stprintf_s(szbuf, NUMELMS(szbuf), L"Output%d\0", m_NextOutputPinNumber);

    CVANCSplitterOutputPin *pPin = new CVANCSplitterOutputPin(NAME("VANCSplitter Output"), pTee,
                                            &hr, szbuf,
                                            m_NextOutputPinNumber);

    if (FAILED(hr) || pPin == NULL) 
    {
        delete pPin;
        return NULL;
    }

    // pPin->AddRef();
    return pPin;

} // CreateNextOutputPin

//
// CreateNextOutputPin
//
CVANCSplitterInputPin *CVANCSplitter::CreateNextInputPin(CVANCSplitter *pTee)
{
    WCHAR szbuf[20];             // Temporary scratch buffer
    m_NextInputPinNumber++;     // Next number to use for pin
    HRESULT hr = NOERROR;

    _stprintf_s(szbuf, NUMELMS(szbuf), L"Input%d\0", m_NextInputPinNumber);

    CVANCSplitterInputPin *pPin = new CVANCSplitterInputPin(NAME("VANCSplitter Input"), pTee,
                                            &hr, szbuf,
                                            m_NextInputPinNumber);

    if (FAILED(hr) || pPin == NULL) 
    {
        delete pPin;
        return NULL;
    }

    // pPin->AddRef();
    return pPin;

} // CreateNextOutputPin

//
// DeleteOutputPin
//
void CVANCSplitter::DeleteOutputPin(CVANCSplitterOutputPin *pPin)
{
    ASSERT(pPin);
    if (!pPin)
        return;
        
    POSITION pos = m_OutputPinsList.GetHeadPosition();

    while(pos) 
    {
        POSITION posold = pos;         // Remember this position
        CVANCSplitterOutputPin *pOutputPin = m_OutputPinsList.GetNext(pos);

        if (pOutputPin == pPin) 
        {
			pOutputPin->BreakConnect();

            // If this pin holds the seek interface release it
            if (pPin->m_bHoldsSeek) 
            {
                InterlockedExchange(&m_lCanSeek, FALSE);
                pPin->m_bHoldsSeek = FALSE;
                pPin->m_pPosition->Release();
            }

            m_OutputPinsList.Remove(posold);
            ASSERT(pOutputPin->m_pOutputQueue == NULL);
            delete pPin;

            m_NumOutputPins--;
            IncrementPinVersion();
            break;
        }
    }

} // DeleteOutputPin

//
// DeleteInputPin
//
void CVANCSplitter::DeleteInputPin(CVANCSplitterInputPin *pPin)
{
    ASSERT(pPin);
    if (!pPin)
        return;
        
    POSITION pos = m_InputPinsList.GetHeadPosition();

    while(pos) 
    {
        POSITION posold = pos;          
        CVANCSplitterInputPin *pInputPin = m_InputPinsList.GetNext(pos);

        if (pInputPin == pPin) 
        {
			pInputPin->BreakConnect();
            m_InputPinsList.Remove(posold);
            delete pPin;

            m_NumInputPins--;
            IncrementPinVersion();
            break;
        }
    }

} // DeleteInputPin

//
// GetNumFreePins
//
int CVANCSplitter::GetNumFreePins()
{
    int n = 0;
    POSITION pos = m_OutputPinsList.GetHeadPosition();

    while(pos) 
    {
        CVANCSplitterOutputPin *pOutputPin = m_OutputPinsList.GetNext(pos);

        if (pOutputPin && pOutputPin->m_Connected == NULL)
            n++;
    }

    return n;

} // GetNumFreePins

//
// Stop
//
// Overriden to handle no input connections
//
STDMETHODIMP CVANCSplitter::Stop()
{
    HRESULT hr;

	if (FAILED(hr = CBaseFilter::Stop()))
		return hr;

    m_State = State_Stopped;

	// Trigger end of stream on input pin
	if (FAILED(hr = GetPin(0)->EndOfStream()))
		return hr;

	// Force end flush on video pin
	if (FAILED(hr = this->GetPinNFromList(0)->DeliverEndFlush()))
		return hr;

	// Force end flush on caption pin
	if (FAILED(hr = this->GetPinNFromList(1)->DeliverEndFlush()))
		return hr;

    return NOERROR;
}




//
// Pause
//
// Overriden to handle no input connections
//
STDMETHODIMP CVANCSplitter::Pause()
{
	FilterTrace("CVANCSplitter::Pause()\n");

	HRESULT hr;

    CAutoLock cObjectLock(m_pLock);
    
	if (FAILED(hr = CBaseFilter::Pause()))
		return hr;

	if (GetPin(0)->IsConnected() == FALSE) 
		if (FAILED(hr = GetPin(0)->EndOfStream()))
			return hr;
	  
    return hr;
}


//
// Run
//
// Overriden to handle no input connections
//
STDMETHODIMP CVANCSplitter::Run(REFERENCE_TIME tStart)
{
	FilterTrace("CVANCSplitter::Run()\n");

    CAutoLock cObjectLock(m_pLock);
    HRESULT hr = CBaseFilter::Run(tStart);

	if (GetPin(0)->IsConnected() == FALSE) 
        GetPin(0)->EndOfStream();

    return hr;
}

// 
// NonDelegatingQueryInterface
//
STDMETHODIMP CVANCSplitter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	if (riid == IID_IVANCSplitter) 
		return GetInterface((IVANCSplitter*) this, ppv);
	else if (riid == IID_ISpecifyPropertyPages) 
		return GetInterface((ISpecifyPropertyPages *) this, ppv);

	return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}
  

//
// GetPages
//
STDMETHODIMP CVANCSplitter::GetPages(CAUUID *pPages)
{
	CheckPointer(pPages,E_POINTER);

	pPages->cElems = 1;
	pPages->pElems = (GUID *) CoTaskMemAlloc(sizeof(GUID));
	
	if (pPages->pElems == NULL)
		return E_OUTOFMEMORY;

	pPages->pElems[0] = CLSID_VANCSplitterPropertyPage;

	return NOERROR;
} 
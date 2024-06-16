////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"
#include "global.h"
#include <stdio.h>
#include "VANCParser.h"
#include "608CaptionParser.h"
#include <vector>
#include <queue>
#include "MediaSampleX.h"

class CVANCSplitter;
class CVANCSplitterOutputPin;
 
class CVANCSplitterInputPin : public CBaseInputPin
{
    friend class CVANCSplitterOutputPin;
	int m_nPinNumber;
    CVANCSplitter *m_pTee;                  // Main filter object
    BOOL m_bInsideCheckMediaType;  // Re-entrancy control
	BITMAPINFOHEADER m_bih;
	CMediaType m_connectedType;
	VANCParser m_vancParser;
	CMediaType m_videoMediaType;
	C608CaptionParser m_608Parser;
	__int16* m_pVANCData;
	DWORD m_dwBytesPerLine;
	DWORD m_dwVANCLineOffset;
	__int32 m_dtvccStart;
	DWORD m_dwFrameLength;
	HANDLE m_hReceiveThread;
	queue<CMediaSampleX*> _sampleBuffer;
	BOOL m_bRunning;
	long m_nPacketStartPos;

public:

    // Constructor and destructor
    CVANCSplitterInputPin(TCHAR *pObjName,
                 CVANCSplitter *pTee,
                 HRESULT *phr,
                 LPCWSTR pPinName,
				 int PinNumber);

    ~CVANCSplitterInputPin();

    // Used to check the input pin connection
    HRESULT CheckMediaType(const CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT BreakConnect();

    // Reconnect outputs if necessary at end of completion
    virtual HRESULT CompleteConnect(IPin *pReceivePin);

    STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);

    // Pass through calls downstream
    STDMETHODIMP EndOfStream();
    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
    STDMETHODIMP NewSegment(
                    REFERENCE_TIME tStart,
                    REFERENCE_TIME tStop,
                    double dRate);

    // Handles the next block of data from the stream
    STDMETHODIMP Receive(IMediaSample *pSample);
	
	// Handles receive background operations
	HRESULT DeliverSample(IMediaSample *pSample);
	HRESULT EndReceiveThread();
};

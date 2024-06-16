////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"
#include "global.h"

class CVANCSplitter;
class CVANCSplitterInputPin;

// Class for the Tee filter's Output pins.
class CVANCSplitterOutputPin : public CBaseOutputPin
{
    friend class CVANCSplitterInputPin;
    friend class CVANCSplitter;

    CVANCSplitter *m_pTee;                  // Main filter object pointer
    IUnknown *m_pPosition;      // Pass seek calls upstream
    BOOL m_bHoldsSeek;             // Is this the one seekable stream

    COutputQueue *m_pOutputQueue;  // Streams data to the peer pin
    BOOL m_bInsideCheckMediaType;  // Re-entrancy control
    LONG m_cOurRef;                // We maintain reference counting
 
public:

    // Constructor and destructor

    CVANCSplitterOutputPin(TCHAR *pObjName,
                   CVANCSplitter *pTee,
                   HRESULT *phr,
                   LPCWSTR pPinName,
                   INT PinNumber);

#ifdef DEBUG
	~CVANCSplitterOutputPin();
#endif

    // Override to expose IMediaPosition
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppvoid);

    // Override since the life time of pins and filters are not the same
    STDMETHODIMP_(ULONG) NonDelegatingAddRef1();
    STDMETHODIMP_(ULONG) NonDelegatingRelease1();

    // Override to enumerate media types
    // STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);
	 
    // Check that we can support an output type
    HRESULT CheckMediaType(const CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);

    // Negotiation to use our input pins allocator
    HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);
    HRESULT DecideBufferSize(IMemAllocator *pMemAllocator,
                             ALLOCATOR_PROPERTIES * ppropInputRequest);

    // Used to create output queue objects
    HRESULT Active();
    HRESULT Inactive();

    // Overriden to create and destroy output pins
    HRESULT CompleteConnect(IPin *pReceivePin);

    // Overriden to pass data to the output queues
    HRESULT Deliver(IMediaSample *pMediaSample);
    HRESULT DeliverEndOfStream();
    HRESULT DeliverBeginFlush();
    HRESULT DeliverEndFlush();
    HRESULT DeliverNewSegment(
                    REFERENCE_TIME tStart,
                    REFERENCE_TIME tStop,
                    double dRate);


    // Overriden to handle quality messages
    STDMETHODIMP Notify(IBaseFilter *pSender, Quality q);
	HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
};

////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VANCSplitterOutputPin.h"
#include "VANCSplitterOutputPin.h"

#include "VANCSplitter.h"
#include "VANCSplitterOutputPin.h"

//
// GetPinNFromList
//
CVANCSplitterInputPin *CVANCSplitter::GetPinNFromInList(int n)
{
    // Validate the position being asked for
    if (n >= m_NumInputPins)
        return NULL;

    // Get the head of the list
    POSITION pos = m_InputPinsList.GetHeadPosition();

    n++;       // Make the number 1 based

    CVANCSplitterInputPin *pInputPin=0;
    while(n) 
    {
        pInputPin = m_InputPinsList.GetNext(pos);
        n--;
    }

    return pInputPin;

} // GetPinNFromList

//
// GetPinNFromList
//
CVANCSplitterOutputPin *CVANCSplitter::GetPinNFromList(int n)
{
    // Validate the position being asked for
    if (n >= m_NumOutputPins)
        return NULL;

    // Get the head of the list
    POSITION pos = m_OutputPinsList.GetHeadPosition();

    n++;       // Make the number 1 based

    CVANCSplitterOutputPin *pOutputPin=0;
    while(n) 
    {
        pOutputPin = m_OutputPinsList.GetNext(pos);
        n--;
    }

    return pOutputPin;

} // GetPinNFromList

//
// CVANCSplitterOutputPin constructor
//
CVANCSplitterOutputPin::CVANCSplitterOutputPin(TCHAR *pName,
                             CVANCSplitter *pTee,
                             HRESULT *phr,
                             LPCWSTR pPinName,
                             int PinNumber) :
    CBaseOutputPin(pName, pTee, pTee, phr, pPinName) ,
    m_pOutputQueue(NULL),
    m_bHoldsSeek(FALSE),
    m_pPosition(NULL),
    m_pTee(pTee),
    m_cOurRef(0),
    m_bInsideCheckMediaType(FALSE)
{
	  
    ASSERT(pTee);
}



#ifdef DEBUG
//
// CVANCSplitterOutputPin destructor
//
CVANCSplitterOutputPin::~CVANCSplitterOutputPin()
{
	FilterTrace("CVANCSplitterOutputPin::CVANCSplitterOutputPin()\n");
    ASSERT(m_pOutputQueue == NULL);
}
#endif


//
// NonDelegatingQueryInterface
//
// This function is overwritten to expose IMediaPosition and IMediaSelection
// Note that only one output stream can be allowed to expose this to avoid
// conflicts, the other pins will just return E_NOINTERFACE and therefore
// appear as non seekable streams. We have a LONG value that if exchanged to
// produce a TRUE means that we have the honor. If it exchanges to FALSE then
// someone is already in. If we do get it and error occurs then we reset it
// to TRUE so someone else can get it.
//
STDMETHODIMP CVANCSplitterOutputPin::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    CheckPointer(ppv,E_POINTER);
    ASSERT(ppv);

    *ppv = NULL;
    HRESULT hr = NOERROR;

	if (riid == IID_IAMStreamConfig) 
		return GetInterface((IAMStreamConfig *) this, ppv);

    // See what interface the caller is interested in.
    if(riid == IID_IMediaPosition || riid == IID_IMediaSeeking)
    {
        if(m_pPosition)
        {
            if(m_bHoldsSeek == FALSE)
                return E_NOINTERFACE;
            return m_pPosition->QueryInterface(riid, ppv);
        }
    }
    else
    {
        return CBaseOutputPin::NonDelegatingQueryInterface(riid, ppv);
    }

    CAutoLock lock_it(m_pLock);
    ASSERT(m_pPosition == NULL);
    IUnknown *pMediaPosition = NULL;

    // Try to create a seeking implementation
    if(InterlockedExchange(&m_pTee->m_lCanSeek, FALSE) == FALSE)
        return E_NOINTERFACE;

    // Create implementation of this dynamically as sometimes we may never
    // try and seek. The helper object implements IMediaPosition and also
    // the IMediaSelection control interface and simply takes the calls
    // normally from the downstream filter and passes them upstream

    hr = CreatePosPassThru(GetOwner(),
                           FALSE,
						   (IPin *)m_pTee->GetPin(0),
                           &pMediaPosition);

    if(pMediaPosition == NULL)
    {
        InterlockedExchange(&m_pTee->m_lCanSeek, TRUE);
        return E_OUTOFMEMORY;
    }

    if(FAILED(hr))
    {
        InterlockedExchange(&m_pTee->m_lCanSeek, TRUE);
        pMediaPosition->Release();
        return hr;
    }

    m_pPosition = pMediaPosition;
    m_bHoldsSeek = TRUE;
    return NonDelegatingQueryInterface(riid, ppv);

} // NonDelegatingQueryInterface


//
// NonDelegatingAddRef
//
// We need override this method so that we can do proper reference counting
// on our output pin. The base class CBasePin does not do any reference
// counting on the pin in RETAIL.
//
// Please refer to the comments for the NonDelegatingRelease method for more
// info on why we need to do this.
//
STDMETHODIMP_(ULONG) CVANCSplitterOutputPin::NonDelegatingAddRef1()
{
    CAutoLock lock_it(m_pLock);

#ifdef DEBUG
    // Update the debug only variable maintained by the base class
    m_cRef++;
    ASSERT(m_cRef > 0);
#endif

    // Now update our reference count
    m_cOurRef++;
    ASSERT(m_cOurRef > 0);
    return m_cOurRef;

} // NonDelegatingAddRef


//
// NonDelegatingRelease
//
// CVANCSplitterOutputPin overrides this class so that we can take the pin out of our
// output pins list and delete it when its reference count drops to 1 and there
// is atleast two free pins.
//
// Note that CreateNextOutputPin holds a reference count on the pin so that
// when the count drops to 1, we know that no one else has the pin.
//
// Moreover, the pin that we are about to delete must be a free pin(or else
// the reference would not have dropped to 1, and we must have atleast one
// other free pin(as the filter always wants to have one more free pin)
//
// Also, since CBasePin::NonDelegatingAddRef passes the call to the owning
// filter, we will have to call Release on the owning filter as well.
//
// Also, note that we maintain our own reference count m_cOurRef as the m_cRef
// variable maintained by CBasePin is debug only.
//
STDMETHODIMP_(ULONG) CVANCSplitterOutputPin::NonDelegatingRelease1()
{
    CAutoLock lock_it(m_pLock);
 
	#ifdef DEBUG
    // Update the debug only variable in CBasePin
    //m_cRef--;
    //ASSERT(m_cRef >= 0);
	#endif

    // Now update our reference count
    m_cOurRef--;
    ASSERT(m_cOurRef >= 0);
	  
	::CBaseOutputPin::NonDelegatingRelease();
		
	//if (m_cOurRef <= 0)
	//	m_pTee->DeleteOutputPin(this);

	return(ULONG) m_cOurRef;

} // NonDelegatingRelease


//
// DecideBufferSize
//
// This has to be present to override the PURE virtual class base function
//
HRESULT CVANCSplitterOutputPin::DecideBufferSize(IMemAllocator *pAlloc,ALLOCATOR_PROPERTIES *pProperties)
{
	CheckPointer(pAlloc,E_POINTER);   
    CheckPointer(pProperties,E_POINTER);   
      
	if (m_mt.majortype == MEDIATYPE_AUXLine21Data)
		m_pTee->m_pAllocator2->GetProperties(pProperties);
	else
		m_pTee->m_pAllocator->GetProperties(pProperties);
  	 
    HRESULT hr = NOERROR;   
    ASSERT(pProperties->cbBuffer);   
      
    ALLOCATOR_PROPERTIES Actual;   
   
    if (FAILED(hr = pAlloc->SetProperties(pProperties,&Actual)))
        return hr;   
     
    return NOERROR;      
} // DecideBufferSize


//
// DecideAllocator
//
HRESULT CVANCSplitterOutputPin::DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc)
{
	FilterTrace("CVANCSplitterOutputPin::DecideAllocator()\n");

    CheckPointer(pPin,E_POINTER);
    CheckPointer(ppAlloc,E_POINTER);
    ASSERT(m_pTee->m_pAllocator != NULL);
  
	// Tell the pin about our allocator, set by the input pin.
    HRESULT hr = NOERROR;

	IMemAllocator* pAllocator;

	if (m_mt.majortype == MEDIATYPE_AUXLine21Data)
		pAllocator = m_pTee->m_pAllocator2;
	else
		pAllocator = m_pTee->m_pAllocator;
  
	if (FAILED(hr = pPin->NotifyAllocator(pAllocator,TRUE)))
		return hr;

	// Return the allocator
	*ppAlloc = pAllocator;
	pAllocator->AddRef();
 
    return NOERROR;
} // DecideAllocator


//
// CheckMediaType
//
HRESULT CVANCSplitterOutputPin::CheckMediaType(const CMediaType *pmt)
{
	// FilterTrace("CVANCSplitterOutputPin::CheckMediaType()\n");

    CAutoLock lock_it(m_pLock);
	 
    // If we are already inside checkmedia type for this pin, return NOERROR
    // It is possble to hookup two of the tee filters and some other filter
    // like the video effects sample to get into this situation. If we
    // do not detect this, we will loop till we blow the stack

    if(m_bInsideCheckMediaType == TRUE)
        return NOERROR;

    m_bInsideCheckMediaType = TRUE;
    HRESULT hr = NOERROR;

#ifdef DEBUG
    // Display the type of the media for debugging purposes
    DisplayMediaType(TEXT("Output Pin Checking"), pmt);
#endif
	 
    m_bInsideCheckMediaType = FALSE;
    return NOERROR;

} // CheckMediaType

/*
//
// EnumMediaTypes
//
STDMETHODIMP CVANCSplitterOutputPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
    CAutoLock lock_it(m_pLock);
    ASSERT(ppEnum);

    // Make sure that we are connected
    if(((CVANCSplitterInputPin*)m_pTee->GetPin(0))->m_Connected == NULL)
        return VFW_E_NOT_CONNECTED;

    // We will simply return the enumerator of our input pin's peer
    return ((CVANCSplitterInputPin*)m_pTee->GetPin(0))->m_Connected->EnumMediaTypes(ppEnum);

} // EnumMediaTypes
*/


//
// SetMediaType
//
HRESULT CVANCSplitterOutputPin::SetMediaType(const CMediaType *pmt)
{
    CAutoLock lock_it(m_pLock);

#ifdef DEBUG
    // Display the format of the media for debugging purposes
    DisplayMediaType(TEXT("Output pin type agreed"), pmt);
#endif

    // Make sure that we have an input connected
	//if(((CVANCSplitterInputPin*)m_pTee->GetPin(0))->m_Connected == NULL)
    //    return VFW_E_NOT_CONNECTED;

    // Make sure that the base class likes it
    HRESULT hr = NOERROR;
    
	if (FAILED(hr = CBaseOutputPin::SetMediaType(pmt)))
        return hr;

    return NOERROR;

} // SetMediaType


//
// CompleteConnect
//
HRESULT CVANCSplitterOutputPin::CompleteConnect(IPin *pReceivePin)
{
	FilterTrace("CVANCSplitterOutputPin::CompleteConnect()\n");

    CAutoLock lock_it(m_pLock);
    ASSERT(m_Connected == pReceivePin);
    HRESULT hr = NOERROR;

    if(FAILED(hr = CBaseOutputPin::CompleteConnect(pReceivePin)))
        return hr;
	 
    return NOERROR;

} // CompleteConnect


//
// Active
//
// This is called when we start running or go paused. We create the
// output queue object to send data to our associated peer pin
//
HRESULT CVANCSplitterOutputPin::Active()
{
	FilterTrace("CVANCSplitterOutputPin::Active()\n");

    CAutoLock lock_it(m_pLock);
    HRESULT hr = NOERROR;

    // Make sure that the pin is connected
    if(m_Connected == NULL)
        return NOERROR;

    // Create the output queue if we have to
    if(m_pOutputQueue == NULL)
    {
        m_pOutputQueue = new COutputQueue(m_Connected, &hr, TRUE, FALSE);
        if(m_pOutputQueue == NULL)
            return E_OUTOFMEMORY;

        // Make sure that the constructor did not return any error
        if(FAILED(hr))
        {
            delete m_pOutputQueue;
            m_pOutputQueue = NULL;
            return hr;
        }
    }

    // Pass the call on to the base class
    CBaseOutputPin::Active();
    return NOERROR;

} // Active


//
// Inactive
//
// This is called when we stop streaming
// We delete the output queue at this time
//
HRESULT CVANCSplitterOutputPin::Inactive()
{
	FilterTrace("CVANCSplitterOutputPin::Inactive()\n");

    CAutoLock lock_it(m_pLock);
	  
    // Delete the output queus associated with the pin.
    if(m_pOutputQueue)
    {
        delete m_pOutputQueue;
        m_pOutputQueue = NULL;
    }

    CBaseOutputPin::Inactive();
    return NOERROR;

} // Inactive


//
// Deliver
//
HRESULT CVANCSplitterOutputPin::Deliver(IMediaSample *pMediaSample)
{
    CheckPointer(pMediaSample,E_POINTER);

    // Make sure that we have an output queue
    if(m_pOutputQueue == NULL)
        return NOERROR;
	  
	pMediaSample->AddRef();
    return m_pOutputQueue->Receive(pMediaSample);
} // Deliver


//
// DeliverEndOfStream
//
HRESULT CVANCSplitterOutputPin::DeliverEndOfStream()
{
	FilterTrace("CVANCSplitterOutputPin::DeliverEndOfStream()\n");

    // Make sure that we have an output queue
    if(m_pOutputQueue == NULL)
        return NOERROR;

    m_pOutputQueue->EOS();
    return NOERROR;

} // DeliverEndOfStream


//
// DeliverBeginFlush
//
HRESULT CVANCSplitterOutputPin::DeliverBeginFlush()
{
	FilterTrace("CVANCSplitterOutputPin::DeliverBeginFlush()\n");

    // Make sure that we have an output queue
    if(m_pOutputQueue == NULL)
        return NOERROR;

    m_pOutputQueue->BeginFlush();
    return NOERROR;

} // DeliverBeginFlush


//
// DeliverEndFlush
//
HRESULT CVANCSplitterOutputPin::DeliverEndFlush()
{
	FilterTrace("CVANCSplitterOutputPin::DeliverEndFlush()\n");

    // Make sure that we have an output queue
    if(m_pOutputQueue == NULL)
        return NOERROR;

    m_pOutputQueue->EndFlush();
    return NOERROR;

} // DeliverEndFlish

//
// DeliverNewSegment
//
HRESULT CVANCSplitterOutputPin::DeliverNewSegment(REFERENCE_TIME tStart, 
                                         REFERENCE_TIME tStop,  
                                         double dRate)
{
    // Make sure that we have an output queue
    if(m_pOutputQueue == NULL)
        return NOERROR;

    m_pOutputQueue->NewSegment(tStart, tStop, dRate);
    return NOERROR;

} // DeliverNewSegment


//
// Notify
//
STDMETHODIMP CVANCSplitterOutputPin::Notify(IBaseFilter *pSender, Quality q)
{
    // We pass the message on, which means that we find the quality sink
    // for our input pin and send it there
    POSITION pos = m_pTee->m_OutputPinsList.GetHeadPosition();
    CVANCSplitterOutputPin *pFirstOutput = m_pTee->m_OutputPinsList.GetNext(pos);

    if(this == pFirstOutput)
    {
        if(((CVANCSplitterInputPin*)m_pTee->GetPin(0))->m_pQSink!=NULL)
        {
            return ((CVANCSplitterInputPin*)m_pTee->GetPin(0))->m_pQSink->Notify(m_pTee, q);
        }
        else
        {
            // No sink set, so pass it upstream
            HRESULT hr;
            IQualityControl * pIQC;

            hr = VFW_E_NOT_FOUND;
            if(((CVANCSplitterInputPin*)m_pTee->GetPin(0))->m_Connected)
            {
                ((CVANCSplitterInputPin*)m_pTee->GetPin(0))->m_Connected->QueryInterface(IID_IQualityControl,(void**)&pIQC);

                if(pIQC!=NULL)
                {
                    hr = pIQC->Notify(m_pTee, q);
                    pIQC->Release();
                }
            }
            return hr;
        }
    }

    // Quality management is too hard to do
    return NOERROR;

} // Notify

HRESULT CVANCSplitterOutputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	if (iPosition == 0)
		pMediaType->Set(m_mt);
	else
		return VFW_S_NO_MORE_ITEMS;

	return S_OK;
}

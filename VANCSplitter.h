////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#pragma once
 
#include "stdafx.h"
#include "global.h"
#include "VANCSplitterInputPin.h"
#include "VANCSplitterOutputPin.h"


// {6A7E647E-ADEC-457D-98E4-20E6B8914191}
DEFINE_GUID(CLSID_VANCSplitter, 0x6a7e647e, 0xadec, 0x457d, 0x98, 0xe4, 0x20, 0xe6, 0xb8, 0x91, 0x41, 0x91);

// {FA953CE0-EBFD-47E2-AC84-34FA6AA2446D}
DEFINE_GUID(IID_IVANCSplitter, 0xfa953ce0, 0xebfd, 0x47e2, 0xac, 0x84, 0x34, 0xfa, 0x6a, 0xa2, 0x44, 0x6d);

MIDL_INTERFACE("FA953CE0-EBFD-47E2-AC84-34FA6AA2446D")
IVANCSplitter : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE SetLogFile(__in_opt LPCOLESTR szLogFilePath) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetVANCLine(__in_opt LONG nVANCLine) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetVANCLine(__out_opt LONG* nVANCLine) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetPacketType(__in_opt LONG nPacketType) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetPacketType(__out_opt LONG* nPacketType) = 0;
};

void DisplayMediaType(TCHAR *pDescription, const CMediaType *pmt);

class CVANCSplitter: public CCritSec, public CBaseFilter, IVANCSplitter, ISpecifyPropertyPages
{
    // Let the pins access our internal state
    friend class CVANCSplitterInputPin;
    friend class CVANCSplitterOutputPin;
    typedef CGenericList <CVANCSplitterOutputPin> COutputList;
	typedef CGenericList <CVANCSplitterInputPin> CInputList;

    INT m_NumOutputPins;            // Current output pin count
	INT m_NumInputPins;            // Current output pin count
    COutputList m_OutputPinsList;   // List of the output pins
	CInputList m_InputPinsList;   // List of the output pins
    INT m_NextOutputPinNumber;      // Increases monotonically.
	INT m_NextInputPinNumber;      // Increases monotonically.
    LONG m_lCanSeek;                // Seekable output pin
    IMemAllocator* m_pAllocator;    // Allocator from our input pin
	IMemAllocator* m_pAllocator2;
	LONG m_nVANCLine;
	LONG m_nPacketType;
	bool m_bTrace;
	TCHAR m_szLogFilePath[MAX_PATH];

public:

	DECLARE_IUNKNOWN;

    CVANCSplitter(TCHAR *pName,LPUNKNOWN pUnk,HRESULT *hr);
    ~CVANCSplitter();

    CBasePin *GetPin(int n);
    int GetPinCount();

    // Function needed for the class factory
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);

    // Send EndOfStream if no input connection
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP Pause();
    STDMETHODIMP Stop();
	STDMETHODIMP GetPages(CAUUID *pPages);
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

	virtual HRESULT STDMETHODCALLTYPE SetLogFile(LPCOLESTR szLogFilePath)
	{
		USES_CONVERSION;

		if(szLogFilePath == NULL)
			memset(m_szLogFilePath, 0, sizeof(m_szLogFilePath));
		else
			_tcscpy_s<MAX_PATH>(m_szLogFilePath, OLE2CT(szLogFilePath));

		SetTraceFile(szLogFilePath);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE SetVANCLine(LONG nLine)
	{
		m_nVANCLine = nLine;
		FilterTrace("CVANCSplitter::SetVANCLine() Line %i (%i)\n", m_nVANCLine, m_nVANCLine + 1);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE GetVANCLine(LONG* nLine)
	{
		*nLine = m_nVANCLine;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPacketType(LONG nPacketType)
	{
		m_nPacketType = nPacketType;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE GetPacketType(LONG* nPacketType)
	{
		*nPacketType = m_nPacketType;
		return S_OK;
	}
	 
	cc_packet_type GetPacketType()
	{
		return (cc_packet_type)m_nPacketType;
	}

	TCHAR* GetLogFileName()
	{
		return m_szLogFilePath;
	}
	 
protected:

    // The following manage the list of output pins
    void InitOutputPinsList();
	void InitInputPinsList();
	CVANCSplitterInputPin *GetPinNFromInList(int n);
    CVANCSplitterOutputPin *GetPinNFromList(int n);
    CVANCSplitterOutputPin *CreateNextOutputPin(CVANCSplitter *pTee);
	CVANCSplitterInputPin *CreateNextInputPin(CVANCSplitter *pTee);
    void DeleteOutputPin(CVANCSplitterOutputPin *pPin);
	void DeleteInputPin(CVANCSplitterInputPin *pPin);
    int GetNumFreePins();
};
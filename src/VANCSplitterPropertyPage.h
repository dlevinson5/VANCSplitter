////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"
#include "VANCSplitter.h"

// {2CE701FE-9F5C-4033-9B5E-29D53FA019B0}
DEFINE_GUID(CLSID_VANCSplitterPropertyPage, 0x2ce701fe, 0x9f5c, 0x4033, 0x9b, 0x5e, 0x29, 0xd5, 0x3f, 0xa0, 0x19, 0xb0);


class CVANCSplitterPropertyPage : public CBasePropertyPage
{
	private:
		CComQIPtr<IVANCSplitter> m_spVANCSplitter;

	public:
		CVANCSplitterPropertyPage(LPUNKNOWN lpUnk, HRESULT *phr);
		~CVANCSplitterPropertyPage(void);

		static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);
		virtual HRESULT OnConnect(IUnknown *pUnknown);
		virtual HRESULT OnActivate();
		virtual HRESULT OnApplyChanges();
};

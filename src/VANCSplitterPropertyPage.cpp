////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VANCSplitterPropertyPage.h"
#include "resource.h"
#include "VANCSplitter.h"
#include "atlconv.h"

CVANCSplitterPropertyPage::CVANCSplitterPropertyPage(LPUNKNOWN lpUnk, HRESULT *phr) : CBasePropertyPage(NAME("Stream Buffer Sink Property Page\0"),lpUnk, IDD_PROPPAGE, IDS_TITLE)
{
}

CVANCSplitterPropertyPage::~CVANCSplitterPropertyPage(void)
{
}

CUnknown* WINAPI CVANCSplitterPropertyPage::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	ATLASSERT(phr);
	return new CVANCSplitterPropertyPage(pUnk, phr);
}

HRESULT CVANCSplitterPropertyPage::OnConnect(IUnknown *pUnknown) 
{
	m_spVANCSplitter = pUnknown;
	return NOERROR;
}

HRESULT CVANCSplitterPropertyPage::OnActivate() 
{ 
	USES_CONVERSION;
	char szBuffer[10];
	LONG nValue = 0;

	for(nValue = 1; nValue < 50; nValue++)
	{
		sprintf_s<10>(szBuffer, "%i", nValue);
		SendDlgItemMessage(m_hwnd, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)A2W(szBuffer));
	}
	 
	for(int i = 0; i < 50; i++)
	{
		sprintf_s<10>(szBuffer, "%i", i);
		SendDlgItemMessage(m_hwnd, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)A2W(szBuffer));
	}

	m_spVANCSplitter->GetVANCLine(&nValue);
	sprintf_s<10>(szBuffer, "%i", nValue + 1);
	SendDlgItemMessage(m_hwnd, IDC_COMBO1, CB_SELECTSTRING, 0, (LPARAM)A2W(szBuffer));

	m_spVANCSplitter->GetPacketType(&nValue);
	sprintf_s<10>(szBuffer, "%i", nValue);
	SendDlgItemMessage(m_hwnd, IDC_COMBO2, CB_SELECTSTRING, 0, (LPARAM)A2W(szBuffer));

	::SetWindowText(GetDlgItem(this->m_hwnd, IDC_EDIT1), ((CVANCSplitter*)m_spVANCSplitter.p)->GetLogFileName());
	m_bDirty = true;

	return NOERROR; 
}

HRESULT CVANCSplitterPropertyPage::OnApplyChanges() 
{
	USES_CONVERSION;
	TCHAR szBuffer[MAX_PATH];
	LONG nResult;
	int nIndex;

	m_bDirty = true;

	nIndex = SendDlgItemMessage(m_hwnd, IDC_COMBO1, CB_GETCURSEL, 0, 0 );
	SendMessage(GetDlgItem(this->m_hwnd, IDC_COMBO1), CB_GETLBTEXT, nIndex, (LPARAM)szBuffer );
	nResult = _tstoi(szBuffer);
	m_spVANCSplitter->SetVANCLine(nResult - 1);

	nIndex = SendDlgItemMessage(m_hwnd, IDC_COMBO2, CB_GETCURSEL, 0, 0 );
	SendMessage(GetDlgItem(this->m_hwnd, IDC_COMBO2), CB_GETLBTEXT, nIndex, (LPARAM)szBuffer );
	nResult = _tstoi(szBuffer);
	m_spVANCSplitter->SetPacketType(nResult);

	::GetWindowText(GetDlgItem(this->m_hwnd, IDC_EDIT1), szBuffer, MAX_PATH);
	m_spVANCSplitter->SetLogFile((_tcslen(szBuffer) == 0)?NULL:szBuffer);
	return NOERROR; 
}

 
 
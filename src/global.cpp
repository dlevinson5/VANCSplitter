////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "global.h"
#include <time.h>

#include "VANCSplitter.h"
#include "VANCSplitterInputPin.h"
#include "VANCSplitterOutputPin.h"
#include "VANCSplitterPropertyPage.h"

// Using this pointer in constructor
#pragma warning(disable:4355 4127)

// Setup data
 
const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
    &MEDIATYPE_NULL,         // Major CLSID
    &MEDIASUBTYPE_NULL       // Minor type
};

const AMOVIESETUP_PIN psudPins[] =
{
    { L"Input",             // Pin's string name
      FALSE,                // Is it rendered
      FALSE,                // Is it an output
      FALSE,                // Allowed none
      FALSE,                // Allowed many
      &CLSID_NULL,          // Connects to filter
      L"Output",            // Connects to pin
      1,                    // Number of types
      &sudPinTypes },       // Pin information
    { L"Output",            // Pin's string name
      FALSE,                // Is it rendered
      TRUE,                 // Is it an output
      FALSE,                // Allowed none
      TRUE,                 // Allowed many
      &CLSID_NULL,          // Connects to filter
      L"Input",             // Connects to pin
      1,                    // Number of types
      &sudPinTypes }        // Pin information
};

const AMOVIESETUP_FILTER sudFilter =
{
    &CLSID_VANCSplitter,    // CLSID of filter
    L"VANC Splitter",		// Filter's name
    MERIT_DO_NOT_USE,       // Filter merit
    2,                      // Number of pins
    psudPins                // Pin information
};


CFactoryTemplate g_Templates [] = {
    { L"VANC Splitter"
    , &CLSID_VANCSplitter
    , CVANCSplitter::CreateInstance
    , NULL
    , &sudFilter },
    { L"VAN Splitter Property Page"
    , &CLSID_VANCSplitterPropertyPage
	, CVANCSplitterPropertyPage::CreateInstance }
};



int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//
// DllRegisterServer
//
STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

//
// DllUnregisterServer
//
STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}


//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}


CCritSec g_lock;
static TCHAR g_szTraceFile[MAX_PATH] = { NULL };

bool IsLogging()
{
	return (g_szTraceFile[0] != NULL);
}
  
void FilterTrace(LPCSTR pszFormat, ...)
{
#ifndef _DEBUG
	if (g_szTraceFile[0] == NULL)
		return;
#endif _DEBUG

	CAutoLock lock(&g_lock);

 	va_list ptr;
	va_start(ptr, pszFormat);

	char buffer[3000], buffer2[3000];
	memset(buffer, 0, 3000);
	memset(buffer2, 0, 3000);
	vsprintf_s(buffer, sizeof(buffer), pszFormat, ptr);

	char tmptime[100];
	char tmpdate[100];
	_strtime_s( tmptime, 100);
	_strdate_s( tmpdate, 100);

	sprintf_s(buffer2, sizeof(buffer), "[%s %s] %s", tmpdate, tmptime, buffer);

#ifdef _DEBUG
	ATLTRACE(buffer2);

	if (g_szTraceFile[0] == NULL)
		return;
#endif _DEBUG
	  
	FILE* pF = NULL;
	_tfopen_s(&pF, g_szTraceFile, L"a");
	
	if (pF != NULL)
	{
		fwrite(buffer2, sizeof(char), strlen(buffer2), pF);
		fclose(pF);
	}
   
	va_end(ptr);
}

void SetTraceFile(LPCTSTR szFilePath)
{
	if (szFilePath == NULL)
	{
		g_szTraceFile[0] = NULL;
		return;
	}

	::DeleteFile(szFilePath);
	_tcscpy_s(g_szTraceFile, 255, szFilePath);

	USES_CONVERSION;
	FilterTrace("Trace [%s]\n", T2A(szFilePath));
}

void WriteToFile(FILE* pF, LPCSTR pszFormat, ...)
{
 	va_list ptr;
	va_start(ptr, pszFormat);

	char buffer[1024], buffer2[1024];
	memset(buffer, 0, 1024);
	memset(buffer2, 0, 1024);
	vsprintf_s(buffer, sizeof(buffer), pszFormat, ptr);

	sprintf_s(buffer2, sizeof(buffer), "%s", buffer);

	if (pF != NULL)
		fwrite(buffer2, sizeof(char), strlen(buffer2), pF);
   
	va_end(ptr);
}
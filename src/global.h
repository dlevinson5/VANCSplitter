////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

void FilterTrace(LPCSTR pszFormat, ...);
void SetTraceFile(LPCTSTR szFilePath);
void WriteToFile(FILE* pF, LPCSTR pszFormat, ...);
bool IsLogging();


 

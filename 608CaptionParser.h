////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <string>
#pragma once

using namespace std;

class C608CaptionParser  
{
	public:
		C608CaptionParser();
		~C608CaptionParser();
		
		HRESULT BufferCB(double SampleTime, BYTE *pBuffer2, long BufferLen);

	private:
		int GetRow(BYTE *pBuffer);
		void RenderBuffer(double SampleTime);

	private:
		BYTE _ccTxMatrix[256];
		double _currentTime;
		std::string _currentBuffer;
		bool _channelOne;
		BYTE _ccRowTable[45]; 
		int m_lastRow;
		BYTE _ccLastCmd[2];
};
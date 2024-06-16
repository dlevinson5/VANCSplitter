////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "608CaptionParser.h"

#define _CCDEBUG
#define CC_DEBUG_INFO

/*********************************************************************************************************
C608CaptionParser class implementation
**********************************************************************************************************/

#pragma region CC_DEBUG_INFO

	#ifdef _CCDEBUG
	bool _DebugRenderCommand(BYTE *pBuffer, std::string& command)
	{
		if (pBuffer[0] == 0x11)
		{
			if (pBuffer[1] >= 0x20 && pBuffer[1] <= 0x2F)
				command = "MRC - Mid-row Code";
		}
		else if (pBuffer[0] == 0x14)
		{
			if (pBuffer[1] == 0x20)
				command = "RCL - Resume caption loading";
			else if (pBuffer[1] == 0x21)
				command = "RB - Backspace";
			else if (pBuffer[1] == 0x22)
				command = "AOF - Alarm Off";
			else if (pBuffer[1] == 0x23)
				command = "AON - Alarm On";
			else if (pBuffer[1] == 0x24)
				command = "DER - Delete to end of row";
			else if (pBuffer[1] == 0x25)
				command = "RU2 - Roll-up captions-2";
			else if (pBuffer[1] == 0x26)
				command = "RU3 - Roll-up captions-3";
			else if (pBuffer[1] == 0x27)
				command = "RU4 - Roll-up captions-4";
			else if (pBuffer[1] == 0x28)
				command = "FON - Flash On";
			else if (pBuffer[1] == 0x29)
				command = "RDC - Resume direct captioning";
			else if (pBuffer[1] == 0x2A)
				command = "TR  - Text restart";
			else if (pBuffer[1] == 0x2B)
				command = "RTD - Resume Text restart";
			else if (pBuffer[1] == 0x2C)
				command = "EDM - Erase Display Memory";
			else if (pBuffer[1] == 0x2D)
				command = "CR  - Carriage Return";
			else if (pBuffer[1] == 0x2E)
				command = "ENM - Erase Non-Display Memory";
			else if (pBuffer[1] == 0x2F)
				command = "EOC - Erase Of Caption (flip-memory)";
			else if (pBuffer[1] == 0x21)
				command = "TO1 - Tab Offset 1 Column";
			else if (pBuffer[1] == 0x22)
				command = "TO1 - Tab Offset 2 Column";
			else if (pBuffer[1] == 0x23)
				command = "TO2 - Tab Offset 3 Column";
		}

		if (command.length() > 0)
			return true;

		return false;
	}
	#endif

#pragma endregion

void trim(std::string& str)
{
	std::string::size_type pos = str.find_last_not_of(' ');

	if(pos != std::string::npos) 
	{
		str.erase(pos + 1);
		pos = str.find_first_not_of(' ');

		if(pos != std::string::npos) 
			str.erase(0, pos);
	}
	else 
		str.erase(str.begin(), str.end());
}

C608CaptionParser::C608CaptionParser()
: _currentTime(0.0)
, _channelOne(true)
{
	memset(_ccLastCmd, 0, sizeof(_ccLastCmd));

	BYTE tmpMatrix[128] = { 
		0x80, 0x01, 0x02, 0x83, 0x04, 0x85, 0x86, 0x07, 0x08, 0x89, 0x8a, 0x0b, 0x8c, 0x0d, 0x0e, 0x8f,
		0x10, 0x91, 0x92, 0x13, 0x94, 0x15, 0x16, 0x97, 0x98, 0x19, 0x1a, 0x9b, 0x1c, 0x9d, 0x9e, 0x1f,
		0x20, 0xa1, 0xa2, 0x23, 0xa4, 0x25, 0x26, 0xa7, 0xa8, 0x29, 0x2a, 0xab, 0x2c, 0xad, 0xae, 0x2f,
		0xb0, 0x31, 0x32, 0xb3, 0x34, 0xb5, 0xb6, 0x37, 0x38, 0xb9, 0xba, 0x3b, 0xbc, 0x3d, 0x3e, 0xbf,
		0x40, 0xc1, 0xc2, 0x43, 0xc4, 0x45, 0x46, 0xc7, 0xc8, 0x49, 0x4a, 0xcb, 0x4c, 0xcd, 0xce, 0x4f,
		0xd0, 0x51, 0x52, 0xd3, 0x54, 0xd5, 0xd6, 0x57, 0x58, 0xd9, 0xda, 0x5b, 0xdc, 0x5d, 0x5e, 0xdf,
		0xe0, 0x61, 0x62, 0xe3, 0x64, 0xe5, 0xe6, 0x67, 0x68, 0xe9, 0xea, 0x6b, 0xec, 0x6d, 0x6e, 0xef,
		0x70, 0xf1, 0xf2, 0x73, 0xf4, 0x75, 0x76, 0xf7, 0xf8, 0x79, 0x7a, 0xfb, 0x7c, 0xfd, 0xfe, 0x7f
	};

	memset(_ccTxMatrix, 0, 256);

	for (int i=0;i<128;i++)
		_ccTxMatrix[tmpMatrix[i]] = i;

	BYTE tmpRowTable[] = { 
		0x11, 0x40, 0x5F,
		0x11, 0x60, 0x7F,
		0x12, 0x40, 0x5F,
		0x12, 0x60, 0x7F,
		0x15, 0x40, 0x5F,
		0x15, 0x60, 0x7F,
		0x16, 0x40, 0x5F,
		0x16, 0x60, 0x7F,
		0x17, 0x40, 0x5F,
		0x17, 0x60, 0x7F,
		0x10, 0x40, 0x5F,
		0x13, 0x40, 0x5F,
		0x13, 0x60, 0x7F,
		0x14, 0x40, 0x5F,
		0x14, 0x60, 0x7F };

	memcpy(_ccRowTable, tmpRowTable, 45);
}

C608CaptionParser::~C608CaptionParser()
{
 
}
  
int C608CaptionParser::GetRow(BYTE *pBuffer)
{
	if (pBuffer[0] >= 11 && pBuffer[0] <= 0x17)
	{
		for (int i=0; i < 15;i++)
		{
			int idx = i * 3;
			int dc1 = _ccRowTable[idx];
 	 
			if (pBuffer[0] == dc1 && pBuffer[1] >= _ccRowTable[idx+1] && pBuffer[1] <= _ccRowTable[idx+2])
				return i;
		}
	}

	return -1;
}

HRESULT C608CaptionParser::BufferCB(double SampleTime, BYTE *pBuffer2, long BufferLen)
{
	if (BufferLen == 2)
	{
		BYTE pBuffer[2];
		memcpy(pBuffer, pBuffer2, 2);

		// Ignore any null requests
		if (pBuffer[0] == 0x80 && pBuffer[1] == 0x80)
			return S_OK;
  
		if (_currentTime == 0.0)
			_currentTime = SampleTime;
  
		// Convert CC from 7-bit
		pBuffer[0] = _ccTxMatrix[ pBuffer[0] ];
		pBuffer[1] = _ccTxMatrix[ pBuffer[1] ];

		#pragma region CC_DEBUG_INFO

		#ifdef _CCDEBUG
		std::string command = "";

		if (_DebugRenderCommand(pBuffer, command))
			ATLTRACE("CC-INFO: [%08x] [%08x] [%s]\n", pBuffer[0], pBuffer[1], command.c_str());
		if (GetRow(pBuffer)> -1)
			ATLTRACE("CC-INFO: [%08x] [%08x] [ROW - Row Changed (Line %i)]\n", pBuffer[0], pBuffer[1], GetRow(pBuffer));
		else
			ATLTRACE("CC-INFO: [%08x] [%08x] [%04x %04x]\n", pBuffer[0], pBuffer[1], (pBuffer[0] == NULL)?' ':pBuffer[0], (pBuffer[1] == NULL)?' ':pBuffer[1]);
		#endif
 
		#pragma endregion  

		if (pBuffer[0] >= 0x20 && pBuffer[0] <= 0x7F && _channelOne)
		{
			_currentBuffer += (char)pBuffer[0];

			// Check second character
			if (pBuffer[1] >= 0x20 && pBuffer[1] <= 0x7F)
				_currentBuffer += (char)pBuffer[1];
		}
		else if (pBuffer[0] >= 0x10 && pBuffer[0] <= 0x14) // Channel 1 codes
		{
			_channelOne = true;
 
			// Detect and ignore duplicate PAC commands
			if (memcmp(pBuffer, _ccLastCmd, 2) == 0)
			{
				#pragma region CC_DEBUG_INFO
				#ifdef _DEBUG
					#ifdef _CCDEBUG
					ATLTRACE("CC-INFO: [%08x] [%08x] [DUPLICATE COMMAND]", pBuffer[0], pBuffer[1]);
					#endif
				#endif
				#pragma endregion

				return S_OK;
			}

			// Store the last command
			memcpy(_ccLastCmd, pBuffer, 2);

			if ((pBuffer[0] == 0x11 && (pBuffer[1] >= 0x20 && pBuffer[1] <= 0x2F)) || // Mid row codes
				(pBuffer[0] == 0x14 && pBuffer[1] == 0x28)) // Flash on
			{
				_currentBuffer += ' ';
			}
			else if (pBuffer[0] == 0x14 && (pBuffer[1] == 0x2C || pBuffer[1] == 0x20 || pBuffer[1] == 0x2D || pBuffer[1] == 0x2E || pBuffer[1] == 0x2F))
			{
				if (pBuffer[1] == 0x2E) // Clear buffer (reset line number)
					m_lastRow = -1;
 
				RenderBuffer(SampleTime); 
			}
			else
			{
  				// Get the current row
				int curRow = GetRow(pBuffer);
	 
				// If the row has changed 
				if (curRow > -1 && curRow != m_lastRow)
				{
					RenderBuffer(SampleTime);
					m_lastRow = curRow;
				}
			}
		}
		else if (pBuffer[0] >= 0x18 && pBuffer[0] <= 0x1F) // Channel 2 codes
		{
			_channelOne = false;
		}
	}
 
	return S_OK;
}

void C608CaptionParser::RenderBuffer(double SampleTime)
{
	trim(_currentBuffer);

	if (_currentBuffer.length() == 0)
		return;

	// Send the line to the closed captioning handler.
	DWORD timeStamp = (DWORD)(_currentTime * 1000);
	ATLTRACE("%s\n", _currentBuffer.c_str());

	#pragma region CC_DEBUG_INFO


	#ifdef _CCDEBUG
	ATLTRACE("CC-INFO: %s", _currentBuffer.c_str());
	#endif
 
	#pragma endregion 
	
	_currentBuffer.clear();
	_currentTime = SampleTime;
}


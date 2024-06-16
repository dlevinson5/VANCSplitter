////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "global.h"
#include "VANCParser.h"
#include <time.h>

char* printBinary(BYTE character, int bit, char* buffer)
{
	memset(buffer, 0, 50);
	strcat_s(buffer, 50, ((character >> (bit - 1)) & 1)?"1":"0");
	return buffer;
}

char* printBinary(BYTE character, char* buffer)
{
	memset(buffer, 0, 50);
	for (int i = 7; i >= 0; --i) 
		strcat_s(buffer, 50, ((character >> i) & 1)?"1":"0");

	return buffer;
};

VANCParser::VANCParser(void) : m_enableLogging(false)
{
	m_szLogFile[0] = NULL;
	vanc_data_packet.vanc_userdata = NULL;   
}

VANCParser::~VANCParser(void)
{
	if (vanc_data_packet.vanc_userdata != NULL)
		free (vanc_data_packet.vanc_userdata);
}

// check if the packet is valid VANC data. scan the entire packet line 
bool VANCParser::IsValidVANCPacket(__int16* packet, DWORD length)
{
	for(UINT i = 0; i < (length - 3); i++)
	{
		if (packet[i] == 0x00 && packet[i + 1] == 0x3ff && packet[i + 2] == 0x3ff)
			return true;
	}

	return false;
}

// Check if the packet is has valid DTVCC  marker
bool VANCParser::IsValidDTVCCPacketPos(__int16* packet)
{
	return (packet[0] == 0x00 && packet[1] == 0x3ff && packet[2] == 0x3ff && packet[3] == 0x161 && packet[4] == 0x101);
}

// Check if the line is a VANC packet and contains DTVCC 
bool VANCParser::IsValidDTVCCPacket(__int16* packet, DWORD length)
{
	return (IsValidVANCPacket(packet, length) && GetDTVCCPacketPos(packet, length) > -1);
}

// Get the starting position of the DTVCC packet 
long VANCParser::GetDTVCCPacketPos(__int16* packet, DWORD length)
{
	for(UINT i = 0; i < (length - 2); i++)
	{
		if (packet[i] == 0x161 && packet[i + 1] == 0x101)
			return i - 3;
	}

	return -1;
}

void VANCParser::Parse(__int16* packet, bool bParseSvcData)
{
	if (vanc_data_packet.vanc_userdata != NULL)
		free (vanc_data_packet.vanc_userdata);

	vanc_data_packet.vanc_marker_1 = packet[0];
	vanc_data_packet.vanc_marker_2 = packet[1];
	vanc_data_packet.vanc_marker_3 = packet[2];
	vanc_data_packet.vanc_did	   = packet[3];
	vanc_data_packet.vanc_sdid	   = packet[4];
	vanc_data_packet.vanc_dc  	   = (unsigned char)packet[5];
	vanc_data_packet.vanc_userdata = (unsigned char*)malloc(vanc_data_packet.vanc_dc * sizeof(char));
	vanc_data_packet.vanc_checksum = packet[5 + vanc_data_packet.vanc_dc + 1];
 
	__int16 checkSum = 0;

	for(int i = 0; i < vanc_data_packet.vanc_dc; i++)
	{
		vanc_data_packet.vanc_userdata[i] = (unsigned char)packet[6 + i] & 0xff;
		checkSum += (unsigned char)vanc_data_packet.vanc_userdata[i];
	}
	  
	cdp_data.cdp_marker_1 = vanc_data_packet.vanc_userdata[0];
	cdp_data.cdp_marker_2 = vanc_data_packet.vanc_userdata[1];
	cdp_data.cdp_length   = (unsigned char)vanc_data_packet.vanc_userdata[2];
	cdp_data.cdp_frame_rate = (unsigned char)vanc_data_packet.vanc_userdata[3] >> 4;
	cdp_data.cdp_flags_timecode_present = (vanc_data_packet.vanc_userdata[4] & 0x80) == 0x80;
	cdp_data.cdp_flags_cc_data_present = (vanc_data_packet.vanc_userdata[4] & 0x40) == 0x40;
	cdp_data.cdp_flags_service_info_present = (vanc_data_packet.vanc_userdata[4] & 0x20) == 0x20;
	cdp_data.cdp_flags_service_info_start = (vanc_data_packet.vanc_userdata[4] & 0x10) == 0x10;
	cdp_data.cdp_flags_service_info_change = (vanc_data_packet.vanc_userdata[4] & 0x8) == 0x8;
	cdp_data.cdp_flags_service_info_complete = (vanc_data_packet.vanc_userdata[4] & 0x4) == 0x4;
	cdp_data.cdp_flags_caption_service_active = (vanc_data_packet.vanc_userdata[4] & 0x2) == 0x2;
	cdp_data.cdp_flags_reserved = (vanc_data_packet.vanc_userdata[4] & 1) == 0x1;
	cdp_data.cdp_sequnce_counter = (((unsigned char)packet[5] << 8) | (unsigned char)packet[6]);
	cdp_data.cc_section_id = vanc_data_packet.vanc_userdata[7]; 
	cdp_data.cc_marker = vanc_data_packet.vanc_userdata[8] & 0xE0;
	cdp_data.cc_count = vanc_data_packet.vanc_userdata[8] & 0x1F;

	int cdp_block_end = vanc_data_packet.vanc_dc - 4; 
	cdp_data.cdp_footer_id = vanc_data_packet.vanc_userdata[cdp_block_end]; // 0x74 (marker)
	cdp_data.cdp_data_sequence_counter = (((unsigned char)vanc_data_packet.vanc_userdata[cdp_block_end + 1] << 8) | (unsigned char)vanc_data_packet.vanc_userdata[cdp_block_end + 2]);

	cdp_data.cdp_packets = new _cdp_cc_packet[cdp_data.cc_count];
	 
	for(int i = 0; i < (cdp_data.cc_count); i++)
	{
		cdp_data.cdp_packets[i].cc_marker = vanc_data_packet.vanc_userdata[9 + (i * 3)] & 0xF8;
		cdp_data.cdp_packets[i].cc_packet_valid = (vanc_data_packet.vanc_userdata[9 + (i * 3)] & 4) == 4;
		cdp_data.cdp_packets[i].cc_packet_type = vanc_data_packet.vanc_userdata[9 + (i * 3)] & 3;
		cdp_data.cdp_packets[i].cc_data_1 = vanc_data_packet.vanc_userdata[9 + (i * 3) + 1];
		cdp_data.cdp_packets[i].cc_data_2 = vanc_data_packet.vanc_userdata[9 + (i * 3) + 2];
	}
	 
	cdp_service_info.cdp_packets = NULL;

	if (bParseSvcData)
	{
		if (cdp_data.cdp_flags_service_info_present)
		{
			int footerOffset = 9 + (cdp_data.cc_count * 3);
			cdp_service_info.cdp_service_info_marker = vanc_data_packet.vanc_userdata[footerOffset];
			cdp_service_info.cdp_service_info_reserved = (vanc_data_packet.vanc_userdata[footerOffset + 1] & 0x80) == 0x80;
			cdp_service_info.cdp_service_info_start = (vanc_data_packet.vanc_userdata[footerOffset + 1] & 0x40) == 0x40;
			cdp_service_info.cdp_service_info_change = (vanc_data_packet.vanc_userdata[footerOffset + 1] & 0x20) == 0x20;
			cdp_service_info.cdp_service_info_complete = (vanc_data_packet.vanc_userdata[footerOffset + 1] & 0x10) == 0x10;
			cdp_service_info.cdp_service_count = (vanc_data_packet.vanc_userdata[footerOffset + 1] & 0x8);
	
			cdp_service_info.cdp_packets = new _cdp_service_info_packet[cdp_service_info.cdp_service_count];
	 
			int serviceBlockOffset = 9 + (cdp_data.cc_count * 3) + 2;
 
			for(int i = 0; i < (cdp_service_info.cdp_service_count); i++)
			{
				serviceBlockOffset =+ (7 * i);

				cdp_service_info.cdp_packets[i].cdp_cc_reserved1 = (vanc_data_packet.vanc_userdata[serviceBlockOffset] & 0x80) == 0x80; // bit 8
				cdp_service_info.cdp_packets[i].cdp_csn_size = (vanc_data_packet.vanc_userdata[serviceBlockOffset] & 0x40) == 0x40; // bit 7
		
				if (cdp_service_info.cdp_packets[i].cdp_csn_size)
				{
					cdp_service_info.cdp_packets[i].cdp_cc_reserved2 = (vanc_data_packet.vanc_userdata[serviceBlockOffset] & 0x20) == 0x20; // bit 6
					cdp_service_info.cdp_packets[i].cdp_cc_service_number = (vanc_data_packet.vanc_userdata[serviceBlockOffset] & 0x1F) == 0x1F;  // bit 5-1 
				}
				else
				{
					cdp_service_info.cdp_packets[i].cdp_cc_service_number = (vanc_data_packet.vanc_userdata[serviceBlockOffset] & 0x3F); // bit 6-1
				}

				cdp_service_info.cdp_packets[i].cdp_service_data1 = vanc_data_packet.vanc_userdata[(serviceBlockOffset) + 1];
				cdp_service_info.cdp_packets[i].cdp_service_data2 = vanc_data_packet.vanc_userdata[(serviceBlockOffset) + 2];
				cdp_service_info.cdp_packets[i].cdp_service_data3 = vanc_data_packet.vanc_userdata[(serviceBlockOffset) + 3];
				cdp_service_info.cdp_packets[i].cdp_service_data4 = vanc_data_packet.vanc_userdata[(serviceBlockOffset) + 4];
				cdp_service_info.cdp_packets[i].cdp_service_data5 = vanc_data_packet.vanc_userdata[(serviceBlockOffset) + 5];
				cdp_service_info.cdp_packets[i].cdp_service_data6 = vanc_data_packet.vanc_userdata[(serviceBlockOffset) + 6];
			}
		}
	}

	if (m_enableLogging)
		LogVANCPacket();
}

bool VANCParser::Get608Packet(byte* line21Pair, cc_packet_type packetType)
{
	for(int i = 0; i < (cdp_data.cc_count); i++)
	{
		if (cdp_data.cdp_packets[i].cc_packet_type == packetType)
		{
			line21Pair[0] = cdp_data.cdp_packets[i].cc_data_1;
			line21Pair[1] = cdp_data.cdp_packets[i].cc_data_2;
			return true;
		}
	}

	return false;
}

void VANCParser::LogVANCPacket()
{
	CHAR buffer[100];

	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("[VANC_DATA_PACKET]\n");
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("%08X (%8i) [VANC MARKER1]\n", vanc_data_packet.vanc_marker_1, vanc_data_packet.vanc_marker_1);
	WriteDebug("%08X (%8i) [VANC MARKER2]\n", vanc_data_packet.vanc_marker_2, vanc_data_packet.vanc_marker_2);
	WriteDebug("%08X (%8i) [VANC MARKER3]\n", vanc_data_packet.vanc_marker_3, vanc_data_packet.vanc_marker_3);
	WriteDebug("%08X (%8i) [DID]         \n", vanc_data_packet.vanc_did, vanc_data_packet.vanc_did);
	WriteDebug("%08X (%8i) [SDID]        \n", vanc_data_packet.vanc_sdid, vanc_data_packet.vanc_sdid);
	WriteDebug("%08X (%8i) [DC]          \n", vanc_data_packet.vanc_dc, vanc_data_packet.vanc_dc);
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");

	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("[CDP HEADER]\n");
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("%08X (%8i) [CDP MARKER1] \n", cdp_data.cdp_marker_1, cdp_data.cdp_marker_1);
	WriteDebug("%08X (%8i) [CDP MARKER2] \n", cdp_data.cdp_marker_2, cdp_data.cdp_marker_2);
	WriteDebug("%08X (%8i) [CDP LENGTH]  \n", cdp_data.cdp_length, cdp_data.cdp_length);
	WriteDebug("%08X (%8i) [FRAME RATE]  \n", cdp_data.cdp_frame_rate, cdp_data.cdp_frame_rate);
	WriteDebug("%08X (%8i) [FLAG:TIME CODE PRESENT]\n", cdp_data.cdp_flags_timecode_present, cdp_data.cdp_flags_timecode_present);
	WriteDebug("%08X (%8i) [FLAG:CC DATA PRESENT]\n", cdp_data.cdp_flags_cc_data_present, cdp_data.cdp_flags_cc_data_present);
	WriteDebug("%08X (%8i) [FLAG:SERVICE INFO PRESENT]\n", cdp_data.cdp_flags_service_info_present, cdp_data.cdp_flags_service_info_present);
	WriteDebug("%08X (%8i) [FLAG:SERVICE INFO START]\n", cdp_data.cdp_flags_service_info_start, cdp_data.cdp_flags_service_info_start);
	WriteDebug("%08X (%8i) [FLAG:SERVICE INFO CHANGE]\n", cdp_data.cdp_flags_service_info_change, cdp_data.cdp_flags_service_info_change);
	WriteDebug("%08X (%8i) [FLAG:SERVICE INFO COMPLETE]\n", cdp_data.cdp_flags_service_info_complete, cdp_data.cdp_flags_service_info_complete);
	WriteDebug("%08X (%8i) [FLAG:CC SERVICEC ACTIVE]\n", cdp_data.cdp_flags_caption_service_active, cdp_data.cdp_flags_caption_service_active);
	WriteDebug("%08X (%8i) [FLAG:RESERVED]\n", cdp_data.cdp_flags_reserved, cdp_data.cdp_flags_reserved);
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
 
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("[CDP FOOTER]\n");
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("%08X (%8i) [CDP FOOTER MARKER] \n", cdp_data.cdp_footer_id, cdp_data.cdp_footer_id);
	WriteDebug("%08X (%8i) [CDP SEQUENCE COUNTER] \n", cdp_data.cdp_data_sequence_counter, cdp_data.cdp_data_sequence_counter);
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");

	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("[CC HEADER]\n");
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	WriteDebug("%08X (%8i) [CC SECTION ID]\n", cdp_data.cc_section_id, cdp_data.cc_section_id);
	WriteDebug("%08X (%8i) [CC MARKER] (%s)\n", cdp_data.cc_marker, cdp_data.cc_marker, printBinary(cdp_data.cc_marker, buffer));
	WriteDebug("%08X (%8i) [CC COUNT]\n", cdp_data.cc_count, cdp_data.cc_count);
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");

	WriteDebug("[CC VALID..........] [CC TYPE ..........] [CC DATA1..........] [CC DATA2..........]\n");
	for(int i = 0; i < (cdp_data.cc_count); i++)
	{
		WriteDebug("%08X (%8i)  ", cdp_data.cdp_packets[i].cc_packet_valid, cdp_data.cdp_packets[i].cc_packet_valid);
		WriteDebug("%08X (%8i)  ", cdp_data.cdp_packets[i].cc_packet_type, cdp_data.cdp_packets[i].cc_packet_type);
		WriteDebug("%08X (%8i)  ", cdp_data.cdp_packets[i].cc_data_1, cdp_data.cdp_packets[i].cc_data_1);
		WriteDebug("%08X (%8i)\n", cdp_data.cdp_packets[i].cc_data_2, cdp_data.cdp_packets[i].cc_data_2);
	}

	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");

	if (cdp_data.cdp_flags_cc_data_present)
	{
		WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
		WriteDebug("[CDP SERVICE INFO]\n");
		WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
		WriteDebug("%08X (%8i) [INFO MARKER]\n", cdp_service_info.cdp_service_info_marker, cdp_service_info.cdp_service_info_marker);
		WriteDebug("%08X (%8i) [INFO RESERVED]\n", cdp_service_info.cdp_service_info_reserved, cdp_service_info.cdp_service_info_reserved);
		WriteDebug("%08X (%8i) [INFO START]\n", cdp_service_info.cdp_service_info_start, cdp_service_info.cdp_service_info_start);
		WriteDebug("%08X (%8i) [INFO CHANGE]\n", cdp_service_info.cdp_service_info_change, cdp_service_info.cdp_service_info_change);
		WriteDebug("%08X (%8i) [INFO COMPLETE]\n", cdp_service_info.cdp_service_info_complete, cdp_service_info.cdp_service_info_complete);
		WriteDebug("%08X (%8i) [SERVICE COUNT]\n", cdp_service_info.cdp_service_count, cdp_service_info.cdp_service_count);
		WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	}

	if (cdp_data.cdp_flags_service_info_present)
	{
		WriteDebug("[RESERVED..........] [CSN SIZE..........] [RESERVED..........] [CSN SERVICE NUMBER] [SERVICE DATA]\n");
		for(int i = 0; i < (cdp_service_info.cdp_service_count); i++)
		{
			WriteDebug("%08X (%8i)  ", cdp_service_info.cdp_packets[i].cdp_cc_reserved1, cdp_service_info.cdp_packets[i].cdp_cc_reserved1);
			WriteDebug("%08X (%8i)  ", cdp_service_info.cdp_packets[i].cdp_csn_size, cdp_service_info.cdp_packets[i].cdp_csn_size);
			WriteDebug("%08X (%8i)  ", cdp_service_info.cdp_packets[i].cdp_cc_reserved2, cdp_service_info.cdp_packets[i].cdp_cc_reserved2);
			WriteDebug("%08X (%8i)  ", cdp_service_info.cdp_packets[i].cdp_cc_service_number, cdp_service_info.cdp_packets[i].cdp_cc_service_number);

			WriteDebug("%08X ", cdp_service_info.cdp_packets[i].cdp_service_data1);
			WriteDebug("%08X ", cdp_service_info.cdp_packets[i].cdp_service_data2);
			WriteDebug("%08X ", cdp_service_info.cdp_packets[i].cdp_service_data3);
			WriteDebug("%08X ", cdp_service_info.cdp_packets[i].cdp_service_data4);
			WriteDebug("%08X ", cdp_service_info.cdp_packets[i].cdp_service_data5);
			WriteDebug("%08X \n", cdp_service_info.cdp_packets[i].cdp_service_data6);
		}
	}

	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	 
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	for(int i = 0; i < vanc_data_packet.vanc_dc; i++)
		WriteDebug("%03x (%i)", vanc_data_packet.vanc_userdata[i] & 0xff, i);
	WriteDebug("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
}

void VANCParser::SetTrace(TCHAR* filePath)
{
	m_enableLogging = (filePath != NULL);
	 
	if (filePath != NULL)
		_tcscpy_s<MAX_PATH>(m_szLogFile, filePath);
	else
		memset(m_szLogFile, NULL, MAX_PATH);
}

void VANCParser::WriteDebug(LPCSTR pszFormat, ...)
{
 	va_list ptr;
	va_start(ptr, pszFormat);

	char buffer[1024], buffer2[1024];
	memset(buffer, 0, 1024);
	memset(buffer2, 0, 1024);
	vsprintf_s(buffer, sizeof(buffer), pszFormat, ptr);

	sprintf_s(buffer2, sizeof(buffer), "%s", buffer);
	ATLTRACE(buffer2);

	if (m_szLogFile[0] != NULL)
	{
		FILE* pF = NULL;
		_tfopen_s(&pF, m_szLogFile, L"a");
	
		if (pF != NULL)
		{
			fwrite(buffer2, sizeof(char), strlen(buffer2), pF);
			fclose(pF);
		}
   
		va_end(ptr);
	}
}
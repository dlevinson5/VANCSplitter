////////////////////////////////////////////////////////////////////////////////
// VANCSplitter - A VANC 608 caption parser Direct Show Filter.
//
// Copyright (c) 2024 David Levinson 
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

struct _vanc_data_packet
{
	__int16  vanc_marker_1; // 0x000;
	__int16  vanc_marker_2; // 0x3ff;
	__int16  vanc_marker_3; // 0x3ff;
	__int16  vanc_did;		// data packet id
	__int16  vanc_sdid;		// secondary data id
	unsigned char   vanc_dc;		// data count
	unsigned char*  vanc_userdata; // data count
	__int16  vanc_checksum;	// data checksum;
};

struct _cdp_cc_packet
{
	unsigned char   cc_marker;		  // [BYTE1 [bit 8-4]
	bool	 cc_packet_valid; // [BYTE1 [bit 3]
	unsigned char   cc_packet_type;  // [BYTE1 [bit 1-2]
	unsigned char   cc_data_1;		  // [BYTE2]
	unsigned char   cc_data_2;		  // [BYTE2]
};

struct _cdp_data
{
	unsigned char   cdp_marker_1; // 0x296;
	unsigned char   cdp_marker_2; // 0x296;
	unsigned char   cdp_length;   // packet length
	unsigned char   cdp_frame_rate;  // packet length
	bool	 cdp_flags_timecode_present;
	bool	 cdp_flags_cc_data_present;
	bool	 cdp_flags_service_info_present;
	bool	 cdp_flags_service_info_start;
	bool	 cdp_flags_service_info_change;
	bool	 cdp_flags_service_info_complete;
	bool	 cdp_flags_caption_service_active;
	bool	 cdp_flags_reserved;
	__int16	 cdp_sequnce_counter;
	unsigned char   cc_section_id;
	unsigned char   cc_marker;
	unsigned char   cc_count;
	_cdp_cc_packet* cdp_packets;

	unsigned char   cdp_footer_id;
	__int16  cdp_data_sequence_counter;
};
 
struct _cdp_service_info_packet
{
	bool     cdp_cc_reserved1;
	bool     cdp_csn_size;
	bool     cdp_cc_reserved2;
	unsigned char   cdp_cc_service_number;
	unsigned char   cdp_service_data1;
	unsigned char   cdp_service_data2;
	unsigned char   cdp_service_data3;
	unsigned char   cdp_service_data4;
	unsigned char   cdp_service_data5;
	unsigned char   cdp_service_data6;
};

struct _cdp_service_info
{
	unsigned char   cdp_service_info_marker; 
	bool	 cdp_service_info_reserved;
	bool	 cdp_service_info_start;
	bool	 cdp_service_info_change;
	bool	 cdp_service_info_complete;
	unsigned char   cdp_service_count;
	_cdp_service_info_packet* cdp_packets;
};

enum cc_packet_type { NTSC_CC1 = 0x00, NTSC_CC2 = 0x01, NTSC_DTVCC = 0x02, NTSC_DTVCC_START = 0x03 };

class VANCParser
{
	public:
		VANCParser(void);
		~VANCParser(void);

	public:
		bool Get608Packet(BYTE* line21Pair, cc_packet_type packetType = NTSC_CC1);
		void Parse(__int16* packet, bool bParseSvcData = false);
		void SetTrace(TCHAR* filePath);
		bool IsValidVANCPacket(__int16* packet, DWORD length);
		bool IsValidDTVCCPacket(__int16* packet, DWORD length);
		bool IsValidDTVCCPacketPos(__int16* packet);
		long GetDTVCCPacketPos(__int16* packet, DWORD length);

	private:
		void LogVANCPacket();
		void WriteDebug(LPCSTR pszFormat, ...);

	private:
		bool m_enableLogging;
		TCHAR m_szLogFile[MAX_PATH];
		_vanc_data_packet vanc_data_packet;
		_cdp_data cdp_data;
		_cdp_service_info cdp_service_info;
};


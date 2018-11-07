
#include "WebSocketProtocol.h"

#include <string.h>
#include <sstream>
#include <string>

#include "utility/SHA1.h"
#include "utility/Base64.h"

#define PAYLOAD_SIZE_BASIC		125 
#define PAYLOAD_SIZE_EXTENDED	0xFFFF


/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-------+-+-------------+-------------------------------+
        |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
        |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
        |N|V|V|V|       |S|             |   (if payload len==126/127)   |
        | |1|2|3|       |K|             |                               |
        +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
        |     Extended payload length continued, if payload len == 127  |
        + - - - - - - - - - - - - - - - +-------------------------------+
        |                               |Masking-key, if MASK set to 1  |
        +-------------------------------+-------------------------------+
        | Masking-key (continued)       |          Payload Data         |
        +-------------------------------- - - - - - - - - - - - - - - - +
        :                     Payload Data continued ...                :
        + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
        |                     Payload Data continued ...                |
        +---------------------------------------------------------------+
		
		FIN
				*  1bit: 0表示该message后续还有frame；1表示是message的最后一个frame
        opcode:
                *  %x0 denotes a continuation frame
                *  %x1 denotes a text frame
                *  %x2 denotes a binary frame
                *  %x3-7 are reserved for further non-control frames
                *  %x8 denotes a connection close
                *  %x9 denotes a ping
                *  %xA denotes a pong
                *  %xB-F are reserved for further control frames

        Payload length:  7 bits, 7+16 bits, or 7+64 bits

        Masking-key:  0 or 4 bytes
*/

struct WebSocketHeader
{
	bool fin;
	bool rsv1;
	bool rsv2;
	bool rsv3;

	WSOPCODE wscode;
	bool mask;
	int payload_len;
};

CWebSocketProtocol::CWebSocketProtocol(void)
	
{
}


CWebSocketProtocol::~CWebSocketProtocol(void)
{
}

int CWebSocketProtocol::Handshake(char *pBuffer, uint32_t uLen, std::string *strOut)
{
	std::istringstream stream(pBuffer);
	std::string req_line;
	std::string::size_type pos = 0;
	std::string websocket_key;
	bool is_find = false;

	std::getline(stream, req_line);
	if(req_line.substr(0, 4) != "GET ")
	{
		return -1;
	}


	while(std::getline(stream, req_line) && req_line != "\r")
	{
		req_line.erase(req_line.end() - 1);
		pos = req_line.find(": ", 0);
		if (pos != std::string::npos)
		{
			std::string key = req_line.substr(0, pos);  
			std::string value = req_line.substr(pos + 2);
			if(key == "Sec-WebSocket-Key")
			{
				is_find = true;
				websocket_key = value;
				break;
			}
		}
	}

	if(!is_find)
	{
		return -1;
	}

	*strOut = MakeHandshakeResponse(websocket_key.c_str());

	return 0;
}
char* CWebSocketProtocol::DecodeFrame(char* pBuffer, unsigned int uLen, int &outLen, int &wsPackageLength)
{
	WebSocketHeader wsHeader;
	unsigned char* inp = (unsigned char*)(pBuffer);

	wsHeader.fin = inp[0] & 0x80;
	wsHeader.rsv1 = inp[0] & 0x40;
	wsHeader.rsv2 = inp[0] & 0x20;
	wsHeader.rsv3 = inp[0] & 0x10;
	wsHeader.wscode = (WSOPCODE)(inp[0] & 0x0F);
	wsHeader.mask = inp[1] & 0x80;

	unsigned long long payload_length = 0;
	int pos = 2;
	int length_field = inp[1] & (~0x80);
	unsigned int mask = 0;

	if(length_field <= PAYLOAD_SIZE_BASIC)
	{
		payload_length = length_field;
	}
	else if(length_field == 126)  //msglen is 16bit!
	{ 
		payload_length = (inp[2] << 8) + inp[3];
		pos += 2;
	}
	else if(length_field == 127)  //msglen is 64bit!
	{
		payload_length = ((unsigned long long)inp[2] << 56) + ((unsigned long long)inp[3] << 48) + 
			((unsigned long long)inp[4] << 40) + ((unsigned long long)inp[5] << 32) +
			((unsigned long long)inp[6] << 24) + ((unsigned long long)inp[7] << 16) +
			((unsigned long long)inp[8] << 8)  + ((unsigned long long)inp[9] << 0);               
		pos += 8;
	}

	if((int)uLen < (payload_length + pos))
	{
		return 0; //不是完整的包
	}

	outLen = payload_length;
	wsPackageLength = payload_length + pos;

	if(wsHeader.mask)
	{
		mask = *((unsigned int*)(inp+pos));
		pos += 4;

		// unmask data:
		for(int i = pos; i < payload_length + pos; i++)
		{
			inp[i] = inp[i] ^ ((unsigned char*)(&mask))[i%4];
		}
	}

	return pBuffer + pos;
}
char* CWebSocketProtocol::EncodeFrame(char* pBuffer, unsigned int uLen, int &outLen)
{
	char *pOutBuffer = NULL;

	if(uLen <= PAYLOAD_SIZE_BASIC)  // 125
	{
		outLen = uLen + 2;
		pOutBuffer = pBuffer - 2;
		pOutBuffer[1] = uLen;
	}
	else if(uLen <= PAYLOAD_SIZE_EXTENDED)   // 65535
	{
		outLen = uLen + 4;
		pOutBuffer = pBuffer - 4;
		pOutBuffer[1] = 126; //16 bit length
		pOutBuffer[2] = (uLen >> 8) & 0xFF; // rightmost first
		pOutBuffer[3] = uLen & 0xFF;
	}
	else  // >2^16-1
	{
		outLen = uLen + 10;
		pOutBuffer = pBuffer - 10;
		pOutBuffer[1] = 127; //64 bit length

		char *tmp = (char*)&uLen;
		pOutBuffer[2] = tmp[3];
		pOutBuffer[3] = tmp[2];
		pOutBuffer[4] = tmp[1];
		pOutBuffer[5] = tmp[0];
		pOutBuffer[6] = 0;
		pOutBuffer[7] = 0;
		pOutBuffer[8] = 0;
		pOutBuffer[9] = 0;
	}

	pOutBuffer[0] = (0x80 | WSOPCODE::WS_OPCODE_BINARY);

	return pOutBuffer;
}

std::string CWebSocketProtocol::MakeHandshakeResponse(const char* seckey)
{
	std::string answer("");
	answer += "HTTP/1.1 101 Switching Protocols\r\n";
	answer += "Upgrade: WebSocket\r\n";
	answer += "Connection: Upgrade\r\n";
	answer += "Sec-WebSocket-Version: 13\r\n";
	if(seckey)
	{
		std::string key(seckey);
		key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		char shakey[20] = {0};
		zl::util::SHA1 sha1;
		sha1.update(key);
		sha1.final(shakey);
		key = zl::util::base64Encode(shakey, 20);
		answer += ("Sec-WebSocket-Accept: "+ key + "\r\n");
	}
	answer += "\r\n";
	return answer;
}

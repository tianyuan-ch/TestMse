#pragma once


#include <string>


enum WSOPCODE
{
	WS_OPCODE_CONTINUE				= 0x0,
	WS_OPCODE_TEXT					= 0x1,
	WS_OPCODE_BINARY				= 0x2,
	WS_OPCODE_CLOSE					= 0x8,
	WS_OPCODE_PING					= 0x9,
	WS_OPCODE_PONG					= 0xa
};

struct BufferDescribe
{
	int nOffset;
	int nLength;
	WSOPCODE wsCode;
};

class CWebSocketProtocol
{
public:
	CWebSocketProtocol(void);
	~CWebSocketProtocol(void);

	static int Handshake(char *pBuffer, unsigned int uLen, std::string *strOut);
	static char* DecodeFrame(char* pBuffer, unsigned int uLen, int &outLen, int &wsPackageLength);
	//pBuffer之前空10个字节
	static char* EncodeFrame(char* pBuffer, unsigned int uLen, int &outLen);

private:
	static std::string MakeHandshakeResponse(const char* seckey);

};

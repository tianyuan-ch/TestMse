#pragma once

#include <string>

#ifdef WIN32
#include "IOCP/PerSocketContext.h"
#else
#include "EPOLL/EPollPerSocketContext.h"
#endif

class CServerModelUser
{
public:
	virtual ~CServerModelUser() {};
	// ���տͻ������ӻص����ͻ���(�̳�CPerSocketContext)����ppContext����ָ�뷵�ظ���������
	virtual int CBAccept(CPerSocketContext **ppSocketContext) = 0;
	// ���տͻ������ݻص���
	virtual int CBRecv(CPerSocketContext *pContext, char *pBuf, uint32_t uBytesTransfered) = 0;
	// ���͸��ͻ���������ɻص���
	virtual int CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered) = 0;
	// ֪ͨ������Դ
	virtual int CBDisconnect(CPerSocketContext *pContext) = 0;
};

class CServerModelInterface
{
public:
	CServerModelInterface(void) {};
	virtual ~CServerModelInterface(void) {};

	virtual int  Start(const char* server_address, uint16_t server_port, int num_thread = 0) = 0;
	virtual void Stop() = 0;
	virtual int  Send(CPerSocketContext *p_socket_context, char* p_buffer, uint32_t len) = 0;
	virtual int  Disconnect(CPerSocketContext *pSocketContext) = 0;
	
	virtual std::string GetIP() = 0;

};

CServerModelInterface* NewServer(CServerModelUser *pUser);


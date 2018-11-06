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
	// 接收客户端连接回调，客户端(继承CPerSocketContext)分配ppContext对象，指针返回给服务器。
	virtual int CBAccept(CPerSocketContext **ppSocketContext) = 0;
	// 接收客户端数据回调。
	virtual int CBRecv(CPerSocketContext *pContext, char *pBuf, uint32_t uBytesTransfered) = 0;
	// 发送给客户端数据完成回调。
	virtual int CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered) = 0;
	// 通知回收资源
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


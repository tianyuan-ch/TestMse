#pragma once

#include <winsock2.h>
#include <MSWSock.h>
#include <windows.h>

#include <vector>

#include "cm/cm_thread_critical_region.h"


// 缓冲区长度 (1024*8)
#define MAX_BUFFER_LEN        8192  
// 默认端口
#define DEFAULT_PORT          12345    
// 默认IP地址
#define DEFAULT_IP            "127.0.0.1"

typedef enum _OPERATION_TYPE  
{  
	ACCEPT_POSTED,					   // 标志投递的Accept操作
	SEND_POSTED,                       // 标志投递的是发送操作
	RECV_POSTED,                       // 标志投递的是接收操作
	NULL_POSTED                        // 用于初始化，无意义
}OPERATION_TYPE;
typedef struct _PER_IO_CONTEXT
{
	OVERLAPPED     m_Overlapped;                               // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)              
	SOCKET         m_sockAccept;                               // 这个网络操作所使用的Socket
	WSABUF         m_wsaBuf;                                   // WSA类型的缓冲区，用于给重叠操作传参数的
	char           m_szBuffer[MAX_BUFFER_LEN];                 // 这个是WSABUF里具体存字符的缓冲区
	uint32_t       m_uRecvOffsetBytes;
	volatile char  m_OpType;								   // 标识网络操作的类型(对应上面的枚举) (原子操作)

	int ResetWSABuf(int nAllLength, int nUseLength)
	{
		if (nUseLength > nAllLength)
			return -1;

		if (nUseLength > MAX_BUFFER_LEN || nAllLength > MAX_BUFFER_LEN)
			return -1;

		int nLen = nAllLength - nUseLength;

		if (nLen != 0)
			memcpy(m_szBuffer, m_szBuffer + nUseLength, nLen);

		m_uRecvOffsetBytes = nLen;
		m_wsaBuf.buf = m_szBuffer + nLen;
		m_wsaBuf.len = MAX_BUFFER_LEN - nLen;
	}

	_PER_IO_CONTEXT()
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));  
		ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
		m_sockAccept = INVALID_SOCKET;
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
		m_OpType     = NULL_POSTED;
		m_uRecvOffsetBytes = 0;
	}
	~_PER_IO_CONTEXT()
	{
	}
} PER_IO_CONTEXT, *PPER_IO_CONTEXT;

class CBaseLock;

class CPerSocketContext
{
public:
	CPerSocketContext();
	virtual ~CPerSocketContext();
	//赋值m_Socket m_ClientAddr
	void InitSocket(SOCKET socket, SOCKADDR_IN* clientAddr);

	SOCKET GetSocket() { return m_Socket;}
	char* GetClientIP();
	uint16_t GetClientPort();

	void CloseSocket();
	_PER_IO_CONTEXT* GetNewIoContext();
	void RemoveContext(_PER_IO_CONTEXT* pContext);
	_PER_IO_CONTEXT* GetIdleIoContext(OPERATION_TYPE type);
	int SetIdleIoContext(_PER_IO_CONTEXT* pContext);

	SOCKADDR_IN m_ClientAddr;								// 客户端的地址
private:
	SOCKET      m_Socket;									// 每一个客户端连接的Socket
	std::vector<_PER_IO_CONTEXT*> m_vectorIoContext;		// 客户端网络操作的上下文数据

	volatile int m_ReferenceCount;

	cm::CBaseLock *m_pLock;

	char m_ip[32];
};
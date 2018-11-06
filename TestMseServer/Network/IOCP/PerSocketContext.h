#pragma once

#include <winsock2.h>
#include <MSWSock.h>
#include <windows.h>

#include <vector>

#include "cm/cm_thread_critical_region.h"


// ���������� (1024*8)
#define MAX_BUFFER_LEN        8192  
// Ĭ�϶˿�
#define DEFAULT_PORT          12345    
// Ĭ��IP��ַ
#define DEFAULT_IP            "127.0.0.1"

typedef enum _OPERATION_TYPE  
{  
	ACCEPT_POSTED,					   // ��־Ͷ�ݵ�Accept����
	SEND_POSTED,                       // ��־Ͷ�ݵ��Ƿ��Ͳ���
	RECV_POSTED,                       // ��־Ͷ�ݵ��ǽ��ղ���
	NULL_POSTED                        // ���ڳ�ʼ����������
}OPERATION_TYPE;
typedef struct _PER_IO_CONTEXT
{
	OVERLAPPED     m_Overlapped;                               // ÿһ���ص�����������ص��ṹ(���ÿһ��Socket��ÿһ����������Ҫ��һ��)              
	SOCKET         m_sockAccept;                               // ������������ʹ�õ�Socket
	WSABUF         m_wsaBuf;                                   // WSA���͵Ļ����������ڸ��ص�������������
	char           m_szBuffer[MAX_BUFFER_LEN];                 // �����WSABUF�������ַ��Ļ�����
	uint32_t       m_uTransmitBytes;
	volatile char  m_OpType;								   // ��ʶ�������������(��Ӧ�����ö��) (ԭ�Ӳ���)

	_PER_IO_CONTEXT()
	{
		ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));  
		ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
		m_sockAccept = INVALID_SOCKET;
		m_wsaBuf.buf = m_szBuffer;
		m_wsaBuf.len = MAX_BUFFER_LEN;
		m_OpType     = NULL_POSTED;
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
	~CPerSocketContext();
	//��ֵm_Socket m_ClientAddr
	void InitSocket(SOCKET socket, SOCKADDR_IN* clientAddr);

	SOCKET GetSocket() { return m_Socket;}
	char* GetClientIP();
	uint16_t GetClientPort();

	void CloseSocket();
	_PER_IO_CONTEXT* GetNewIoContext();
	void RemoveContext(_PER_IO_CONTEXT* pContext);
	_PER_IO_CONTEXT* GetIdleIoContext(OPERATION_TYPE type);
	int SetIdleIoContext(_PER_IO_CONTEXT* pContext);

	SOCKADDR_IN m_ClientAddr;								// �ͻ��˵ĵ�ַ
private:
	SOCKET      m_Socket;									// ÿһ���ͻ������ӵ�Socket
	std::vector<_PER_IO_CONTEXT*> m_vectorIoContext;		// �ͻ����������������������

	volatile int m_ReferenceCount;

	cm::CBaseLock *m_pLock;

	char m_ip[32];
};
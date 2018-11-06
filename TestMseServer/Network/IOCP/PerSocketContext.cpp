#include "IOCPModel.h"

#include <WS2tcpip.h>

CPerSocketContext::CPerSocketContext()
{
	m_pLock = cm::CreateLock();

	m_Socket = INVALID_SOCKET;
	m_ReferenceCount = 0;
	memset(&m_ClientAddr, 0, sizeof(m_ClientAddr));

	GetNewIoContext();
	GetNewIoContext();
	GetNewIoContext();
	GetNewIoContext();
}
CPerSocketContext::~CPerSocketContext()
{
	CloseSocket();
	// 释放掉所有的IO上下文数据
	for(UINT i=0;i<m_vectorIoContext.size();i++ )
	{
		delete m_vectorIoContext[i];
	}
	m_vectorIoContext.clear();
}

void CPerSocketContext::InitSocket(SOCKET socket, SOCKADDR_IN* clientAddr)
{
	m_Socket = socket;
	memcpy(&m_ClientAddr, clientAddr, sizeof(SOCKADDR_IN));

	inet_ntop(AF_INET, (void*)&m_ClientAddr.sin_addr, m_ip, 32);
	//strcpy_s(m_ip, inet_ntoa(m_ClientAddr.sin_addr));
}

char* CPerSocketContext::GetClientIP()
{
	return m_ip;
}
uint16_t CPerSocketContext::GetClientPort()
{
	return m_ClientAddr.sin_port;
}

void CPerSocketContext::CloseSocket()
{
	if( m_Socket!=INVALID_SOCKET )
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
}
// 获取一个新的IoContext
_PER_IO_CONTEXT* CPerSocketContext::GetNewIoContext()
{
	_PER_IO_CONTEXT* p = new _PER_IO_CONTEXT;

	p->m_OpType = NULL_POSTED;
	m_vectorIoContext.push_back(p);	

	return p;
}
// 从数组中移除一个指定的IoContext
void CPerSocketContext::RemoveContext( _PER_IO_CONTEXT* pContext )
{
	assert(pContext!=NULL);

	for(UINT i=0;i<m_vectorIoContext.size();i++ )
	{
		if( pContext==m_vectorIoContext[i] )
		{
			delete pContext;
			pContext = NULL;
			m_vectorIoContext.erase(m_vectorIoContext.begin() + i);			
			break;
		}
	}
}
// 获取一个空闲的IoContext  type为 SEND_POSTED or RECV_POSTED
_PER_IO_CONTEXT* CPerSocketContext::GetIdleIoContext(OPERATION_TYPE type)
{
	cm::CAutoLock autoLock(m_pLock);

	//m_ReferenceCount为0时,不处理SEND_POST。因为创建后始终存在一个RECV_POSTED，如果它不存在了就代表此对象被回收了。
	if(type != RECV_POSTED && m_ReferenceCount == 0)
	{
		return NULL;
	}

	for(UINT i = 0; i < m_vectorIoContext.size(); i++)
	{
		if(m_vectorIoContext[i]->m_OpType == NULL_POSTED)
		{
			m_ReferenceCount++;
			m_vectorIoContext[i]->m_OpType = type;
			return m_vectorIoContext[i];
		}
	}
	return NULL;
}
int CPerSocketContext::SetIdleIoContext(_PER_IO_CONTEXT* pContext)
{
	cm::CAutoLock autoLock(m_pLock);
	pContext->m_OpType = NULL_POSTED;
	m_ReferenceCount --;

	return m_ReferenceCount;
}
#include "EPollPerSocketContext.h"

#ifndef WIN32

#include <string.h>

CPerSocketContext::CPerSocketContext()
{
	memset(m_ip, 0, 32);
}
CPerSocketContext::~CPerSocketContext()
{

}

void CPerSocketContext::SetSocket(SOCKET socket, sockaddr_in* p_client_addr)
{
	socket_ = socket;
	if(p_client_addr)
	{
		memcpy(&client_addr_, p_client_addr, sizeof(sockaddr_in));
		strcpy(m_ip, inet_ntoa(m_ClientAddr.sin_addr));
	}
}

char* CPerSocketContext::GetClientIP()
{
	return m_ip;
}
int CPerSocketContext::GetClientPort()
{
	return client_addr_.sin_port;
}

#endif
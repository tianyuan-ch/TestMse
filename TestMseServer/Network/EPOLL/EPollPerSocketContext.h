#pragma once

#ifndef WIN32

#include "cm/cm_socket.h"

class CPerSocketContext
{
public:
	CPerSocketContext();
	~CPerSocketContext();

	void SetSocket(SOCKET socket, sockaddr_in* p_client_addr);
	SOCKET GetSocket() { return socket_; }

	char *GetClientIP();
	int GetClientPort();

private:
	SOCKET socket_;
	sockaddr_in client_addr_;
};


#endif
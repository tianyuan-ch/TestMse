#include"cm_socket.h"


#ifdef WIN32
#include <MSTCPiP.h>
#pragma comment(lib,"ws2_32.lib")
#define S_ADDR(addr) addr.sin_addr.S_un.S_addr

#else
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h> 
#include <memory.h>
#include <netinet/tcp.h>

#define S_ADDR(addr)	addr.sin_addr.s_addr

#endif

namespace cm {

int cm_socket_init_environment()
{
#ifdef WIN32
	WSADATA Ws;
	if(WSAStartup(MAKEWORD(2,2), &Ws) != 0)
	{
		return -1;
	}
#endif
	return 0;
}
int cm_socket_free_environment()
{
#ifdef WIN32
	WSACleanup();
#endif
	return 0;
}

int cm_socket_closesocket(SOCKET socket)
{
#ifdef WIN32
	closesocket(socket);
#else
	close(socket);
#endif
	return 0;
}
int cm_socket_epoll_setsocket(SOCKET socket)
{
#ifndef WIN32
	int reuse = 1;  
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));  
#endif
	return 0;
}
int cm_socket_setsocketopt_noblock(SOCKET socket)
{
#ifdef WIN32
	DWORD nMode = 1;
	int nRes = ioctlsocket(socket, FIONBIO, &nMode);
	if(nRes == SOCKET_ERROR)
	{
		return -1;
	}
#else
	int flags, ret;
	flags = fcntl(socket, F_GETFL, 0);
	if(flags == -1)
	{
		return -1;
	}
	flags |= O_NONBLOCK; 
	ret = fcntl(socket, F_SETFL, flags);
	if(ret == -1)
	{
		return -1;
	}
#endif

	return 0;
}

int cm_socket_setkeepalivetime(SOCKET socket, int keep_idle, int keep_interval, int keep_count)
{
#ifdef WIN32
	uint32_t ret_bytes;
	tcp_keepalive in_keep_setting;
	tcp_keepalive ret_keep_setting;

	in_keep_setting.onoff = keep_count;
	in_keep_setting.keepalivetime = keep_idle; 
	in_keep_setting.keepaliveinterval = keep_interval;

	//if (WSAIoctl(socket, SIO_KEEPALIVE_VALS, 
	//	&in_keep_setting,
	//	sizeof(in_keep_setting),
	//	&ret_keep_setting,
	//	sizeof(ret_keep_setting),
	//	&ret_bytes,
	//	NULL,
	//	NULL) != 0)
	//{
	//	return -1;
	//}
	return 0;
#else
	int keep_alive = 1;

	if(setsockopt(socket,SOL_SOCKET,SO_KEEPALIVE,(void*)&keep_alive,sizeof(keep_alive)) == SOCKET_ERROR)
		return -1;

	if(setsockopt(socket,SOL_TCP,TCP_KEEPIDLE,(void*)&keep_idle,sizeof(keep_idle)) == SOCKET_ERROR)
		return -2;

	if(setsockopt(socket,SOL_TCP,TCP_KEEPINTVL,(void*)&keep_interval,sizeof(keep_interval)) == SOCKET_ERROR)
		return -3;

	if(setsockopt(socket,SOL_TCP,TCP_KEEPCNT,(void*)&keep_count,sizeof(keep_count)) == SOCKET_ERROR)
		return -4;

#endif
}

int cm_socket_get_error()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

bool cm_socket_haserror()
{
#ifdef WIN32
	int err = WSAGetLastError();
	if(err == WSAEWOULDBLOCK)
		return false;
#else
	int err = errno;
	if(err == EINPROGRESS || err == EAGAIN)
		return false;
#endif

return true;
}

namespace tcp{
	int cm_socket_createsocket(SOCKET &sock)
	{
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(sock == INVALID_SOCKET){
			return -1;
		}
		return 0;
	}
	int cm_socket_bind(SOCKET socket,const char *ip, uint16_t port)
	{
		sockaddr_in addr;

		memset(&addr, 0, sizeof(sockaddr_in));

		addr.sin_family = AF_INET;
		addr.sin_port   = htons(port);
		S_ADDR(addr)    = inet_addr(ip);

		if(SOCKET_ERROR == bind(socket, (sockaddr*)&addr, sizeof(sockaddr)))
		{
			return -1;
		}
		return 0;
	}
	int cm_socket_listen(SOCKET socket, int backlog)
	{
		if(SOCKET_ERROR == listen(socket, backlog))
		{
			return -1;
		}
		return 0;
	}
	int cm_socket_accept(SOCKET socket, sockaddr_in *client_addr)
	{
#ifdef WIN32
		int len = sizeof(sockaddr_in);
#else
		uint32_t len = sizeof(sockaddr_in);
#endif
		SOCKET socket_client = accept(socket, (sockaddr*)client_addr, &len);
		if(socket_client == SOCKET_ERROR)
		{
			return -1;
		}

		return socket_client;
	}
	int cm_socket_connect(SOCKET socket, const char *ip, uint16_t port)
	{
		unsigned long serveraddr = inet_addr(ip);
		if(serveraddr == INADDR_NONE)
		{
			return -1;
		}

		sockaddr_in addr_in; 
		memset((void *)&addr_in, 0, sizeof(addr_in)); 
		addr_in.sin_family = AF_INET; 
		addr_in.sin_port = htons(port); 
		addr_in.sin_addr.s_addr = serveraddr; 

		long second = 2;

		if(connect(socket, (sockaddr *)&addr_in, sizeof(addr_in)) == SOCKET_ERROR)
		{
			if(cm_socket_haserror())
			{
				return -1;
			}

			timeval timeout;
			timeout.tv_sec = second;
			timeout.tv_usec = 0;

			fd_set writeset, exceptset; 
			FD_ZERO(&writeset); 
			FD_ZERO(&exceptset); 
			FD_SET(socket, &writeset); 
			FD_SET(socket, &exceptset); 

			int ret = select(FD_SETSIZE, NULL, &writeset, &exceptset, &timeout); 
			if(ret == 0 || ret < 0) 
			{
				return -1;
			}
			else 
			{
				ret = FD_ISSET(socket, &exceptset);
				if(ret)
				{
					return -1;
				}
			}
			return 0;
		}

		return 0;
	}
	int cm_socket_send(SOCKET socket, const char *buffer, uint32_t size)
	{
		if(buffer == NULL || socket == INVALID_SOCKET)
			return -1;

		int sendsize = send(socket, buffer, size, 0);
		if(sendsize > 0) 
		{
			return sendsize;
		}
		else
		{
			if(cm_socket_haserror())
			{
				return -1;
			}
		}

		return 0;
	}
	int cm_socket_recv(SOCKET socket, char *buffer, uint32_t size)
	{
		if(buffer == NULL || socket == INVALID_SOCKET)
			return -1;

		int recvsize = recv(socket, buffer, size, 0);
		if(recvsize > 0)
		{
			return recvsize;
		}
		else if(recvsize == 0)
		{
			return recvsize;
		}
		else
		{
			if(cm_socket_haserror())
			{
				return recvsize;
			}
		}

		return 0;
	}
	int cm_socket_recv_select(SOCKET socket, char *buffer, uint32_t size, uint32_t timeout)
	{
		if(buffer == NULL || socket == INVALID_SOCKET)
			return -1;

		timeval timev;
		timev.tv_sec = timeout / 1000;
		timev.tv_usec = 0;

		fd_set readset, exceptset; 
		FD_ZERO(&readset); 
		FD_ZERO(&exceptset); 
		FD_SET(socket, &readset); 
		FD_SET(socket, &exceptset); 

		int ret = select(FD_SETSIZE, &readset, NULL, &exceptset, &timev); 
		if(ret < 0) 
		{
			return -1;
		}
		else if(ret == 0)		//³¬Ê±
		{
			return -2;
		}
		else 
		{
			ret = FD_ISSET(socket, &exceptset);
			if(ret)
			{
				return -1;
			}
			ret = FD_ISSET(socket, &readset);
			if(ret)
			{
				ret = recv(socket, buffer, size, 0);
				return ret;
			}
		}
		return 0;
	}
}
namespace udp{
	int cm_socket_createsocket(SOCKET &sock)
	{
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(sock == INVALID_SOCKET){
			return -1;
		}
		return 0;
	}
}

}
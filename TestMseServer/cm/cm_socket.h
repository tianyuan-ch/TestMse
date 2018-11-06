#ifndef _CM_SOCKET_H_
#define _CM_SOCKET_H_

#include "cm_type.h"
#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
typedef int SOCKET;
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET 0

#endif

namespace cm {

	int cm_socket_init_environment();
	int cm_socket_free_environment();
	int cm_socket_closesocket(SOCKET socket);

	int cm_socket_epoll_setsocket(SOCKET socket);
	int cm_socket_setsocketopt_noblock(SOCKET socket);
	int cm_socket_setkeepalivetime(SOCKET socket,int keep_idle, int keep_interval, int keep_count); 
	int cm_socket_get_error();
	bool cm_socket_haserror();

namespace tcp{
	int cm_socket_createsocket(SOCKET &socket);
	int cm_socket_bind(SOCKET socket,const char *ip, uint16_t port);
	int cm_socket_listen(SOCKET socket, int backlog);
	int cm_socket_accept(SOCKET socket, sockaddr_in *client_addr);
	int cm_socket_connect(SOCKET socket, const char *ip, uint16_t port);
	int cm_socket_send(SOCKET socket, const char *buffer, uint32_t size);
	int cm_socket_recv(SOCKET socket, char *buffer, uint32_t size);
	int cm_socket_recv_select(SOCKET socket, char *buffer, uint32_t size, uint32_t timeout);
}
namespace udp{
	int cm_socket_createsocket(SOCKET &socket);
}

}



#endif
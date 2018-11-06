#pragma once

#ifndef WIN32

#include "../ServerModelInterface.h"
#include "EPollPerSocketContext.h"

#include "cm/cm_type.h"
#include "cm/cm_socket.h"
#include "cm/cm_thread.h"

#include <string>

#define THREADRECVBUFFERSIZE	8000
#define THREADSENDBUFFERSIZE    1024

struct SThreadParam
{
	int  thread_index_;
	char array_recv_buffer_[THREADRECVBUFFERSIZE];
	char array_send_buffer_[THREADSENDBUFFERSIZE];
	void *p_epoll_model_; 
};


class CEPollModel : public CServerModelInterface
{
public:
	CEPollModel(CServerModelUser* pUser);
	~CEPollModel(void);

public:
	virtual int Start(const char* server_address, uint16_t server_port, int num_thread = 0);
	virtual void Stop();
	virtual int  Send(CPerSocketContext *p_socket_context, char* p_buffer, uint32_t len);
	virtual int  Disconnect(CPerSocketContext *socket_context);

	std::string GetIP() {};

private:
	static void* _WorkerThread(void* param);
	int StartWorkerThreads(int num_threads);

	int DoAccept();
	int DoRecv(CPerSocketContext *socket_context, char *buffer);


private:
	friend class CPerSocketContext;
	CServerModelUser* server_model_user_;

private:
	uint16_t port_;
	std::string string_ip_;

	int epoll_fd_;
	SOCKET socket_listen_fd_;


	SThreadParam *p_thread_params;
	CPerSocketContext listen_socket_context;
};

#endif
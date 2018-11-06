#include "EPollModel.h"

#ifndef WIN32

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/sysinfo.h>
#include "../Head.h"

CEPollModel::CEPollModel(CServerModelUser* pUser)
	: server_model_user_(pUser)
{
	epoll_fd_ = -1;
	socket_listen_fd_ = -1;
}
CEPollModel::~CEPollModel(void)
{

}

int CEPollModel::Start(const char* server_address, uint16_t server_port, int num_thread)
{
	int ret;
	struct epoll_event event_add;

	port_ = server_port;
	string_ip_ = server_address;

	int cores = get_nprocs();

	num_thread = (num_thread == 0) ? cores : num_thread;

	do{
		ret = cm::tcp::cm_socket_createsocket(socket_listen_fd_);
		if(ret < 0)
		{
			LOGE(__FILE__, __LINE__, "Start cm_socket_createsocket error %d", cm::cm_socket_get_error());
			break;
		}

		cm::cm_socket_epoll_setsocket(socket_listen_fd_);

		ret = cm::cm_socket_setsocketopt_noblock(socket_listen_fd_);
		if(ret < 0)
		{
			LOGE(__FILE__, __LINE__, "Start cm_socket_setsocketopt_noblock error %d", cm::cm_socket_get_error());
			break;
		}

		ret = cm::tcp::cm_socket_bind(socket_listen_fd_, server_address, server_port);
		if(ret < 0)
		{
			LOGE(__FILE__, __LINE__, "Start cm_socket_bind error %d", cm::cm_socket_get_error());
			break;
		}

		ret = cm::tcp::cm_socket_listen(socket_listen_fd_, 1024);
		if(ret < 0)
		{
			LOGE(__FILE__, __LINE__, "Start cm_socket_listen error %d", cm::cm_socket_get_error());
			break;
		}

		epoll_fd_ = epoll_create(265);
		if(epoll_fd_ == -1)
		{
			LOGE(__FILE__, __LINE__, "Start epoll_create error %d", cm::cm_socket_get_error());
			break;
		}

		listen_socket_context.SetSocket(socket_listen_fd_, NULL);
		event_add.data.ptr = (void*)&listen_socket_context;
		event_add.events  = EPOLLIN | EPOLLET | EPOLLONESHOT; 
		ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_listen_fd_, &event_add);
		if(ret == -1)
		{
			LOGE(__FILE__, __LINE__, "Start epoll_ctl error %d", cm::cm_socket_get_error());
			break;
		}

		//创建线程
		ret = StartWorkerThreads(num_thread);
		if(ret < 0)
		{
			LOGF(__FILE__, __LINE__, "Start StartWorkerThreads error", NULL);
			break;
		}

		return 0;
	}while(false);

	if(socket_listen_fd_ != -1)
	{
		cm::cm_socket_closesocket(socket_listen_fd_);
		socket_listen_fd_ = -1;
	}

	if(epoll_fd_ != -1)
	{
		cm::cm_socket_closesocket(epoll_fd_);
		epoll_fd_ = -1;
	}

	return -1;
}
void CEPollModel::Stop()
{

}
int  CEPollModel::Send(CPerSocketContext *p_socket_context, char* p_buffer, uint32_t len)
{
	int ret;
	ret = cm::tcp::cm_socket_send(p_socket_context->GetSocket(), p_buffer, len);
	return ret;
}
int  CEPollModel::Disconnect(CPerSocketContext *socket_context)
{
	int ret ;
	ret = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, socket_context->GetSocket(), NULL);
	cm::cm_socket_closesocket(socket_context->GetSocket());
	socket_context->SetSocket(0, NULL);
	ret = server_model_user_->CBDisconnect(socket_context);
	

	LOGI(__FILE__, __LINE__,"Disconnect disconnect,current connections %d", ret);

	return 0;
}

void* CEPollModel::_WorkerThread(void* param)
{
	LOGF(__FILE__, __LINE__, "_WorkerThread In", NULL);

	int num_fds, i, ret;
	struct epoll_event events_wait[200];

	SThreadParam *p_thread_params = (SThreadParam*)param;
	CEPollModel *p_epoll_model = (CEPollModel*)p_thread_params->p_epoll_model_;

	while(1)
	{
		num_fds = epoll_wait(p_epoll_model->epoll_fd_, events_wait, 180, -1);
		for(i = 0; i < num_fds; i++)
		{
			if(events_wait[i].data.ptr == (void*)&(p_epoll_model->listen_socket_context))
			{
				ret = p_epoll_model->DoAccept();
				if(ret < 0)
				{
					LOGI(__FILE__, __LINE__, "_WorkerThread DoAccept error %d", ret);
				}
			}
			else if(events_wait[i].events & EPOLLRDHUP)
			{
				LOGI(__FILE__, __LINE__, "_WorkerThread EPOLLRDHUP call", NULL);
				p_epoll_model->Disconnect((CPerSocketContext*)events_wait[i].data.ptr);
			}
			else if(events_wait[i].events & EPOLLERR)
			{
				LOGI(__FILE__, __LINE__, "_WorkerThread EPOLLERR call", NULL);
				p_epoll_model->Disconnect((CPerSocketContext*)events_wait[i].data.ptr);
			}
			else if(events_wait[i].events & EPOLLIN)
			{
				LOGI(__FILE__, __LINE__, "_WorkerThread EPOLLIN call", NULL);
				ret = p_epoll_model->DoRecv((CPerSocketContext*)events_wait[i].data.ptr, p_thread_params->array_recv_buffer_);
				if(ret < 0)
				{
					p_epoll_model->Disconnect((CPerSocketContext*)events_wait[i].data.ptr);
					LOGI(__FILE__, __LINE__, "_WorkerThread DoRecv error %d", ret);
				}
			}
			else
			{
				LOGI(__FILE__, __LINE__, "_WorkerThread else call", NULL);
			}
		}

	}
}

int CEPollModel::StartWorkerThreads(int num_threads)
{
	int ret ,i;

	p_thread_params = new SThreadParam[num_threads];

	cm_pthread_t thread_id;
	for(i = 0; i < num_threads; i++)
	{
		p_thread_params[i].thread_index_  = i;
		p_thread_params[i].p_epoll_model_ = (void*)this;
		ret = cm_pthread_create(&thread_id, 0, _WorkerThread, (void*)&(p_thread_params[i]));
		if(ret != 0)
		{
			return -1;
		}
	}

	return 0;
}

int CEPollModel::DoAccept()
{
	int ret;
	struct epoll_event event_add;
	SOCKET socket_connect_fd;
	sockaddr_in addr_client;

	socket_connect_fd = cm::tcp::cm_socket_accept(socket_listen_fd_, &addr_client);
	if(socket_connect_fd < 0)
	{
		LOGE(__FILE__, __LINE__, "DoAccept cm_socket_accept error %d", cm::cm_socket_get_error());
		return -1;
	}
	cm::cm_socket_setsocketopt_noblock(socket_connect_fd);
	cm::cm_socket_setkeepalivetime(socket_connect_fd, 5, 5, 1);

	event_add.data.ptr = (void*)&listen_socket_context;
	event_add.events  = EPOLLIN | EPOLLET | EPOLLONESHOT; 
	ret = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, socket_listen_fd_, &event_add);
	if(ret == -1)
	{
		LOGE(__FILE__, __LINE__, "DoAccept accept epoll_ctl error %d", cm::cm_socket_get_error());
		return -1;
	}

	//获得对象
	CPerSocketContext *new_socket_context;
	ret = server_model_user_->CBAccept(&new_socket_context);
	if(ret < 0)
	{
		LOGE(__FILE__, __LINE__, "DoAccept CBAccept error %d", ret);
		cm::cm_socket_closesocket(socket_connect_fd);
		return -1;
	}
	new_socket_context->SetSocket(socket_connect_fd, &addr_client);

	int num = ret;
	LOGI(__FILE__, __LINE__,"new connect,current connections %d, ip %s, port %d", ret, inet_ntoa(addr_client.sin_addr), addr_client.sin_port);


	//加入epoll
	event_add.events   = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLRDHUP;
	event_add.data.ptr  = new_socket_context;
	ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_connect_fd, &event_add);
	if(ret == -1)
	{
		LOGE(__FILE__, __LINE__, "DoAccept epoll_ctl error %d", cm::cm_socket_get_error());
		cm::cm_socket_closesocket(new_socket_context->GetSocket());
		server_model_user_->CBDisconnect(new_socket_context);

		LOGI(__FILE__, __LINE__,"DoAccept disconenct,current connections %d", num - 1);

		return -1;
	}


	return 0;
}

int CEPollModel::DoRecv(CPerSocketContext *socket_context, char *buffer)
{

	int ret;
	struct epoll_event event_add;
	uint32_t len = THREADSENDBUFFERSIZE;

	ret = cm::tcp::cm_socket_recv(socket_context->GetSocket(), buffer, THREADRECVBUFFERSIZE);
	//等于0 没数据也算锁
	if(ret <= 0)
	{
		LOGE(__FILE__, __LINE__, "DoRecv cm_socket_recv ret %d", ret);
		return -1;
	}

	ret = server_model_user_->CBRecv(socket_context, buffer, ret);
	if(ret < 0)
	{
		LOGE(__FILE__, __LINE__, "DoRecv CBRecv ret %d, buffer %s", ret, buffer);
		return -1;
	}

	event_add.events   = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLERR | EPOLLRDHUP;
	event_add.data.ptr = (void*)socket_context;
	ret = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, socket_context->GetSocket(), &event_add);
	if(ret == -1)
	{
		LOGE(__FILE__, __LINE__, "DoRecv epoll_ctl error %d", cm::cm_socket_get_error());
		return -1;
	}

	return 0;
}
#endif
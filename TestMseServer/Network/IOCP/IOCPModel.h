
#pragma once


// winsock 2 的头文件和库

#include <vector>
#include <string>
#include <assert.h>

#include "..\ServerModelInterface.h"
#include "PerSocketContext.h"

class CIOCPModel;

typedef struct _tagThreadParams_WORKER
{
	CIOCPModel* pIOCPModel;                                   // 类指针，用于调用类中的函数
	int         nThreadNo;                                    // 线程编号
} THREADPARAMS_WORKER,*PTHREADPARAM_WORKER; 


class CIOCPModel : public CServerModelInterface
{
public:
	CIOCPModel(CServerModelUser* pUser);
	~CIOCPModel(void);
public:
	// 启动服务器
	virtual int  Start(const char* server_address, uint16_t server_port, int num_thread = 0);
	// 停止服务器
	virtual void Stop();
	// 发送消息
	virtual int Send(CPerSocketContext *pSocketContext, char* pBuffer, uint32_t uLen);
	// 断开连接
	virtual int Disconnect(CPerSocketContext *pSocketContext);

	virtual std::string GetIP(){ return m_strIP; }
private:
	int SetIdleIoContext(CPerSocketContext* pSocketContext, _PER_IO_CONTEXT* pContext);

private:
	// 线程函数，为IOCP请求服务的工作者线程
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	bool LoadSocketLib();
	void UnloadSocketLib() { WSACleanup(); }
private:
	friend CPerSocketContext;
	// 初始化IOCP
	bool _InitializeIOCP();
	// 初始化Socket
	bool _InitializeListenSocket(const char* pServerAddress);
	// 最后释放资源
	void _DeInitialize();
	// 将句柄绑定到完成端口中
	bool _AssociateWithIOCP( CPerSocketContext *pContext);

	// 投递Accept请求
	bool _PostAccept(PER_IO_CONTEXT* pAcceptIoContext); 
	// 投递接收数据请求
	bool _PostRecv(PER_IO_CONTEXT* pIoContext);
	// 投递发送数据
	bool _PostSend(PER_IO_CONTEXT* pIoContext, DWORD dwSend);
	// 在有客户端连入的时候，进行处理
	bool _DoAccpet( CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered);
	// 在有接收的数据到达的时候，进行处理
	bool _DoRecv(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered);
	// 发送完成，进行处理
	bool _DoSend(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered);

	// 判断客户端Socket是否已经断开
	bool _IsSocketAlive(SOCKET s);
	// 处理完成端口上的错误
	bool HandleError( CPerSocketContext *pContext,const DWORD& dwErr );

	// 获得本机的处理器数量
	int _GetNoOfProcessors();
	// 打印消息
	void _ShowMessage( const std::string szFormat,...) const;

private:
	HANDLE                      m_hShutdownEvent;              // 用来通知线程系统退出的事件，为了能够更好的退出线程
	HANDLE                      m_hIOCompletionPort;           // 完成端口的句柄
	HANDLE*                     m_phWorkerThreads;             // 工作者线程的句柄指针
	int		                    m_nThreads;                    // 生成的线程数量
	std::string                 m_strIP;                       // 服务器端的IP地址
	int                         m_nPort;                       // 服务器端的监听端口  
	CPerSocketContext*          m_pListenContext;              // 用于监听的Socket的Context信息
	LPFN_ACCEPTEX				m_lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
	LPFN_GETACCEPTEXSOCKADDRS	m_lpfnGetAcceptExSockAddrs; 

private:
	CServerModelUser* m_pIOCPUser;
};



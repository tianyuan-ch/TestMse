
#include "IOCPModel.h"

#include "cm\cm_socket.h"

#include <WS2tcpip.h>

#pragma comment(lib,"ws2_32.lib")

// 每一个处理器上产生多少个线程(为了最大限度的提升服务器性能，详见配套文档)
#define WORKER_THREADS_PER_PROCESSOR 1
// 同时投递的Accept请求的数量(这个要根据实际的情况灵活设置)
#define MAX_POST_ACCEPT              5
// 传递给Worker线程的退出信号
#define EXIT_CODE                    NULL


// 释放指针和句柄资源的宏

// 释放指针宏
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// 释放句柄宏
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// 释放Socket宏
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}



CIOCPModel::CIOCPModel(CServerModelUser* pUser):
							m_nThreads(0),
							m_hShutdownEvent(NULL),
							m_hIOCompletionPort(NULL),
							m_phWorkerThreads(NULL),
							m_strIP(DEFAULT_IP),
							m_nPort(DEFAULT_PORT),
							m_lpfnAcceptEx( NULL ),
							m_pListenContext( NULL )
{
	m_pIOCPUser = pUser;
}
CIOCPModel::~CIOCPModel(void)
{
	// 确保资源彻底释放
	this->Stop();
}

int CIOCPModel::Start(const char* server_address, uint16_t server_port, int num_thread)
{
	m_nPort = server_port;

	LoadSocketLib();
	// 建立系统退出的事件通知
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// 初始化IOCP
	if (false == _InitializeIOCP())
	{
		this->_ShowMessage("初始化IOCP失败！\n");
		return -1;
	}
	else
	{
		this->_ShowMessage("IOCP初始化完毕.\n");
	}

	// 初始化Socket
	if( false==_InitializeListenSocket(server_address) )
	{
		this->_ShowMessage("Listen Socket初始化失败！\n");
		this->_DeInitialize();
		return -1;
	}
	else
	{
		this->_ShowMessage("IP:%s Port:%d Listen Socket初始化完毕.", m_strIP.c_str(), m_nPort);
	}

	this->_ShowMessage("系统准备就绪，等候连接....\n");

	return 0;
}
void CIOCPModel::Stop()
{
	if(m_pListenContext!=NULL && m_pListenContext->GetSocket() !=INVALID_SOCKET)
	{
		SetEvent(m_hShutdownEvent);
		for (int i = 0; i < m_nThreads; i++)
		{
			// 通知所有的完成端口操作退出
			PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
		}
		// 等待所有的客户端资源退出
		WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);
		// 释放其他资源
		this->_DeInitialize();
		this->_ShowMessage("停止监听\n");
	}	
}

int CIOCPModel::Send(CPerSocketContext *pSocketContext, char* pBuffer, uint32_t uLen)
{
	//PER_IO_CONTEXT* pIdleIoContext = pSocketContext->GetIdleIoContext(SEND_POSTED);
	//if(pIdleIoContext == NULL)
	//	return -1;
	//pIdleIoContext->m_sockAccept = pSocketContext->GetSocket();

	//WSABUF *p_wbuf   = &pIdleIoContext->m_wsaBuf;
	//memcpy(p_wbuf->buf, pBuffer, uLen);
	//p_wbuf->len = uLen;
	//if(!this->_PostSend(pIdleIoContext, uLen))
	//{
	//	SetIdleIoContext(pSocketContext, pIdleIoContext);
	//	return -1;
	//}

	return cm::tcp::cm_socket_send(pSocketContext->GetSocket(), pBuffer, uLen);
	
	return 0;
}

int CIOCPModel::Disconnect(CPerSocketContext *pSocketContext)
{
	//设置退出标记，设置WSARECV WSASEND超时，超时后不重新投递，调用closesocket
	return 0;
}

DWORD WINAPI CIOCPModel::_WorkerThread(LPVOID lpParam)
{    
	THREADPARAMS_WORKER* pParam = (THREADPARAMS_WORKER*)lpParam;
	CIOCPModel* pIOCPModel = (CIOCPModel*)pParam->pIOCPModel;
	int nThreadNo = (int)pParam->nThreadNo;

	pIOCPModel->_ShowMessage("工作者线程启动，ID: %d.\r\n",nThreadNo);

	OVERLAPPED           *pOverlapped = NULL;
	CPerSocketContext    *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;
	// int nRet;
	// 循环处理请求，知道接收到Shutdown信息为止
	while (WAIT_OBJECT_0 != WaitForSingleObject(pIOCPModel->m_hShutdownEvent, 0))
	{ 
		BOOL bReturn = GetQueuedCompletionStatus(
			pIOCPModel->m_hIOCompletionPort,
			&dwBytesTransfered,
			(PULONG_PTR)&pSocketContext,
			&pOverlapped,
			INFINITE);

		// 如果收到的是退出标志，则直接退出
		if (EXIT_CODE==(DWORD)pSocketContext)
		{
			break;
		}

		// 判断是否出现了错误
		if( !bReturn )  
		{  
			DWORD dwErr = GetLastError();

			// 显示一下提示信息
			if( !pIOCPModel->HandleError( pSocketContext,dwErr ) )
			{
				break;
			}

			PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  
			
			if(RECV_POSTED == pIoContext->m_OpType || SEND_POSTED == pIoContext->m_OpType)
			{
				PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  
				pIOCPModel->SetIdleIoContext(pSocketContext, pIoContext);
			}
			else if(ACCEPT_POSTED == pIoContext->m_OpType)
			{
				RELEASE_SOCKET(pIoContext->m_sockAccept);
				pIOCPModel->_PostAccept(pIoContext);
			}
			continue;  
		}  
		else  
		{  	
			// 读取传入的参数
			PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  

			// 判断是否有客户端断开了
			if((0 == dwBytesTransfered) && (RECV_POSTED==pIoContext->m_OpType || SEND_POSTED==pIoContext->m_OpType))  
			{  

				pIOCPModel->SetIdleIoContext(pSocketContext, pIoContext);

 				continue;  
			}  

			switch( pIoContext->m_OpType )  
			{
			case ACCEPT_POSTED:
				{
					pIOCPModel->_DoAccpet(pSocketContext, pIoContext, dwBytesTransfered);						
				}
				break;
			case RECV_POSTED:
				{
					pIOCPModel->_DoRecv(pSocketContext, pIoContext, dwBytesTransfered);
				}
				break;
			case SEND_POSTED:
				{
					pIOCPModel->_DoSend(pSocketContext, pIoContext, dwBytesTransfered);
				}
				break;
			default:
				break;
			} //switch
		}//if
	}//while

	RELEASE(lpParam);	

	return 0;
}

bool CIOCPModel::LoadSocketLib()
{    
	WSADATA wsaData;
	int nResult;
	nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	// 错误(一般都不可能出现)
	if (NO_ERROR != nResult)
	{
		this->_ShowMessage("初始化WinSock 2.2失败！\n");
		return false; 
	}

	return true;
}
bool CIOCPModel::_InitializeIOCP()
{
	// 建立第一个完成端口
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );

	if ( NULL == m_hIOCompletionPort)
	{
		this->_ShowMessage("建立完成端口失败！错误代码: %d!\n", WSAGetLastError());
		return false;
	}

	// 根据本机中的处理器数量，建立对应的线程数
	m_nThreads = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();

	
	// 为工作者线程初始化句柄
	m_phWorkerThreads = new HANDLE[m_nThreads];
	
	// 根据计算出来的数量建立工作者线程
	DWORD nThreadID;
	for (int i = 0; i < m_nThreads; i++)
	{
		THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
		pThreadParams->pIOCPModel = this;
		pThreadParams->nThreadNo  = i+1;
		m_phWorkerThreads[i] = ::CreateThread(0, 0, _WorkerThread, (void *)pThreadParams, 0, &nThreadID);
	}

	//TRACE(" 建立 _WorkerThread %d 个.\n", m_nThreads );

	return true;
}

bool CIOCPModel::_InitializeListenSocket(const char* pServerAddress)
{
	// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
	GUID GuidAcceptEx = WSAID_ACCEPTEX;  
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS; 

	struct sockaddr_in ServerAddress;

	// 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == socket) 
	{
		this->_ShowMessage("初始化Socket失败，错误代码: %d.\n", WSAGetLastError());
		return false;
	}
	else
	{
		//TRACE("WSASocket() 完成.\n");
	}

	// 填充地址信息
	// GetLocalIP();
	m_strIP = pServerAddress;
	ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
	ServerAddress.sin_family = AF_INET;                    
	inet_pton(AF_INET, m_strIP.c_str(), (void*)&ServerAddress.sin_addr);
	//ServerAddress.sin_addr.s_addr = inet_addr(m_strIP.c_str());         
	ServerAddress.sin_port = htons(m_nPort);

	m_pListenContext = new CPerSocketContext;
	m_pListenContext->InitSocket(socket, &ServerAddress);

	// 将Listen Socket绑定至完成端口中
	if( NULL== CreateIoCompletionPort((HANDLE)socket, m_hIOCompletionPort,(DWORD)m_pListenContext, 0))  
	{  
		this->_ShowMessage("绑定 Listen Socket至完成端口失败！错误代码: %d/n", WSAGetLastError());
		RELEASE(m_pListenContext);
		return false;
	}
	else
	{
		//TRACE("Listen Socket绑定完成端口 完成.\n");
	}                      

	if (SOCKET_ERROR == bind(socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress))) 
	{
		int lastError = GetLastError();
		this->_ShowMessage("bind()函数执行错误.\n");
		RELEASE(m_pListenContext);
		return false;
	}
	else
	{
		//TRACE("bind() 完成.\n");
	}

	if (SOCKET_ERROR == listen(socket,SOMAXCONN))
	{
		this->_ShowMessage("Listen()函数执行出现错误.\n");
		RELEASE(m_pListenContext);
		return false;
	}
	else
	{
		//TRACE("Listen() 完成.\n");
	}

	// 获取AcceptEx函数指针
	DWORD dwBytes = 0;  
	if(SOCKET_ERROR == WSAIoctl(
		m_pListenContext->GetSocket(), 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidAcceptEx, 
		sizeof(GuidAcceptEx), 
		&m_lpfnAcceptEx, 
		sizeof(m_lpfnAcceptEx), 
		&dwBytes, 
		NULL, 
		NULL))  
	{  
		this->_ShowMessage("WSAIoctl 未能获取AcceptEx函数指针。错误代码: %d\n", WSAGetLastError()); 
		this->_DeInitialize();
		return false;  
	}  

	// 获取GetAcceptExSockAddrs函数指针
	if(SOCKET_ERROR == WSAIoctl(
		m_pListenContext->GetSocket(), 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		&GuidGetAcceptExSockAddrs,
		sizeof(GuidGetAcceptExSockAddrs), 
		&m_lpfnGetAcceptExSockAddrs, 
		sizeof(m_lpfnGetAcceptExSockAddrs),   
		&dwBytes, 
		NULL, 
		NULL))  
	{  
		this->_ShowMessage("WSAIoctl 未能获取GuidGetAcceptExSockAddrs函数指针。错误代码: %d\n", WSAGetLastError());  
		this->_DeInitialize();
		return false; 
	}  


	// 为AcceptEx 准备参数，然后投递AcceptEx I/O请求
	for( int i=0;i<MAX_POST_ACCEPT;i++ )
	{
		// 新建一个IO_CONTEXT
		PER_IO_CONTEXT* pAcceptIoContext = m_pListenContext->GetNewIoContext();

		if( false==this->_PostAccept( pAcceptIoContext ) )
		{
			m_pListenContext->RemoveContext(pAcceptIoContext);
			return false;
		}
	}

	this->_ShowMessage("投递%d个AcceptEx请求完毕\r\n", MAX_POST_ACCEPT);

	return true;
}
void CIOCPModel::_DeInitialize()
{

	// 关闭系统退出事件句柄
	RELEASE_HANDLE(m_hShutdownEvent);

	// 释放工作者线程句柄指针
	for( int i=0;i<m_nThreads;i++ )
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}
	
	RELEASE(m_phWorkerThreads);

	// 关闭IOCP句柄
	RELEASE_HANDLE(m_hIOCompletionPort);

	// 关闭监听Socket
	RELEASE(m_pListenContext);

	this->_ShowMessage("释放资源完毕.\n");
}
bool CIOCPModel::_AssociateWithIOCP( CPerSocketContext *pContext )
{
	// 将用于和客户端通信的SOCKET绑定到完成端口中
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->GetSocket(), m_hIOCompletionPort, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		this->_ShowMessage(("执行CreateIoCompletionPort()出现错误.错误代码：%d"),GetLastError());
		return false;
	}

	return true;
}

bool CIOCPModel::_PostAccept( PER_IO_CONTEXT* pAcceptIoContext )
{
	assert(INVALID_SOCKET != m_pListenContext->GetSocket());

	// 准备参数
	DWORD dwBytes = 0;  
	pAcceptIoContext->m_OpType = ACCEPT_POSTED;  
	WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;
	
	// 为以后新连入的客户端先准备好Socket( 这个是与传统accept最大的区别 ) 
	pAcceptIoContext->m_sockAccept  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  
	if(INVALID_SOCKET == pAcceptIoContext->m_sockAccept )  
	{  
		_ShowMessage("创建用于Accept的Socket失败！错误代码: %d", WSAGetLastError()); 
		return false;  
	} 

	cm::cm_socket_setkeepalivetime(pAcceptIoContext->m_sockAccept, 5, 5, 1);

	// 投递AcceptEx
	if(FALSE == m_lpfnAcceptEx( m_pListenContext->GetSocket(), pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),   
								sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))  
	{  
		if(WSA_IO_PENDING != WSAGetLastError())  
		{  
			_ShowMessage("投递 AcceptEx 请求失败，错误代码: %d", WSAGetLastError());  
			return false;  
		}  
	} 

	return true;
}
bool CIOCPModel::_DoAccpet(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered)
{
	int nRet;
	char szSendBuffer[1024];
	uint32_t uLen = 1024;
	int nNum;
	SOCKADDR_IN* ClientAddr = NULL;
	SOCKADDR_IN* LocalAddr = NULL;
	int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);  

	// m_lpfnGetAcceptExSockAddrs connect + recv
	this->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16)*2),  
		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);  

	CPerSocketContext* pNewSocketContext;
	nRet = m_pIOCPUser->CBAccept(&pNewSocketContext);
	if(nRet < 0)
	{
		this->_ShowMessage("CBAccept return nRet %d.\r\n", nRet);
		RELEASE_SOCKET(pIoContext->m_sockAccept);
	}
	else
	{
		nNum = nRet;
		pNewSocketContext->InitSocket(pIoContext->m_sockAccept, ClientAddr);

		nRet = m_pIOCPUser->CBRecv(pNewSocketContext, pIoContext->m_wsaBuf.buf, dwBytesTransfered);
		if(nRet < 0)
		{
			pNewSocketContext->CloseSocket();
			m_pIOCPUser->CBDisconnect(pNewSocketContext);
		}
		else
		{
			if(false == this->_AssociateWithIOCP(pNewSocketContext))
			{
				pNewSocketContext->CloseSocket();
				m_pIOCPUser->CBDisconnect(pNewSocketContext);
			}
			else
			{
				PER_IO_CONTEXT* pNewIoContext = pNewSocketContext->GetIdleIoContext(RECV_POSTED);
				pNewIoContext->m_sockAccept   = pNewSocketContext->GetSocket();
				if(false == this->_PostRecv(pNewIoContext))
				{
					pNewSocketContext->CloseSocket();
					SetIdleIoContext(pNewSocketContext, pIoContext);
				}
				else
				{
					char dst[16];
					inet_ntop(AF_INET, (void*)&ClientAddr->sin_addr, dst, 16);
					this->_ShowMessage("客户端 %s:%d 连入.客户端数%d\r\n", dst, ntohs(ClientAddr->sin_port), nNum);
				}
			}
		}
	}

	return this->_PostAccept(pIoContext); 	
}
bool CIOCPModel::_PostRecv(PER_IO_CONTEXT* pIoContext)
{
	// 初始化变量
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	// 初始化完成后，，投递WSARecv请求
	int nBytesRecv = WSARecv( pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		this->_ShowMessage("投递第一个WSARecv失败！");
		return false;
	}
	return true;
}
bool CIOCPModel::_DoRecv(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered)
{
	int nRet;

	WSABUF *p_wbuf = &pIoContext->m_wsaBuf;

	nRet = m_pIOCPUser->CBRecv(pSocketContext, p_wbuf->buf, dwBytesTransfered);
	if(nRet < 0)
	{
		pSocketContext->CloseSocket();
		SetIdleIoContext(pSocketContext, pIoContext);
		return false;
	}
	if(!_PostRecv(pIoContext))
	{
		pSocketContext->CloseSocket();

		SetIdleIoContext(pSocketContext, pIoContext);

		return false;
	}

	return true;;
}
bool CIOCPModel::_PostSend(PER_IO_CONTEXT* pIoContext, DWORD dwSend)
{
	DWORD dwFlags = 0;
	DWORD dwBytes = dwSend;
	WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;
	
	int nBytesSend = WSASend(pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, dwFlags, p_ol, NULL);
	if ((SOCKET_ERROR == nBytesSend) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		int nRet = WSAGetLastError();
		this->_ShowMessage("投递第一个WSASend失败！");
		return false;
	}

	return true;
}
bool CIOCPModel::_DoSend(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered)
{
	//发送字节数不够

	m_pIOCPUser->CBSend(pSocketContext, dwBytesTransfered);

	SetIdleIoContext(pSocketContext, pIoContext);

	return true;
}

bool CIOCPModel::_IsSocketAlive(SOCKET s)
{
	int nByteSent=send(s,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}
bool CIOCPModel::HandleError( CPerSocketContext *pContext,const DWORD& dwErr )
{
	// 如果是超时了，就再继续等吧  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// 确认客户端是否还活着...
		if( !_IsSocketAlive( pContext->GetSocket()) )
		{
			this->_ShowMessage("检测到客户端异常退出！");
			return true;
		}
		else
		{
			this->_ShowMessage("网络操作超时！重试中...");
			return true;
		}
	}  

	// 可能是客户端异常退出了
	else if( ERROR_NETNAME_DELETED==dwErr )
	{
		this->_ShowMessage("检测到客户端异常退出！");
		return true;
	}

	else
	{
		this->_ShowMessage("完成端口操作出现错误，线程退出。错误代码：%d",dwErr );
		return false;
	}
}

int CIOCPModel::_GetNoOfProcessors()
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);

	return si.dwNumberOfProcessors;
}
void CIOCPModel::_ShowMessage(const std::string szFormat,...) const
{
	// 根据传入的参数格式化字符串
	std::string strMessage;
	va_list arglist;

	va_start(arglist, szFormat);

	int num_of_chars = _vscprintf(szFormat.c_str(), arglist);  
	if (num_of_chars > strMessage.capacity()){  
		strMessage.resize(num_of_chars + 1);  
	}

	vsprintf_s((char *)strMessage.c_str(), sizeof(strMessage.c_str()), szFormat.c_str(), arglist);
	//vsprintf((char *) strMessage.c_str(), szFormat.c_str(), arglist);

	printf(strMessage.c_str());

	va_end(arglist);
}

int CIOCPModel::SetIdleIoContext(CPerSocketContext* pSocketContext, _PER_IO_CONTEXT* pContext)
{
	int nCount = pSocketContext->SetIdleIoContext(pContext);
	if(nCount == 0)
	{
		char dst[16];
		inet_ntop(AF_INET, (void*)&pSocketContext->m_ClientAddr.sin_addr, dst, 16);
		this->_ShowMessage("客户端 %s:%d 断开.", dst, ntohs(pSocketContext->m_ClientAddr.sin_port));
		pSocketContext->CloseSocket();
		//应用计数为0时，调用
		int nRet = m_pIOCPUser->CBDisconnect(pSocketContext);
		this->_ShowMessage("剩余连接%d\r\n", nRet);
	}
	return nCount;
}
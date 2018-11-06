
#include "IOCPModel.h"

#include "cm\cm_socket.h"

#include <WS2tcpip.h>

#pragma comment(lib,"ws2_32.lib")

// ÿһ���������ϲ������ٸ��߳�(Ϊ������޶ȵ��������������ܣ���������ĵ�)
#define WORKER_THREADS_PER_PROCESSOR 1
// ͬʱͶ�ݵ�Accept���������(���Ҫ����ʵ�ʵ�����������)
#define MAX_POST_ACCEPT              5
// ���ݸ�Worker�̵߳��˳��ź�
#define EXIT_CODE                    NULL


// �ͷ�ָ��;����Դ�ĺ�

// �ͷ�ָ���
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// �ͷž����
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// �ͷ�Socket��
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
	// ȷ����Դ�����ͷ�
	this->Stop();
}

int CIOCPModel::Start(const char* server_address, uint16_t server_port, int num_thread)
{
	m_nPort = server_port;

	LoadSocketLib();
	// ����ϵͳ�˳����¼�֪ͨ
	m_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	// ��ʼ��IOCP
	if (false == _InitializeIOCP())
	{
		this->_ShowMessage("��ʼ��IOCPʧ�ܣ�\n");
		return -1;
	}
	else
	{
		this->_ShowMessage("IOCP��ʼ�����.\n");
	}

	// ��ʼ��Socket
	if( false==_InitializeListenSocket(server_address) )
	{
		this->_ShowMessage("Listen Socket��ʼ��ʧ�ܣ�\n");
		this->_DeInitialize();
		return -1;
	}
	else
	{
		this->_ShowMessage("IP:%s Port:%d Listen Socket��ʼ�����.", m_strIP.c_str(), m_nPort);
	}

	this->_ShowMessage("ϵͳ׼���������Ⱥ�����....\n");

	return 0;
}
void CIOCPModel::Stop()
{
	if(m_pListenContext!=NULL && m_pListenContext->GetSocket() !=INVALID_SOCKET)
	{
		SetEvent(m_hShutdownEvent);
		for (int i = 0; i < m_nThreads; i++)
		{
			// ֪ͨ���е���ɶ˿ڲ����˳�
			PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE, NULL);
		}
		// �ȴ����еĿͻ�����Դ�˳�
		WaitForMultipleObjects(m_nThreads, m_phWorkerThreads, TRUE, INFINITE);
		// �ͷ�������Դ
		this->_DeInitialize();
		this->_ShowMessage("ֹͣ����\n");
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
	//�����˳���ǣ�����WSARECV WSASEND��ʱ����ʱ������Ͷ�ݣ�����closesocket
	return 0;
}

DWORD WINAPI CIOCPModel::_WorkerThread(LPVOID lpParam)
{    
	THREADPARAMS_WORKER* pParam = (THREADPARAMS_WORKER*)lpParam;
	CIOCPModel* pIOCPModel = (CIOCPModel*)pParam->pIOCPModel;
	int nThreadNo = (int)pParam->nThreadNo;

	pIOCPModel->_ShowMessage("�������߳�������ID: %d.\r\n",nThreadNo);

	OVERLAPPED           *pOverlapped = NULL;
	CPerSocketContext    *pSocketContext = NULL;
	DWORD                dwBytesTransfered = 0;
	// int nRet;
	// ѭ����������֪�����յ�Shutdown��ϢΪֹ
	while (WAIT_OBJECT_0 != WaitForSingleObject(pIOCPModel->m_hShutdownEvent, 0))
	{ 
		BOOL bReturn = GetQueuedCompletionStatus(
			pIOCPModel->m_hIOCompletionPort,
			&dwBytesTransfered,
			(PULONG_PTR)&pSocketContext,
			&pOverlapped,
			INFINITE);

		// ����յ������˳���־����ֱ���˳�
		if (EXIT_CODE==(DWORD)pSocketContext)
		{
			break;
		}

		// �ж��Ƿ�����˴���
		if( !bReturn )  
		{  
			DWORD dwErr = GetLastError();

			// ��ʾһ����ʾ��Ϣ
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
			// ��ȡ����Ĳ���
			PER_IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, PER_IO_CONTEXT, m_Overlapped);  

			// �ж��Ƿ��пͻ��˶Ͽ���
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
	// ����(һ�㶼�����ܳ���)
	if (NO_ERROR != nResult)
	{
		this->_ShowMessage("��ʼ��WinSock 2.2ʧ�ܣ�\n");
		return false; 
	}

	return true;
}
bool CIOCPModel::_InitializeIOCP()
{
	// ������һ����ɶ˿�
	m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );

	if ( NULL == m_hIOCompletionPort)
	{
		this->_ShowMessage("������ɶ˿�ʧ�ܣ��������: %d!\n", WSAGetLastError());
		return false;
	}

	// ���ݱ����еĴ�����������������Ӧ���߳���
	m_nThreads = WORKER_THREADS_PER_PROCESSOR * _GetNoOfProcessors();

	
	// Ϊ�������̳߳�ʼ�����
	m_phWorkerThreads = new HANDLE[m_nThreads];
	
	// ���ݼ�����������������������߳�
	DWORD nThreadID;
	for (int i = 0; i < m_nThreads; i++)
	{
		THREADPARAMS_WORKER* pThreadParams = new THREADPARAMS_WORKER;
		pThreadParams->pIOCPModel = this;
		pThreadParams->nThreadNo  = i+1;
		m_phWorkerThreads[i] = ::CreateThread(0, 0, _WorkerThread, (void *)pThreadParams, 0, &nThreadID);
	}

	//TRACE(" ���� _WorkerThread %d ��.\n", m_nThreads );

	return true;
}

bool CIOCPModel::_InitializeListenSocket(const char* pServerAddress)
{
	// AcceptEx �� GetAcceptExSockaddrs ��GUID�����ڵ�������ָ��
	GUID GuidAcceptEx = WSAID_ACCEPTEX;  
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS; 

	struct sockaddr_in ServerAddress;

	// ��Ҫʹ���ص�IO�������ʹ��WSASocket������Socket���ſ���֧���ص�IO����
	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == socket) 
	{
		this->_ShowMessage("��ʼ��Socketʧ�ܣ��������: %d.\n", WSAGetLastError());
		return false;
	}
	else
	{
		//TRACE("WSASocket() ���.\n");
	}

	// ����ַ��Ϣ
	// GetLocalIP();
	m_strIP = pServerAddress;
	ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
	ServerAddress.sin_family = AF_INET;                    
	inet_pton(AF_INET, m_strIP.c_str(), (void*)&ServerAddress.sin_addr);
	//ServerAddress.sin_addr.s_addr = inet_addr(m_strIP.c_str());         
	ServerAddress.sin_port = htons(m_nPort);

	m_pListenContext = new CPerSocketContext;
	m_pListenContext->InitSocket(socket, &ServerAddress);

	// ��Listen Socket������ɶ˿���
	if( NULL== CreateIoCompletionPort((HANDLE)socket, m_hIOCompletionPort,(DWORD)m_pListenContext, 0))  
	{  
		this->_ShowMessage("�� Listen Socket����ɶ˿�ʧ�ܣ��������: %d/n", WSAGetLastError());
		RELEASE(m_pListenContext);
		return false;
	}
	else
	{
		//TRACE("Listen Socket����ɶ˿� ���.\n");
	}                      

	if (SOCKET_ERROR == bind(socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress))) 
	{
		int lastError = GetLastError();
		this->_ShowMessage("bind()����ִ�д���.\n");
		RELEASE(m_pListenContext);
		return false;
	}
	else
	{
		//TRACE("bind() ���.\n");
	}

	if (SOCKET_ERROR == listen(socket,SOMAXCONN))
	{
		this->_ShowMessage("Listen()����ִ�г��ִ���.\n");
		RELEASE(m_pListenContext);
		return false;
	}
	else
	{
		//TRACE("Listen() ���.\n");
	}

	// ��ȡAcceptEx����ָ��
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
		this->_ShowMessage("WSAIoctl δ�ܻ�ȡAcceptEx����ָ�롣�������: %d\n", WSAGetLastError()); 
		this->_DeInitialize();
		return false;  
	}  

	// ��ȡGetAcceptExSockAddrs����ָ��
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
		this->_ShowMessage("WSAIoctl δ�ܻ�ȡGuidGetAcceptExSockAddrs����ָ�롣�������: %d\n", WSAGetLastError());  
		this->_DeInitialize();
		return false; 
	}  


	// ΪAcceptEx ׼��������Ȼ��Ͷ��AcceptEx I/O����
	for( int i=0;i<MAX_POST_ACCEPT;i++ )
	{
		// �½�һ��IO_CONTEXT
		PER_IO_CONTEXT* pAcceptIoContext = m_pListenContext->GetNewIoContext();

		if( false==this->_PostAccept( pAcceptIoContext ) )
		{
			m_pListenContext->RemoveContext(pAcceptIoContext);
			return false;
		}
	}

	this->_ShowMessage("Ͷ��%d��AcceptEx�������\r\n", MAX_POST_ACCEPT);

	return true;
}
void CIOCPModel::_DeInitialize()
{

	// �ر�ϵͳ�˳��¼����
	RELEASE_HANDLE(m_hShutdownEvent);

	// �ͷŹ������߳̾��ָ��
	for( int i=0;i<m_nThreads;i++ )
	{
		RELEASE_HANDLE(m_phWorkerThreads[i]);
	}
	
	RELEASE(m_phWorkerThreads);

	// �ر�IOCP���
	RELEASE_HANDLE(m_hIOCompletionPort);

	// �رռ���Socket
	RELEASE(m_pListenContext);

	this->_ShowMessage("�ͷ���Դ���.\n");
}
bool CIOCPModel::_AssociateWithIOCP( CPerSocketContext *pContext )
{
	// �����ںͿͻ���ͨ�ŵ�SOCKET�󶨵���ɶ˿���
	HANDLE hTemp = CreateIoCompletionPort((HANDLE)pContext->GetSocket(), m_hIOCompletionPort, (DWORD)pContext, 0);

	if (NULL == hTemp)
	{
		this->_ShowMessage(("ִ��CreateIoCompletionPort()���ִ���.������룺%d"),GetLastError());
		return false;
	}

	return true;
}

bool CIOCPModel::_PostAccept( PER_IO_CONTEXT* pAcceptIoContext )
{
	assert(INVALID_SOCKET != m_pListenContext->GetSocket());

	// ׼������
	DWORD dwBytes = 0;  
	pAcceptIoContext->m_OpType = ACCEPT_POSTED;  
	WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;
	
	// Ϊ�Ժ�������Ŀͻ�����׼����Socket( ������봫ͳaccept�������� ) 
	pAcceptIoContext->m_sockAccept  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);  
	if(INVALID_SOCKET == pAcceptIoContext->m_sockAccept )  
	{  
		_ShowMessage("��������Accept��Socketʧ�ܣ��������: %d", WSAGetLastError()); 
		return false;  
	} 

	cm::cm_socket_setkeepalivetime(pAcceptIoContext->m_sockAccept, 5, 5, 1);

	// Ͷ��AcceptEx
	if(FALSE == m_lpfnAcceptEx( m_pListenContext->GetSocket(), pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),   
								sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))  
	{  
		if(WSA_IO_PENDING != WSAGetLastError())  
		{  
			_ShowMessage("Ͷ�� AcceptEx ����ʧ�ܣ��������: %d", WSAGetLastError());  
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
					this->_ShowMessage("�ͻ��� %s:%d ����.�ͻ�����%d\r\n", dst, ntohs(ClientAddr->sin_port), nNum);
				}
			}
		}
	}

	return this->_PostAccept(pIoContext); 	
}
bool CIOCPModel::_PostRecv(PER_IO_CONTEXT* pIoContext)
{
	// ��ʼ������
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
	OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

	// ��ʼ����ɺ󣬣�Ͷ��WSARecv����
	int nBytesRecv = WSARecv( pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

	// �������ֵ���󣬲��Ҵ���Ĵ��벢����Pending�Ļ����Ǿ�˵������ص�����ʧ����
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		this->_ShowMessage("Ͷ�ݵ�һ��WSARecvʧ�ܣ�");
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
		this->_ShowMessage("Ͷ�ݵ�һ��WSASendʧ�ܣ�");
		return false;
	}

	return true;
}
bool CIOCPModel::_DoSend(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered)
{
	//�����ֽ�������

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
	// ����ǳ�ʱ�ˣ����ټ����Ȱ�  
	if(WAIT_TIMEOUT == dwErr)  
	{  	
		// ȷ�Ͽͻ����Ƿ񻹻���...
		if( !_IsSocketAlive( pContext->GetSocket()) )
		{
			this->_ShowMessage("��⵽�ͻ����쳣�˳���");
			return true;
		}
		else
		{
			this->_ShowMessage("���������ʱ��������...");
			return true;
		}
	}  

	// �����ǿͻ����쳣�˳���
	else if( ERROR_NETNAME_DELETED==dwErr )
	{
		this->_ShowMessage("��⵽�ͻ����쳣�˳���");
		return true;
	}

	else
	{
		this->_ShowMessage("��ɶ˿ڲ������ִ����߳��˳���������룺%d",dwErr );
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
	// ���ݴ���Ĳ�����ʽ���ַ���
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
		this->_ShowMessage("�ͻ��� %s:%d �Ͽ�.", dst, ntohs(pSocketContext->m_ClientAddr.sin_port));
		pSocketContext->CloseSocket();
		//Ӧ�ü���Ϊ0ʱ������
		int nRet = m_pIOCPUser->CBDisconnect(pSocketContext);
		this->_ShowMessage("ʣ������%d\r\n", nRet);
	}
	return nCount;
}
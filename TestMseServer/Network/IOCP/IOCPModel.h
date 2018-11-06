
#pragma once


// winsock 2 ��ͷ�ļ��Ϳ�

#include <vector>
#include <string>
#include <assert.h>

#include "..\ServerModelInterface.h"
#include "PerSocketContext.h"

class CIOCPModel;

typedef struct _tagThreadParams_WORKER
{
	CIOCPModel* pIOCPModel;                                   // ��ָ�룬���ڵ������еĺ���
	int         nThreadNo;                                    // �̱߳��
} THREADPARAMS_WORKER,*PTHREADPARAM_WORKER; 


class CIOCPModel : public CServerModelInterface
{
public:
	CIOCPModel(CServerModelUser* pUser);
	~CIOCPModel(void);
public:
	// ����������
	virtual int  Start(const char* server_address, uint16_t server_port, int num_thread = 0);
	// ֹͣ������
	virtual void Stop();
	// ������Ϣ
	virtual int Send(CPerSocketContext *pSocketContext, char* pBuffer, uint32_t uLen);
	// �Ͽ�����
	virtual int Disconnect(CPerSocketContext *pSocketContext);

	virtual std::string GetIP(){ return m_strIP; }
private:
	int SetIdleIoContext(CPerSocketContext* pSocketContext, _PER_IO_CONTEXT* pContext);

private:
	// �̺߳�����ΪIOCP�������Ĺ������߳�
	static DWORD WINAPI _WorkerThread(LPVOID lpParam);

	bool LoadSocketLib();
	void UnloadSocketLib() { WSACleanup(); }
private:
	friend CPerSocketContext;
	// ��ʼ��IOCP
	bool _InitializeIOCP();
	// ��ʼ��Socket
	bool _InitializeListenSocket(const char* pServerAddress);
	// ����ͷ���Դ
	void _DeInitialize();
	// ������󶨵���ɶ˿���
	bool _AssociateWithIOCP( CPerSocketContext *pContext);

	// Ͷ��Accept����
	bool _PostAccept(PER_IO_CONTEXT* pAcceptIoContext); 
	// Ͷ�ݽ�����������
	bool _PostRecv(PER_IO_CONTEXT* pIoContext);
	// Ͷ�ݷ�������
	bool _PostSend(PER_IO_CONTEXT* pIoContext, DWORD dwSend);
	// ���пͻ��������ʱ�򣬽��д���
	bool _DoAccpet( CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered);
	// ���н��յ����ݵ����ʱ�򣬽��д���
	bool _DoRecv(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered);
	// ������ɣ����д���
	bool _DoSend(CPerSocketContext* pSocketContext, PER_IO_CONTEXT* pIoContext, DWORD dwBytesTransfered);

	// �жϿͻ���Socket�Ƿ��Ѿ��Ͽ�
	bool _IsSocketAlive(SOCKET s);
	// ������ɶ˿��ϵĴ���
	bool HandleError( CPerSocketContext *pContext,const DWORD& dwErr );

	// ��ñ����Ĵ���������
	int _GetNoOfProcessors();
	// ��ӡ��Ϣ
	void _ShowMessage( const std::string szFormat,...) const;

private:
	HANDLE                      m_hShutdownEvent;              // ����֪ͨ�߳�ϵͳ�˳����¼���Ϊ���ܹ����õ��˳��߳�
	HANDLE                      m_hIOCompletionPort;           // ��ɶ˿ڵľ��
	HANDLE*                     m_phWorkerThreads;             // �������̵߳ľ��ָ��
	int		                    m_nThreads;                    // ���ɵ��߳�����
	std::string                 m_strIP;                       // �������˵�IP��ַ
	int                         m_nPort;                       // �������˵ļ����˿�  
	CPerSocketContext*          m_pListenContext;              // ���ڼ�����Socket��Context��Ϣ
	LPFN_ACCEPTEX				m_lpfnAcceptEx;                // AcceptEx �� GetAcceptExSockaddrs �ĺ���ָ�룬���ڵ�����������չ����
	LPFN_GETACCEPTEXSOCKADDRS	m_lpfnGetAcceptExSockAddrs; 

private:
	CServerModelUser* m_pIOCPUser;
};



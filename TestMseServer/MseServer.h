#pragma once

#include "Network\ServerModelInterface.h"

class MseServer : public CServerModelUser
{
public:
	MseServer();
	~MseServer();

public:
	int StartServer(char *pServerAddress, uint16_t sPort);

public:
	//��ɶ˿ڷ������ص�
	virtual int CBAccept(CPerSocketContext **ppSocketContext);
	virtual int CBRecv(CPerSocketContext *pContext, char *pBuffer, uint32_t uBytesTransfered);
	virtual int CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered);
	virtual int CBDisconnect(CPerSocketContext *pContext);

private:
	//websocket����������ʵ��
	CServerModelInterface* m_pServerModel;
};


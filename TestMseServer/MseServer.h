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
	//完成端口服务器回调
	virtual int CBAccept(CPerSocketContext **ppSocketContext);
	virtual int CBRecv(CPerSocketContext *pContext, char *pBuffer, uint32_t uBytesTransfered);
	virtual int CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered);
	virtual int CBDisconnect(CPerSocketContext *pContext);

private:
	//websocket服务器对象实例
	CServerModelInterface* m_pServerModel;
};


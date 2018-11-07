#include "MseServer.h"

#include "WebSocket\WebSocketProtocol.h"

MseServer::MseServer()
	:m_pServerModel(NULL)
{
}


MseServer::~MseServer()
{
}

int MseServer::StartServer(const char *pServerAddress, uint16_t sPort)
{
	if (m_pServerModel != NULL)
		return -1;

	m_pServerModel = NewServer(this);

	return m_pServerModel->Start((const char*)pServerAddress, sPort, 0);
}

#pragma region Á´Â·½Ó¿Ú


int MseServer::CBAccept(CPerSocketContext **ppSocketContext)
{
	CWebSocketItem* pWebSocketItem = new CWebSocketItem();
	if (pWebSocketItem == NULL)
	{
		return -1;
	}

	*ppSocketContext = pWebSocketItem;

	return 0;
}
int MseServer::CBRecv(CPerSocketContext *pContext, char *pBuffer, uint32_t uBytesTransfered)
{
	int nRet = 0;

	CWebSocketItem* pWebSocketItem = (CWebSocketItem*)pContext;
	if (!pWebSocketItem->IsHandshakeComplete())
	{
		std::string strOut;
		nRet = CWebSocketProtocol::Handshake(pBuffer, uBytesTransfered, &strOut);
		if (nRet < 0)
		{
			printf("CBRecv DoHandshake failed return %d", nRet);
			return -1;
		}

		nRet = m_pServerModel->Send(pContext, (char*)strOut.c_str(), strOut.size());
		if (nRet != strOut.size())
		{
			printf("CBRecv Send failed return %d, size %d", nRet, strOut.size());
			return -1;
		}

		pWebSocketItem->HandshakeComplete();

		return 0;
	}


	return nRet;
}
int MseServer::CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered)
{

	return 0;
}
int MseServer::CBDisconnect(CPerSocketContext *pContext)
{
	delete pContext;
	return 0;
}

#pragma endregion
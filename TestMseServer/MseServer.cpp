#include "MseServer.h"



MseServer::MseServer()
{
}


MseServer::~MseServer()
{
}

int MseServer::StartServer(char *pServerAddress, uint16_t sPort)
{
	if (m_pServerModel != NULL)
		return -1;

	m_pServerModel = NewServer(this);

	return m_pServerModel->Start((const char*)pServerAddress, sPort, 0);
}

#pragma region 链路接口


int MseServer::CBAccept(CPerSocketContext **ppSocketContext)
{
	int nNum;
	CWebSocketItem* pWebSocketItem = g_GatewayManage.AcquireWebSocketItem(nNum);
	if (pWebSocketItem == NULL)
	{
		return ERETURN_FAILED;
	}

	*ppSocketContext = pWebSocketItem;

	return nNum;
}
int MseServer::CBRecv(CPerSocketContext *pContext, char *pBuffer, uint32_t uBytesTransfered)
{
	int nRet = 0;

	CWebSocketItem* pWebSocketItem = (CWebSocketItem*)pContext;
	if (!pWebSocketItem->IsHandshakeComplete())
	{
		std::string strOut;
		nRet = DoHandshake(pBuffer, uBytesTransfered, &strOut);
		if (nRet < 0)
		{
			LOGF(__FILE__, __LINE__, "CBRecv DoHandshake failed return %d", nRet);
			return ERETURN_FAILED;
		}

		nRet = m_pServerModel->Send(pContext, (char*)strOut.c_str(), strOut.size());
		if (nRet != strOut.size())
		{
			LOGF(__FILE__, __LINE__, "CBRecv Send failed return %d, size %d", nRet, strOut.size());
			return ERETURN_FAILED;
		}

		pWebSocketItem->HandshakeComplete();

		return ERETURN_SUCCESS;
	}

	CCommonBuffer* pRestBuffer = pWebSocketItem->GetRestBuffer();
	CCommonBuffer* pRecvBuffer = pWebSocketItem->GetRecvBuffer();

	uint8_t ucMsgOPCode;
	uint32_t uRestLength;
	if (pRestBuffer->GetUseLength() > 0)
	{
		pRestBuffer->PushBuffer(pBuffer, uBytesTransfered);
		pBuffer = pRestBuffer->GetBuffer();
		uRestLength = pRestBuffer->GetUseLength();
	}
	else {
		uRestLength = uBytesTransfered;
	}

	while (uRestLength)
	{
		//如果是错误的数据,需断开连接。 待添加验证 = 0 为数据不够， < 0 为解析错误
		nRet = CWebSocketProtocol::DecodeFrame(pBuffer, uRestLength, pRecvBuffer, ucMsgOPCode);
		if (nRet <= 0)
			break;

		pBuffer += nRet;
		uRestLength -= nRet;

		switch (ucMsgOPCode)
		{
		case WS_OPCODE_BINARY:
		case WS_OPCODE_CONTINUE:			//只支持2进制文件的拆包情况。
		{
			//DoRecvFileSlice(pRecvBuffer);
		}
		break;
		case WS_OPCODE_TEXT:
		{
			nRet = DoWSText(pWebSocketItem, pRecvBuffer);
		}
		break;
		case WS_OPCODE_CLOSE:
		{
			return -1;
		}
		break;
		case WS_OPCODE_PING:
		{

		}
		break;
		case WS_OPCODE_PONG:
		{

		}
		break;
		default:
		{
			//出错，断开连接
		}
		}
	}

	if (nRet == 0)		//剩下数据保存到RestBuffer
	{
		if (pRestBuffer->GetUseLength() > 0)
		{
			pRestBuffer->RetainBuffer(uRestLength);
		}
		else
		{
			if (uRestLength > 0)
			{
				pRestBuffer->PushBuffer(pBuffer + (uBytesTransfered - uRestLength), uRestLength);
			}
		}
	}


	return nRet;
}
int MseServer::CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered)
{

	return 0;
}
int MseServer::CBDisconnect(CPerSocketContext *pContext)
{
	return g_GatewayManage.ReleaseWebSocketItem((CWebSocketItem*)pContext);
}

#pragma endregion
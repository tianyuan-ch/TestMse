#pragma once

#include "Network\ServerModelInterface.h"
#include "cm\cm_thread.h"
#include "FMp4\H264AacPackToFmp4Man.h"

#include <map>

class MseServer;

class CWebSocketItem : public CPerSocketContext
{
public:
	CWebSocketItem(MseServer* mServer) :m_IsHandshakeFinish(false), m_fmp4Muxer(NULL) { m_Server = mServer; };
	~CWebSocketItem() {};

	bool IsHandshakeComplete() { return m_IsHandshakeFinish; }
	void HandshakeComplete() { m_IsHandshakeFinish = true; }

private:
	bool m_IsHandshakeFinish;

public:
	H264AacPackToFmp4Man *m_fmp4Muxer;
	MseServer *m_Server;

};

class MseServer : public CServerModelUser
{
	class CThreadReader : public cm::CBaseThread
	{
	public:
		CThreadReader(MseServer *pServer) { m_Server = pServer; }

		virtual void ThreadMain()
		{
			m_Server->ThreadReader();
		}

	private:
		MseServer *m_Server;
	};

public:
	MseServer();
	~MseServer();

public:
	int StartServer(const char *pServerAddress, uint16_t sPort);
public:
	void ThreadReader();
public:
	//完成端口服务器回调
	virtual int CBAccept(CPerSocketContext **ppSocketContext);
	virtual int CBRecv(CPerSocketContext *pContext, char *pBuffer, uint32_t uBytesTransfered);
	virtual int CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered);
	virtual int CBDisconnect(CPerSocketContext *pContext);

	void FMp4CallBack(CWebSocketItem *pContext, uint8_t *buf, int buf_size);
private:
	//websocket服务
	CServerModelInterface* m_pServerModel;
	//读视频文件线程
	CThreadReader* m_thdReader;
	//发送列表
	std::map <long, CWebSocketItem*>m_mapWebItem;
	
};


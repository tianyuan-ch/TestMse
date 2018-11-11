#include "MseServer.h"

#include "WebSocket\WebSocketProtocol.h"


#include "utility\FfmpegHeader.h"
#include "cm\cm_time.h"


//视频h264  音频aac 8000采样
#define VIDEO_FILE "D:\\test_file\\cyx-hukux-720p-ar8000-ac1.mp4"

void FMp4CallBack(long arg, uint8_t *buf, int buf_size)
{
	CWebSocketItem* pWebSocketItem = (CWebSocketItem*)arg;
	pWebSocketItem->m_Server->FMp4CallBack(pWebSocketItem, buf, buf_size);
}

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

	m_thdReader = new CThreadReader(this);
	m_thdReader->Start();

	m_pServerModel = NewServer(this);
	return m_pServerModel->Start((const char*)pServerAddress, sPort, 0);
}

#pragma region 链路接口

int MseServer::CBAccept(CPerSocketContext **ppSocketContext)
{
	CWebSocketItem* pWebSocketItem = new CWebSocketItem(this);
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

		return uBytesTransfered;
	}

	int nPos = 0;

	//接收0 1 2 3  发送5 6 7 8
	while (nPos < uBytesTransfered)
	{
		int nOutLen, nPackageLen;
		char *pData = CWebSocketProtocol::DecodeFrame(pBuffer + nPos, uBytesTransfered - nPos, nOutLen, nPackageLen);
		if (pData == NULL)
		{
			break;
		}
		nPos += nPackageLen;

		if (pData[0] == 0 && pData[1] == 1 && pData[2] == 2 && pData[3] == 3)
		{
			if (m_mapWebItem.end() == m_mapWebItem.find(pWebSocketItem->GetSocket()))
			{
				pWebSocketItem->m_fmp4Muxer = new H264AacPackToFmp4Man();
				pWebSocketItem->m_fmp4Muxer->Init(::FMp4CallBack, (long)pWebSocketItem);
				m_mapWebItem.insert(std::pair<long, CWebSocketItem*>((long)pWebSocketItem->GetSocket(), pWebSocketItem));
			}


			//char szSend[14];
			//szSend[10] = 5;
			//szSend[11] = 6;
			//szSend[12] = 7;
			//szSend[13] = 8;

			//char *pSendData = CWebSocketProtocol::EncodeFrame(szSend + 10, 4, nOutLen);

			//m_pServerModel->Send(pContext, pSendData, nOutLen);
		}
		else
		{
			return -1;
		}
	}

	return nPos;
}
int MseServer::CBSend(CPerSocketContext *pContext, uint32_t uBytesTransfered)
{

	return 0;
}
int MseServer::CBDisconnect(CPerSocketContext *pContext)
{
	//CWebSocketItem* pWebSocketItem = (CWebSocketItem*)pContext;

	//if (m_mapWebItem.end() != m_mapWebItem.find(pWebSocketItem->GetSocket()))
	//	m_mapWebItem.erase(pWebSocketItem->GetSocket());
	//delete pContext;

	return 0;
}

#pragma endregion

void MseServer::FMp4CallBack(CWebSocketItem *pContext, uint8_t *buf, int buf_size)
{
	int nOutLen;
	char *pSendData = CWebSocketProtocol::EncodeFrame((char*)buf, buf_size, nOutLen);
	m_pServerModel->Send(pContext, pSendData, nOutLen);

	//printf("send %d\r\n", nOutLen);
}

void MseServer::ThreadReader()
{
	av_register_all();
	avcodec_register_all();

	int ret;
	AVFormatContext* pFormatContext = NULL;

	ret = avformat_open_input(&pFormatContext, VIDEO_FILE, NULL, NULL);
	if (ret != 0)
	{
		printf("avformat_open_input failed!\n");
		return;
	}

	ret = avformat_find_stream_info(pFormatContext, NULL);
	if (ret != 0)
	{
		printf("avformat_find_stream_info failed!\n");
		return;
	}

	AVStream *vStream = NULL, *aStream = NULL;
	for (int i = 0; i < pFormatContext->nb_streams; i++)
	{
		if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			vStream = pFormatContext->streams[i];
		}
		else if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			aStream = pFormatContext->streams[i];
		}
	}

	AVBitStreamFilterContext* mp4ToAnnexb = av_bitstream_filter_init("h264_mp4toannexb");

	int beginTime = cm::cm_gettickcount();
	int64_t beginPTS = -1;

	AVPacket avPacket;
	av_init_packet(&avPacket);

	while (true)
	{
		ret = av_read_frame(pFormatContext, &avPacket);
		if (ret == AVERROR_EOF)
		{
			av_seek_frame(pFormatContext, 0, 0, AVSEEK_FLAG_ANY);
			continue;
		}
		else if (ret < 0)
		{
			printf("av_read_frame failed!\n");
			return;
		}

		if (avPacket.stream_index == aStream->index)
		{
			int64_t pts = avPacket.pts * 1000 / (aStream->time_base.den / (double)aStream->time_base.num);

			std::map <long, CWebSocketItem*>::iterator iter;
			for (iter = m_mapWebItem.begin(); iter != m_mapWebItem.end(); ++iter)
			{
				CWebSocketItem* pWebSocketItem = iter->second;
				pWebSocketItem->m_fmp4Muxer->InputAudio(avPacket.data, avPacket.size, pts);

			}
		}
		else if (avPacket.stream_index == vStream->index)
		{
			int64_t pts = avPacket.pts * 1000 / (vStream->time_base.den / (double)vStream->time_base.num);

			if (beginPTS == -1)
			{
				beginPTS = pts;
				beginTime == cm::cm_gettickcount();
			}
			else
			{
				int nSleep = pts - beginPTS - (cm::cm_gettickcount() - beginTime);
				if (nSleep >= 1)
				{
					cm::cm_sleep(nSleep);
				}
			}


			ret = av_bitstream_filter_filter(mp4ToAnnexb, vStream->codec, NULL, &avPacket.data, &avPacket.size, avPacket.data, avPacket.size, avPacket.flags & AV_PKT_FLAG_KEY);

			std::map <long, CWebSocketItem*>::iterator iter;
			for (iter = m_mapWebItem.begin(); iter != m_mapWebItem.end(); ++iter)
			{
				CWebSocketItem* pWebSocketItem = iter->second;
				pWebSocketItem->m_fmp4Muxer->InputH264(avPacket.data, avPacket.size, pts);

			}
			
		}


	}

}
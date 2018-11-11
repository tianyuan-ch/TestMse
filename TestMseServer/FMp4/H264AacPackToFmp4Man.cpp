#include "H264AacPackToFmp4Man.h"

#include "H264AacPackToFmp4FfmpegImpl.h"

const unsigned char AAC_FIRST[22] = { 0xde, 0x04, 0x00, 0x4c, 0x61, 0x76, 0x63, 0x35, 0x37, 0x2e, 0x31,
							0x30, 0x37, 0x2e, 0x31, 0x30, 0x30, 0x00, 0x02, 0x30, 0x40, 0x0e };
const unsigned char AAC_DATA[4] = { 0x01, 0x18, 0x20, 0x07 };

H264AacPackToFmp4Man::H264AacPackToFmp4Man()
	: m_to_fmp4(NULL)
{
}


H264AacPackToFmp4Man::~H264AacPackToFmp4Man()
{
	Deinit();
}

int H264AacPackToFmp4Man::Init(Fmp4Data call_back, long arg)
{
	if (m_to_fmp4 != NULL)
		Deinit();

	int ret;
	m_to_fmp4 = new H264AacPackToFmp4FfmpegImpl();
	ret = m_to_fmp4->Init(call_back, arg);
	if (ret < 0)
	{
		Deinit();
		printf("H264AacPackToFmp4Man::Init m_to_fmp4->Init failed \n");
		return -1;
	}


	FillAudioInit();

	m_timestamp_unite_prepare = false;
	m_timestamp_video_last = -1;
	m_timestamp_video_last_use = -1;
	m_timestamp_video_differ = 0;
	m_timestamp_audio_differ = 0;

	return ret;
}


int H264AacPackToFmp4Man::InputAudio(uint8_t *data, int data_size, int64_t timestamp)
{
	if (m_to_fmp4 == NULL)
		return -1;

	//与视频时间戳为基准(超过3秒认为起始时间戳起始不同)
	if (!m_timestamp_unite_prepare)
	{
		if (abs(timestamp - m_timestamp_video_last_use) > 3000)
		{
			m_timestamp_audio_differ = timestamp - m_timestamp_video_last_use;
		}
		else
		{
			m_timestamp_audio_differ = 0;
		}
		m_timestamp_unite_prepare = true;
	}
	timestamp = timestamp - m_timestamp_audio_differ;

	//与视频时间戳相差超过3秒，丢弃
	if (m_timestamp_video_last_use != -1)
	{
		if (abs(timestamp - m_timestamp_video_last_use) > 3000)
		{
			m_timestamp_unite_prepare = false;
			return -1;
		}
	}

	UpdateAudioTimestamp(timestamp);

	AVPacket packet;
	av_init_packet(&packet);
	packet.data = (uint8_t*)data;
	packet.size = data_size;
	packet.pts = timestamp;
	return m_to_fmp4->InputAAC(&packet);

	return 0;
}

int H264AacPackToFmp4Man::InputH264(uint8_t *data, int data_size, int64_t timestamp)
{
	if (m_to_fmp4 == NULL)
		return -1;
	
	//时间戳基准变更的情况(超过10秒认定为变更)
	if (m_timestamp_video_last != -1)
	{
		if (abs(timestamp - m_timestamp_video_last) > 10000)
		{
			m_timestamp_unite_prepare = false;
			m_timestamp_video_differ += (timestamp - m_timestamp_video_last);
		}
	}
	m_timestamp_video_last = timestamp;

	timestamp = timestamp - m_timestamp_video_differ;

	m_timestamp_video_last_use = timestamp;

	UpdateVideoTimestamp(timestamp);

	AVPacket packet;
	av_init_packet(&packet);
	packet.data = (uint8_t*)data;
	packet.size = data_size;
	packet.pts = timestamp;

	return m_to_fmp4->InputH264(&packet);
}

void H264AacPackToFmp4Man::Deinit()
{
	if (m_to_fmp4 != NULL)
	{
		delete m_to_fmp4;
		m_to_fmp4 = NULL;
	}

}

void H264AacPackToFmp4Man::FillAudioInit()
{
	m_fillao_first = true;
	m_fillao_last_audio_pts = 0;
	m_fillao_last_video_pts = 0;
	m_fillao_video_continuity = false;
	m_fillao_fill_audio_pts = 0;

	m_delay_ms_fill_audio = 2000;
}

bool H264AacPackToFmp4Man::UpdateAudioTimestamp(int64_t timestamp)
{
	if (m_delay_ms_fill_audio <= 0)
		return true;

	m_fillao_first = false;
	m_fillao_last_audio_pts = timestamp;

	//小于填充pts不播放
	if (m_fillao_last_audio_pts < m_fillao_fill_audio_pts)
	{
		if (m_fillao_fill_audio_pts - m_fillao_last_audio_pts > m_delay_ms_fill_audio - 1000)
		{
			return false;
		}
	}
	else
	{
		m_fillao_fill_audio_pts = m_fillao_last_audio_pts;
	}

	return true;
}

void H264AacPackToFmp4Man::UpdateVideoTimestamp(int64_t timestamp)
{

	m_fillao_last_video_pts = timestamp;
	if (m_fillao_last_video_pts - m_fillao_last_audio_pts > m_delay_ms_fill_audio)
	{
		m_fillao_fill_audio_pts = (m_fillao_last_video_pts - m_fillao_fill_audio_pts) > m_delay_ms_fill_audio ?
			m_fillao_last_video_pts - m_delay_ms_fill_audio : m_fillao_fill_audio_pts;

		//补充音频
		while (m_fillao_last_video_pts - m_fillao_fill_audio_pts > m_delay_ms_fill_audio - 1000)
		{
			AVPacket packet;
			av_init_packet(&packet);
			//if (m_fillao_first)
			//{
			//	packet.data = (uint8_t*)AAC_FIRST;
			//	packet.size = sizeof(AAC_FIRST);
			//}
			//else
			{
				packet.data = (uint8_t*)AAC_DATA;
				packet.size = sizeof(AAC_DATA);
			}
			packet.pts = m_fillao_fill_audio_pts;
			m_fillao_fill_audio_pts += 128;

			//printf("test audio fill 123\n");

			m_to_fmp4->InputAAC(&packet);

		}
	}
}

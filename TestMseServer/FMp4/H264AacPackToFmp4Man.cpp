#include "H264AacPackToFmp4Man.h"

#include "H264AacPackToFmp4FfmpegImpl.h"

const byte AAC_FIRST[22] = { 0xde, 0x04, 0x00, 0x4c, 0x61, 0x76, 0x63, 0x35, 0x37, 0x2e, 0x31,
							0x30, 0x37, 0x2e, 0x31, 0x30, 0x30, 0x00, 0x02, 0x30, 0x40, 0x0e };
const byte AAC_DATA[4] = { 0x01, 0x18, 0x20, 0x07 };

H264AacPackToFmp4Man::H264AacPackToFmp4Man()
	: m_to_fmp4(NULL)
	, m_audio_transcoder(NULL)
{
}


H264AacPackToFmp4Man::~H264AacPackToFmp4Man()
{
	Deinit();
}

int H264AacPackToFmp4Man::Init(int in_samples_rate, AVSampleFormat sample_format, int channels, Fmp4Data call_back, int arg, int delay_sec_fill_audio)
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

	m_samples_rate = in_samples_rate;
	m_sample_format = sample_format;
	m_channels = channels;
	m_audio_codec_type = (JTRTAVCodeType)NONE;
	m_prev_timestamp = 0;
	m_delay_ms_fill_audio = delay_sec_fill_audio * 1000;

	FillAudioInit();

	return ret;
}

int H264AacPackToFmp4Man::SetAudioCodecType(JTRTAVCodeType codec_type)
{
	if (m_to_fmp4 == NULL)
		return -1;
	if (m_audio_codec_type != (JTRTAVCodeType)NONE)
		return 0;

	int ret;
	m_audio_codec_type = codec_type;

	if (codec_type != (JTRTAVCodeType)AAC)
	{
		m_audio_transcoder = new AudioTranscoderMan();
		ret = m_audio_transcoder->Init(m_samples_rate, m_sample_format, m_channels, codec_type);
		if (ret < 0)
		{
			delete m_audio_transcoder;
			m_audio_transcoder = NULL;
			printf("m_audio_transcoder->Init failed\n");
			return -1;
		}
	}

	return 0;
}

int H264AacPackToFmp4Man::InputAudio(uint8_t *data, int data_size, int64_t timestamp)
{
	if (m_to_fmp4 == NULL || m_audio_codec_type == NONE)
		return -1;

	if (!AudioUpdate(timestamp))
	{
		printf("InputAudio AudioUpdate return false\n");
		return -1;
	}

	uint8_t *out_data = NULL;
	int out_size;
	int ret;

	//8000采样128ms一帧
	if (m_prev_timestamp == 0)
		m_prev_timestamp = timestamp + (128 - timestamp % 128);

	if (m_audio_codec_type != (JTRTAVCodeType)AAC && m_audio_transcoder != NULL)
	{
		ret = m_audio_transcoder->TranscodeSend(data, data_size);
		if (ret >= 0)
		{
			m_audio_transcoder->TranscodeReceive(&out_data, out_size);
		}
	}
	else
	{
		out_data = data;
		out_size = data_size;
	}

	if (out_data != NULL)
	{
		AVPacket packet;
		av_init_packet(&packet);
		packet.data = (uint8_t*)out_data;
		packet.size = out_size;
		packet.pts = m_prev_timestamp;
		m_prev_timestamp = 0;
		return m_to_fmp4->InputAAC(&packet);
	}

	return 0;
}

int H264AacPackToFmp4Man::InputH264(uint8_t *data, int data_size, int64_t timestamp)
{
	if (m_to_fmp4 == NULL)
		return -1;
	VideoUpdate(timestamp);

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

	if (m_audio_transcoder != NULL)
	{
		delete m_audio_transcoder;
		m_audio_transcoder = NULL;
	}
}

byte* H264AacPackToFmp4Man::JT1078PackData(JTRTHead head, byte* data, int &len)
{
	return m_jt1078_packdata.PackData(head, data, len);
}

void H264AacPackToFmp4Man::FillAudioInit()
{
	m_fillao_first = true;
	m_fillao_last_audio_pts = 0;
	m_fillao_last_video_pts = 0;
	m_fillao_video_continuity = false;
	m_fillao_fill_audio_pts = 0;
}

bool H264AacPackToFmp4Man::AudioUpdate(int64_t timestamp)
{
	if (m_delay_ms_fill_audio <= 0)
		return false;

	m_fillao_first = false;
	m_fillao_last_audio_pts = timestamp;

	//小于填充pts不播放
	if (m_fillao_last_audio_pts < m_fillao_fill_audio_pts)
	{
		if (m_fillao_fill_audio_pts - m_fillao_last_audio_pts > m_delay_ms_fill_audio - 1000)
		{
			return false;
		}
		return true;
	}
	else
	{
		m_fillao_fill_audio_pts = m_fillao_last_audio_pts;
		return true;
	}
}

void H264AacPackToFmp4Man::VideoUpdate(int64_t timestamp)
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

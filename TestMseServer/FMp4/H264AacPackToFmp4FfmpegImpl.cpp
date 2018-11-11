#include "H264AacPackToFmp4FfmpegImpl.h"

#include "utility\H264Utils.h"

int AVIOWritePacket(void* opaque, uint8_t *buf, int buf_size)
{
	H264AacPackToFmp4FfmpegImpl* object = (H264AacPackToFmp4FfmpegImpl*)opaque;
	object->m_call_back(object->m_arg, buf, buf_size);

	return buf_size;
}

int AVIOReadPacket(void* opaque, uint8_t *buf, int buf_size)
{
	return buf_size;
}

int64_t AVIOSeek(void* opaque, int64_t offset, int whence)
{
	return offset;
}

H264AacPackToFmp4FfmpegImpl::H264AacPackToFmp4FfmpegImpl()
	: m_is_codec_init(false)
	, m_first_timestamp(0)
	, m_imft_ctx(NULL)
	, m_format_context(NULL)
	, m_parser_codec_ctx(NULL)
	, m_parser(NULL)
	, m_parse_packet(NULL)
	, avio_ctx(NULL)
	, avio_ctx_buffer(NULL)
	, avio_ctx_buffer_size(409600)
{

}


H264AacPackToFmp4FfmpegImpl::~H264AacPackToFmp4FfmpegImpl()
{
	Deinit();
}

int H264AacPackToFmp4FfmpegImpl::Init(Fmp4Data call_back, long arg)
{
	if (m_format_context != NULL)
		Deinit();

	int ret = 0;
	m_call_back = call_back;
	m_arg = arg;

	AVCodec* codec_decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (codec_decoder == NULL)
	{
		printf("H264AacPackToFmp4FfmpegImpl::Init avcodec_find_decoder AV_CODEC_ID_H264 failed\n");
		return -1;
	}

	m_parser_codec_ctx = avcodec_alloc_context3(codec_decoder);
	if (m_parser_codec_ctx == NULL)
	{
		printf("H264AacPackToFmp4FfmpegImpl::Init avcodec_alloc_context3 failed\n");
		return -1;
	}

	m_parser = av_parser_init(AV_CODEC_ID_H264);
	if (m_parser == NULL)
	{
		printf("H264AacPackToFmp4FfmpegImpl::Init av_parser_init failed\n");
		return -1;
	}


	m_parse_packet = av_packet_alloc();
	if (!m_parse_packet) {
		printf("av_packet_alloc failed\n");
		return -1;
	}

	m_is_codec_init = false;

	m_width = 0;
	m_height = 0;
	m_prev_video_pts = -1;
	m_prev_audio_pts = -1;
	return 0;
}

int H264AacPackToFmp4FfmpegImpl::InputH264(AVPacket* packet)
{
	if (m_parser_codec_ctx == NULL)
		return -1;

	int ret = 0;
	uint8_t* data = packet->data;
	int data_size = packet->size;

	/*while*/ if(data_size > 0)
	{
		//ret = av_parser_parse2(m_parser, m_parser_codec_ctx, &m_parse_packet->data, &m_parse_packet->size,
		//	data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
		//if (ret < 0) {
		//	printf("Error while parsing\n");
		//	return ret;
		//}

		//data += ret;
		//data_size -= ret;

		//if (data_size != 0)
		//{
		//	//printf("VideoDecoderFfmpegImpl::DecodeSend34 data_size=%d\n", data_size);
		//}

		//if (m_parse_packet->size)
		//{
			//AVPacket* packet_decode = m_parse_packet;
			AVPacket* packet_decode = packet;
			//获得宽高, 是否为关键帧
			int width = 0, height = 0;
			bool is_key;
			H264Utils::H264GetInfo(packet_decode->data, packet_decode->size, width, height, is_key);

			if (is_key && width > 0)
			{
				if (m_height == 0 || m_width == 0)
				{
					m_width = width;
					m_height = height;
					printf("H264AacPackToFmp4FfmpegImpl::InputH264 width=%d, height=%d\n", m_width, m_height);
				}

				if (m_width != width || m_height != height)
				{
					printf("H264AacPackToFmp4FfmpegImpl::InputH264 switch resolution\n");
					m_width = width;
					m_height = height;
					DeinitFMp4();
				}
			}

			if (!m_is_codec_init)
			{
				if (!is_key)
					return -1;

				if(InitFMp4(packet_decode, width, height) < 0)
					return -1;
			}

			//if (is_key)
			{
				packet_decode->flags = AV_PKT_FLAG_KEY;
			}

			packet_decode->pts = (packet->pts - m_first_timestamp) * 90;
			if (m_prev_video_pts != -1)
			{
				if (packet_decode->pts <= m_prev_video_pts)
				{
					packet_decode->pts = m_prev_video_pts + 128;
				}
			}
			m_prev_video_pts = packet_decode->pts;

			packet_decode->dts = packet_decode->pts;
			packet_decode->stream_index = 0;
	
			ret = av_interleaved_write_frame(m_format_context, packet_decode);
			if (ret < 0)
			{
				return ret;
			}
		//}
	}


	return ret;
}

int H264AacPackToFmp4FfmpegImpl::InputAAC(AVPacket* packet)
{
	if (!m_is_codec_init)
	{
		return -1;
	}

	int ret;
	packet->stream_index = 1;
	packet->pts = (packet->pts - m_first_timestamp) * 8;

	if (m_prev_audio_pts != -1)
	{
		if (packet->pts <= m_prev_audio_pts)
		{
			packet->pts = m_prev_audio_pts + 80;
		}
	}
	m_prev_audio_pts = packet->pts;

	packet->dts = packet->pts;
	
	ret = av_interleaved_write_frame(m_format_context, packet);
	if (ret < 0)
	{
		return ret;
	}

	return 0;
}

int H264AacPackToFmp4FfmpegImpl::Deinit()
{
	DeinitFMp4();

	if (m_parser_codec_ctx != NULL)
	{
		avcodec_free_context(&m_parser_codec_ctx);
		m_parser_codec_ctx = NULL;
	}

	if (m_parse_packet != NULL)
	{
		av_packet_unref(m_parse_packet);
		m_parse_packet = NULL;
	}

	if (m_parser != NULL)
	{
		av_parser_close(m_parser);
		m_parser = NULL;
	}

	return 0;
}

int H264AacPackToFmp4FfmpegImpl::InitFMp4(AVPacket* packet, int width, int height)
{
	if (!m_is_codec_init) {
		int ret;
		int len;
		int have_video = 0;
		AVCodecParameters* codec_context = NULL;
		AVStream *stream_video = NULL, *stream_audio = NULL;

		uint8_t *extradata = H264Utils::H264GenerateExtradataFromIFrame(packet->data, packet->size, len);
		if (extradata == NULL)
		{
			printf("H264AacPackToFmp4FfmpegImpl::InitFMp4 H264GenerateExtradataFromIFrame failed \n");
			return -1;
		}

		ret = avformat_alloc_output_context2(&m_format_context, NULL, "mp4", NULL);
		if (!m_format_context)
		{
			printf("H264AacPackToFmp4FfmpegImpl::InitFMp4 avformat_alloc_output_context2 failed\n");
			return -1;
		}

		avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size + 10);
		if (avio_ctx_buffer == NULL)
		{
			printf("H264AacPackToFmp4FfmpegImpl::InitFMp4 av_malloc faield\n");
			return -1;
		}

		avio_ctx = avio_alloc_context(avio_ctx_buffer + 10, avio_ctx_buffer_size,
			1, this, &AVIOReadPacket, &AVIOWritePacket, &AVIOSeek);
		if (avio_ctx == NULL)
		{
			printf("H264AacPackToFmp4FfmpegImpl::InitFMp4 avio_alloc_context failed\n");
			return -1;
		}

		m_format_context->pb = avio_ctx;

		stream_video = avformat_new_stream(m_format_context, NULL);
		if (stream_video == NULL)
		{
			printf("H264AacPackToFmp4FfmpegImpl::InitFMp4 avformat_new_stream failed \n");
			Deinit();
			return -1;
		}
		stream_video->id = (int)m_format_context->nb_streams - 1;
		stream_video->time_base.num = 1;
		stream_video->time_base.den = 90000;
		codec_context = stream_video->codecpar;
		codec_context->codec_id = AV_CODEC_ID_H264;
		codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
		codec_context->format = AV_PIX_FMT_YUV420P;
		codec_context->width = width;
		codec_context->height = height;
		codec_context->extradata = extradata;
		codec_context->extradata_size = len;


		stream_audio = avformat_new_stream(m_format_context, NULL);
		stream_audio->id = (int)m_format_context->nb_streams - 1;
		stream_audio->time_base.num = 1;
		stream_audio->time_base.den = 8000;
		codec_context = stream_audio->codecpar;
		codec_context->codec_id = AV_CODEC_ID_AAC;
		codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
		codec_context->sample_rate = 8000;
		codec_context->format = AV_SAMPLE_FMT_FLT;
		codec_context->channels = 1;
		codec_context->frame_size = 1024;
		codec_context->extradata = (uint8_t*)av_malloc(2 + AV_INPUT_BUFFER_PADDING_SIZE);
		codec_context->extradata[0] = 0x15;//(2 << 3) | (11 >> 1);
		codec_context->extradata[1] = 0x88;//(11 << 7) | (1 << 3);
		codec_context->extradata_size = 2;

		AVDictionary *opts = NULL;
		//delay_moov
		ret = av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+default_base_moof", 0);
		ret = avformat_write_header(m_format_context, &opts);
		if (ret < 0)
		{
			printf("H264AacPackToFmp4FfmpegImpl::InitFMp4 avformat_write_header failed \n");
			return -1;
		}

		m_parse_packet->flags = AV_PKT_FLAG_KEY;
		m_prev_video_pts = -1;
		m_prev_audio_pts = -1;
		m_first_timestamp = packet->pts;
		m_is_codec_init = true;
	}

	return 0;
}

void H264AacPackToFmp4FfmpegImpl::DeinitFMp4()
{
	if (m_is_codec_init)
	{
		av_write_trailer(m_format_context);
		m_is_codec_init = false;
	}

	if (m_format_context != NULL)
	{
		avformat_free_context(m_format_context);
		m_format_context = NULL;
	}
	if (avio_ctx != NULL)
	{
		avio_context_free(&avio_ctx);
	}
	if (avio_ctx_buffer != NULL)
	{
		av_free(avio_ctx_buffer);
		avio_ctx_buffer = NULL;
	}
}


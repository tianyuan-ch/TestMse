#pragma once

#include "utility/FfmpegHeader.h"
#include "IH264AacPackToFmp4.h"

class H264AacPackToFmp4FfmpegImpl : public IH264AacPackToFmp4
{
public:
	H264AacPackToFmp4FfmpegImpl();
	~H264AacPackToFmp4FfmpegImpl();

	virtual int Init(Fmp4Data call_back, int arg);
	virtual int InputAAC(AVPacket* packet);
	virtual int InputH264(AVPacket* packet);
	virtual int Deinit();

public:
	Fmp4Data m_call_back;
	int m_arg;

private:
	int InitFMp4(AVPacket* packet, int width, int height);
	void DeinitFMp4();

private:
	AVFormatContext* m_imft_ctx;
	AVFormatContext* m_format_context;

	int m_width;
	int m_height;

	//parse
	AVCodecContext* m_parser_codec_ctx;
	AVCodecParserContext *m_parser;
	AVPacket* m_parse_packet;

	bool m_is_codec_init;
	int64_t m_first_timestamp;

	int64_t m_prev_video_pts;
	int64_t m_prev_audio_pts;
private:
	AVIOContext *avio_ctx;
	uint8_t *avio_ctx_buffer;
	size_t avio_ctx_buffer_size;
};


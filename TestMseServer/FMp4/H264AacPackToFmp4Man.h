#pragma once

#include "JX.h"
#include "IH264AacPackToFmp4.h"
#include "AudioTranscoderMan.h"

class H264AacPackToFmp4Man
{
public:
	H264AacPackToFmp4Man();
	~H264AacPackToFmp4Man();

	int Init( int in_samples_rate, AVSampleFormat sample_format, int channels, Fmp4Data call_back, int arg, int delay_sec_fill_audio);

	int SetAudioCodecType(JTRTAVCodeType codec_type);

	int InputAudio(uint8_t *data, int data_size, int64_t timestamp);

	int InputH264(uint8_t *data, int data_size, int64_t timestamp);

	void Deinit();

private://≤π“Ù∆µœ‡πÿ
	void FillAudioInit();
	bool AudioUpdate(int64_t timestamp);
	void VideoUpdate(int64_t timestamp);
	
	bool m_fillao_first;
	int64_t m_fillao_last_audio_pts;
	int64_t m_fillao_last_video_pts;
	bool m_fillao_video_continuity;

	int64_t m_fillao_fill_audio_pts;

	int m_delay_ms_fill_audio;
private:
	IH264AacPackToFmp4 *m_to_fmp4;
	AudioTranscoderMan *m_audio_transcoder;

	JTRTAVCodeType m_audio_codec_type;
	int m_samples_rate;
	AVSampleFormat m_sample_format;
	int m_channels;

	long long m_prev_timestamp;

public:
	byte* JT1078PackData(JTRTHead head, byte* data, int &len);

private:
	JTRTPackData m_jt1078_packdata;

public:
	JTRTHead m_jt1078_head;

};


#pragma once

#include "IH264AacPackToFmp4.h"

class H264AacPackToFmp4Man
{
public:
	H264AacPackToFmp4Man();
	~H264AacPackToFmp4Man();

	int Init(Fmp4Data call_back, long arg);

	int InputAudio(uint8_t *data, int data_size, int64_t timestamp);

	int InputH264(uint8_t *data, int data_size, int64_t timestamp);

	void Deinit();

private://≤π“Ù∆µœ‡πÿ
	void FillAudioInit();
	bool UpdateAudioTimestamp(int64_t timestamp);
	void UpdateVideoTimestamp(int64_t timestamp);
	
	bool m_timestamp_unite_prepare;
	int64_t m_timestamp_video_last;
	int64_t m_timestamp_video_last_use;
	int64_t m_timestamp_audio_differ;
	int64_t m_timestamp_video_differ;

	bool m_fillao_first;
	int64_t m_fillao_last_audio_pts;
	int64_t m_fillao_last_video_pts;
	bool m_fillao_video_continuity;

	int64_t m_fillao_fill_audio_pts;

	int m_delay_ms_fill_audio;
private:
	IH264AacPackToFmp4 *m_to_fmp4;

};


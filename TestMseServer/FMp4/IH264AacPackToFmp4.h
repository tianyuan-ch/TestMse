#pragma once

#include "utility/FfmpegHeader.h"

typedef void(*Fmp4Data)(long arg, uint8_t *buf, int buf_size);

class IH264AacPackToFmp4
{
public:
	virtual ~IH264AacPackToFmp4() {};

	virtual int Init(Fmp4Data call_back, long arg) = 0;
	virtual int InputAAC(AVPacket* packet) = 0;
	virtual int InputH264(AVPacket* packet) = 0;
	virtual int Deinit() = 0;
};
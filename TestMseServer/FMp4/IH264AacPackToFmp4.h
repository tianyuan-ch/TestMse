#pragma once

#include "utility/FfmpegHeader.h"

typedef void(*Fmp4Data)(int arg, uint8_t *buf, int buf_size);

class IH264AacPackToFmp4
{
public:
	virtual ~IH264AacPackToFmp4() {};

	virtual int Init(Fmp4Data call_back, int arg) = 0;
	virtual int InputAAC(AVPacket* packet) = 0;
	virtual int InputH264(AVPacket* packet) = 0;
	virtual int Deinit() = 0;
};
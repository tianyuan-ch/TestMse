#pragma once
class H264Utils
{
public:
	H264Utils();
	~H264Utils();

	static unsigned char* H264GenerateExtradataFromIFrame(unsigned char* stream_h264, int length, int &len);

	static bool H264DecodeSPS(unsigned char* buf, unsigned int length, int &width, int &height);

	static bool H264GetInfo(unsigned char *buf, unsigned int length, int &width, int &height, bool &is_key);
};


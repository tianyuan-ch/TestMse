#ifndef _CM_COLORSPACE_CONVERT_H_
#define _CM_COLORSPACE_CONVERT_H_

namespace cm {

void YV12ToRGB24(unsigned char* pRGB24, unsigned char* pYV0, unsigned char* pYV1, unsigned char* pYV2, int nStride0, int nStride1, int nStride2, int nWidth, int nHeight);
void YUV420ToYV12(unsigned char* pYV12, unsigned char* pYUV420, int nWidth, int nHeight);
void RGB24ToYV12(unsigned char* pYV12, unsigned char* pRGB, int nWidth, int nHeight);
void RGB32ToRGB24(unsigned char* pRGB24, unsigned char* pRGB32, int nWidth, int nHeight, int nWidthStride, int nHeightStride);

}

#endif
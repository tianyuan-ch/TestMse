#include "H264Utils.h"

#include "FfmpegHeader.h"
#include <stdio.h>
#include <math.h>

#pragma region private

static int unsigned_encode(unsigned char* buf, unsigned int length, unsigned int &startbit)
{
	unsigned int zeroNum = 0;
	while (startbit < length * 8)
	{
		if (((unsigned char)buf[startbit / 8] & (0x80 >> (int)(startbit % 8))) != 0)
		{
			break;
		}
		++zeroNum;
		++startbit;
	}
	++startbit;

	unsigned int ret = 0;
	for (unsigned int i = 0; i < zeroNum; ++i)
	{
		ret <<= 1;
		if (((unsigned char)buf[startbit / 8] & (0x80 >> (int)(startbit % 8))) != 0)
		{
			ret += 1;
		}
		++startbit;
	}

	return (int)((1 << (int)zeroNum) - 1 + ret);
}
static int signed_encode(unsigned char* buf, unsigned int length, unsigned int &startbit)
{
	int ueval = (int)unsigned_encode(buf, length, startbit);
	double k = ueval;
	int value = (int)ceil(k / 2);
	if (ueval % 2 == 0)
	{
		value = -value;
	}
	return value;
}
static int u(unsigned int bitcount, unsigned char* buf, unsigned int &startbit)
{
	int ret = 0;
	for (unsigned int i = 0; i < bitcount; ++i)
	{
		ret <<= 1;
		if (((unsigned char)buf[startbit / 8] & (0x80 >> (int)(startbit % 8))) != 0)
		{
			ret += 1;
		}
		startbit++;
	}
	return ret;
}
static int FindNext0x000001(unsigned char* stream_h264, int &i, int max)
{
	int nal_start = i;
	i++;
	for (; i < max; i++)
	{
		if (stream_h264[i + 0] == 0x00 && stream_h264[i + 1] == 0x00 && stream_h264[i + 2] == 0x01)
		{
			if (i > 1 && stream_h264[i - 1] == 0)
				return i - 1 - nal_start;
			return i - nal_start;
		}
	}
	return 0;
}

#pragma endregion



H264Utils::H264Utils()
{
}


H264Utils::~H264Utils()
{
}

unsigned char* H264Utils::H264GenerateExtradataFromIFrame(unsigned char* stream_h264, int length, int &len)
{
	int size;
	int sps_size;
	int pps_size;
	int max = length < 200 ? length : 200;
	int sei_size = 0;
	unsigned char *sps = NULL;
	unsigned char *pps = NULL;



	for (int i = 0; i < max + sei_size; i++)
	{
		if (stream_h264[i + 0] == 0x00 && stream_h264[i + 1] == 0x00 && stream_h264[i + 2] == 0x01)
		{

			int nal_unit_type = stream_h264[i + 3] & 31;

			//sei 可能很长
			if (nal_unit_type == 6)
			{
				i += 3;
				sei_size = FindNext0x000001(stream_h264, i, length - i);
				sei_size += 3;
				i--;
				continue;
			}

			//sps
			if (nal_unit_type == 7)
			{
				sps = stream_h264 + i;
				sps_size = FindNext0x000001(stream_h264, i, max + sei_size);
				if (sps_size <= 0)
					break;
				i--;
				//sps[0] = (size >> 24) & 0x000000ff;
				//sps[1] = (size >> 16) & 0x000000ff;
				//sps[2] = (size >> 8) & 0x000000ff;
				//sps[3] = (size) & 0x000000ff;
				continue;
			}

			//pps
			if (nal_unit_type == 8)
			{
				pps = stream_h264 + i;
				pps_size = FindNext0x000001(stream_h264, i, max + sei_size);
				if (pps_size <= 0)
					break;
				i--;
				continue;
			}

			if (nal_unit_type == 5 && sps != NULL && pps != NULL)
			{
				uint8_t *extradata = (uint8_t*)av_mallocz(pps_size + sps_size + AV_INPUT_BUFFER_PADDING_SIZE);

				memcpy(extradata, sps, sps_size);
				memcpy(extradata + sps_size, pps, pps_size);

				len = pps_size + sps_size;

				return extradata;
			}
		}


	}

	return NULL;
}

bool H264Utils::H264DecodeSPS(unsigned char* buf, unsigned int length, int &width, int &height)
{
	unsigned int startbit = 0;
	int forbidden_zero_bit = u(1, buf, startbit);
	int nal_ref_idc = u(2, buf, startbit);
	int nal_unit_type = u(5, buf, startbit);
	if (nal_unit_type == 7)
	{
		int profile_idc = u(8, buf, startbit);
		int constraint_set_flag = 0;
		constraint_set_flag |= u(1, buf, startbit) << 0;
		constraint_set_flag |= u(1, buf, startbit) << 1;
		constraint_set_flag |= u(1, buf, startbit) << 2;
		constraint_set_flag |= u(1, buf, startbit) << 3;
		constraint_set_flag |= u(1, buf, startbit) << 4;
		constraint_set_flag |= u(1, buf, startbit) << 5;
		int reserved_zero_2bits = u(2, buf, startbit);
		int level_idc = u(8, buf, startbit);

		int seq_parameter_set_id = unsigned_encode(buf, length, startbit);

		if (profile_idc == 100 ||  // High profile
			profile_idc == 110 ||  // High10 profile
			profile_idc == 122 ||  // High422 profile
			profile_idc == 244 ||  // High444 Predictive profile
			profile_idc == 44 ||  // Cavlc444 profile
			profile_idc == 83 ||  // Scalable Constrained High profile (SVC)
			profile_idc == 86 ||  // Scalable High Intra profile (SVC)
			profile_idc == 118 ||  // Stereo High profile (MVC)
			profile_idc == 128 ||  // Multiview High profile (MVC)
			profile_idc == 138 ||  // Multiview Depth High profile (MVCD)
			profile_idc == 144)   // old High444 profile
		{
			int chroma_format_idc = unsigned_encode(buf, length, startbit);
			if (chroma_format_idc == 3)
				/*int residual_colour_transform_flag = */u(1, buf, startbit);
			int bit_depth_luma_minus8 = unsigned_encode(buf, length, startbit);
			int bit_depth_chroma_minus8 = unsigned_encode(buf, length, startbit);
			int qpprime_y_zero_transform_bypass_flag = u(1, buf, startbit);
			int seq_scaling_matrix_present_flag = u(1, buf, startbit);

			int seq_scaling_list_present_flag/*[8]*/;
			if (seq_scaling_matrix_present_flag != 0)
			{
				int i, j;
				for (i = 0; i < 6; ++i)
				{
					int last = 8, next = 8, size = 16;

					seq_scaling_list_present_flag/*[i]*/ = u(1, buf, startbit);
					if (seq_scaling_list_present_flag != 0)
					{
						for (j = 0; j < size; j++)
						{
							if (next != 0)
							{
								int v = signed_encode(buf, length, startbit);
								next = (last + v) & 0xff;
							}

							if (next == 0)
								break;
							last = (next != 0) ? next : last;
						}
					}
				}

				for (i = 6; i < 8; i++)
				{
					int last = 8, next = 8, size = 64;

					seq_scaling_list_present_flag/*[i]*/ = u(1, buf, startbit);
					if (seq_scaling_list_present_flag != 0)
					{
						for (j = 0; j < size; j++)
						{
							if (next != 0)
							{
								int v = signed_encode(buf, length, startbit);
								next = (last + v) & 0xff;
							}
							if (next == 0)
								break;
							last = (next != 0) ? next : last;
						}

					}
				}

			}
		}

		int log2_max_frame_num_minus4 = unsigned_encode(buf, length, startbit);
		int pic_order_cnt_type = unsigned_encode(buf, length, startbit);
		if (pic_order_cnt_type == 0)
		{
			int log2_max_pic_order_cnt_lsb_minus4 = unsigned_encode(buf, length, startbit);
		}
		else if (pic_order_cnt_type == 1)
		{
			int delta_pic_order_always_zero_flag = u(1, buf, startbit);
			int offset_for_non_ref_pic = signed_encode(buf, length, startbit);
			int offset_for_top_to_bottom_field = signed_encode(buf, length, startbit);
			int num_ref_frames_in_pic_order_cnt_cycle = unsigned_encode(buf, length, startbit);

			int/***/ offset_for_ref_frame /*= new int[num_ref_frames_in_pic_order_cnt_cycle]*/;
			for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i)
			{
				offset_for_ref_frame/*[i]*/ = signed_encode(buf, length, startbit);
			}
			//delete[] offset_for_ref_frame;
		}

		int num_ref_frames = unsigned_encode(buf, length, startbit);
		int gaps_in_frame_num_value_allowed_flag = u(1, buf, startbit);
		int pic_width_in_mbs_minus1 = unsigned_encode(buf, length, startbit);
		int pic_height_in_map_units_minus1 = unsigned_encode(buf, length, startbit);

		width = (pic_width_in_mbs_minus1 + 1) * 16;
		height = (pic_height_in_map_units_minus1 + 1) * 16;
		return true;
	}
	else
	{
		return false;
	}
}

bool H264Utils::H264GetInfo(unsigned char *stream_h264, unsigned int length, int &width, int &height, bool &is_key)
{
	int size;
	int max = length < 500 ? length : 500;
	is_key = false;

	int sei_size = 0;

	for (int i = 0; i < max + sei_size; i++)
	{
		if (stream_h264[i + 0] == 0x00 && stream_h264[i + 1] == 0x00 && stream_h264[i + 2] == 0x01)
		{
			int nal_unit_type = stream_h264[i + 3] & 31;

			//sei 可能很长
			if (nal_unit_type == 6)
			{
				i += 3;
				sei_size = FindNext0x000001(stream_h264, i, length - i);
				sei_size += 3;
				i--;


				continue;
			}

			//sps
			if (nal_unit_type == 7)
			{
				i += 3;
				unsigned char *sps = stream_h264 + i;
				size = FindNext0x000001(stream_h264, i, max + sei_size);
				i--;
				if (size <= 0)
					break;

				is_key = true;

				H264DecodeSPS(sps, size, width, height);

				continue;
			}

			//pps
			if (nal_unit_type == 8)
			{
				is_key = true;

				continue;
			}

			if (nal_unit_type == 5)
			{
				is_key = true;

				return true;
			}
			if (nal_unit_type == 1)
			{
				is_key = false;
				return true;
			}
		}
	}
	return false;
}
#include "cm_string_coding.h"



namespace cm{

#ifdef WIN32
	#include <windows.h>
	void gb2312_to_unicode(WCHAR* out,char *text)
	{
		MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,text,2,out,1);
		return;
	}
	void unicode_to_utf8(char* out, WCHAR* text)
	{
		// 注意 WCHAR高低字的顺序,低字节在前，高字节在后
		char* pchar = (char *)text;

		out[0] = (0xE0 | ((pchar[1] & 0xF0) >> 4));
		out[1] = (0x80 | ((pchar[1] & 0x0F) << 2)) + ((pchar[0] & 0xC0) >> 6);
		out[2] = (0x80 | (pchar[0] & 0x3F));

		return;
	}
	void win32_string_gb2312_to_utf8(char *text, int len, std::string& out)
	{
		char buf[4];
		memset(buf,0,4);

		out.clear();

		int i = 0;
		while(i < len)
		{
			//如果是英文直接复制就可以
			if( text[i] >= 0)
			{
				char asciistr[2]={0};
				asciistr[0] = (text[i++]);
				out.append(asciistr);
			}
			else
			{
				WCHAR w_buffer;
				gb2312_to_unicode(&w_buffer,text+i);

				unicode_to_utf8(buf,&w_buffer);

				out.append(buf);

				i += 2;
			}
		}

		return;
	}
#else
	#include <iconv.h>
	int linux_string_gb2312_to_utf8(char *text, int len, std::string& out)
	{
		iconv_t conv;

		out.clear();

		conv = iconv_open("utf-8", "gb2312");
		if(conv == 0)
		{
			return -1;
		}

		char c_out[1024];
		unsigned int out_len = 1024;
		unsigned int in_len = len;
		if(iconv(conv, &text, &in_len, (char**)&c_out, &out_len) == -1)
		{
			iconv_close(conv);
			return -1;
		}

		out = c_out;
		iconv_close(conv);
		return 0;
	}
#endif

	int cm_string_gb2312_to_utf8(char *text, int len, std::string& out)
	{
#ifdef WIN32
		win32_string_gb2312_to_utf8(text, len, out);
		return 0;
#else
		return linux_string_gb2312_to_utf8(text, len, out);
#endif
	}



}
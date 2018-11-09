#include "FFmpegBinariesHelper.h"

#ifdef  WIN32
#include <direct.h>
#include <string>
#include <io.h>
#include <Windows.h>
#else
#include <unistd.h>
#endif



FFmpegBinariesHelper::FFmpegBinariesHelper()
{
}


FFmpegBinariesHelper::~FFmpegBinariesHelper()
{
}


void FFmpegBinariesHelper::RegisterFFmpegBinaries()
{
	char szPath[265];
	std::string strPath = _getcwd(szPath, 265);
	std::string ffmpegPath;
	std::string fullPath;
#ifdef WIN32
#ifndef _WIN64
	ffmpegPath = "/FFmpeg/Windows/bin/x64";
#else
	ffmpegPath = "/FFmpeg/Windows/bin/x86";
#endif
	char *pContext;
	while (true)
	{
		fullPath = strPath + ffmpegPath;
		if (_access(fullPath.c_str(), 0) == 0)
		{
			SetDllDirectory(strPath.c_str());
			break;
		}

		int pos = strPath.find_last_of("\\");
		if (pos == -1)
			return;
		strPath = strPath.substr(0, pos);
	}
#else

#endif
}
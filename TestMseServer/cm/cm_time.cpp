#include "cm_time.h"


#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif



namespace cm{


	uint32_t cm_gettickcount()
	{
		uint32_t currentTime;

#ifdef WIN32
		currentTime = GetTickCount();
#else
		struct timeval current;
		gettimeofday(&current, 0);
		currentTime = current.tv_sec * 1000 + current.tv_usec/1000;
#endif
		return currentTime;
	}



}
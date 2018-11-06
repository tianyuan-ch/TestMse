#include "cm_thread.h"


#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace cm {

	void cm_sleep(unsigned int msec)
	{
#ifdef WIN32
		Sleep(msec);
#else
		usleep(msec * 1000);
#endif
	}

	void* ThreadFun(void *param)
	{
		CBaseThread *base_thread = (CBaseThread*)param;
		base_thread->ThreadMain();

		return NULL;
	}

	bool CBaseThread::Start()
	{
		if(cm_pthread_create(&thread_id, NULL, ThreadFun, this))
			return false;
		return true;
	}

	void CBaseThread::Stop()
	{
		cm_pthread_join(thread_id, NULL);
	}

}


#ifndef _CM_THREAD_H_
#define _CM_THREAD_H_

#ifdef WIN32
#include "cm_win32thread.h"
#else
#include <pthread.h>
#define cm_pthread_t               pthread_t
#define cm_pthread_attr_t          pthread_attr_t
#define cm_pthread_create          pthread_create
#define cm_pthread_join            pthread_join

#endif

namespace cm {

	void cm_sleep(unsigned int msec);


	class CBaseThread
	{
	public:
		CBaseThread(void) {};
		~CBaseThread(void) {};

		bool Start();
		void Stop();

		virtual void ThreadMain() = 0;

	private:
		cm_pthread_t thread_id;

	};

}



#endif
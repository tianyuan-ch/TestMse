#include "cm_win32thread.h"
#include <process.h> 

namespace cm {

	static unsigned __stdcall cm_win32thread_worker( void *arg )
	{
		cm_pthread_t *h = (cm_pthread_t*)arg;
		*h->p_ret = h->func( h->arg );
		return 0;
	}
	
	int cm_pthread_create(cm_pthread_t *thread, const cm_pthread_attr_t *attr, void *(*start_routine)( void* ), void *arg)
	{
		thread->func   = start_routine;
		thread->arg    = arg;
		thread->p_ret  = &thread->ret;
		thread->ret    = NULL;
		thread->handle = (void*)_beginthreadex( NULL, 0, cm_win32thread_worker, thread, 0, NULL );
		return !thread->handle;
	}

	int cm_pthread_join(cm_pthread_t thread, void **value_ptr)
	{
		DWORD ret = WaitForSingleObject(thread.handle, INFINITE);
		if(ret != WAIT_OBJECT_0)
		{
			return -1;
		}
		if( value_ptr )
			*value_ptr = *thread.p_ret;
		CloseHandle(thread.handle);
		return 0;
	}
}


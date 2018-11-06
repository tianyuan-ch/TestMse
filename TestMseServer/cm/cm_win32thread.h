#ifndef _CM_WIN32_THREAD_H_
#define _CM_WIN32_THREAD_H_

#include <windows.h>

namespace cm {

	typedef struct
	{
		void *handle;
		void *(*func)( void* arg );
		void *arg;
		void **p_ret;
		void *ret;
	} cm_pthread_t;

	#define cm_pthread_attr_t int
	
	int cm_pthread_create(cm_pthread_t *thread, const cm_pthread_attr_t *attr, void *(*start_routine)( void* ), void *arg);
	int cm_pthread_join(cm_pthread_t thread, void **value_ptr);

	//int cm_pthread_mutex_init(cm_pthread_mutex_t *mutex, const cm_pthread_mutexattr_t *attr );
	//int cm_pthread_mutex_destroy(cm_pthread_mutex_t *mutex );
	//int cm_pthread_mutex_lock(cm_pthread_mutex_t *mutex );
	//int cm_pthread_mutex_unlock(cm_pthread_mutex_t *mutex );

	//int cm_pthread_cond_init(cm_pthread_cond_t *cond, const cm_pthread_condattr_t *attr );
	//int cm_pthread_cond_destroy(cm_pthread_cond_t *cond );
	//int cm_pthread_cond_broadcast(cm_pthread_cond_t *cond );
	//int cm_pthread_cond_wait(cm_pthread_cond_t *cond, cm_pthread_mutex_t *mutex );
	//int cm_pthread_cond_signal(cm_pthread_cond_t *cond );
}



#endif
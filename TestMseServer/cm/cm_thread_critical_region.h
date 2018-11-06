#ifndef THREAD_SYNC_H
#define THREAD_SYNC_H


#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace cm{

class CBaseLock
{
public:
	CBaseLock() {};
	virtual ~CBaseLock() {};

public:
	virtual void Lock() = 0;
	virtual void Unlock() = 0;
};

#ifdef _WIN32
class CWinCSLock : public CBaseLock
{
public:
	CWinCSLock() { InitializeCriticalSection(&m_cs_); }
	virtual ~CWinCSLock() { DeleteCriticalSection(&m_cs_); }

	virtual void Lock() { EnterCriticalSection(&m_cs_); }
	virtual void Unlock() { LeaveCriticalSection(&m_cs_); }
private:
	CRITICAL_SECTION m_cs_;
};
#else
class CLinuxMutexLock : public CBaseLock
{
public:
	CLinuxMutexLock() { pthread_mutex_init(&m_mutex_, NULL); }
	virtual ~CLinuxMutexLock() { pthread_mutex_destroy(&m_mutex_); }

	virtual void Lock() { pthread_mutex_lock(&m_mutex_); }
	virtual void Unlock() { pthread_mutex_unlock(&m_mutex_); }

private:
	pthread_mutex_t m_mutex_;
};

#endif

class CAutoLock
{
public:
	CAutoLock(CBaseLock* lock);
	~CAutoLock();
private:
	CBaseLock *lock_;
};

extern CBaseLock* CreateLock();

}

#endif
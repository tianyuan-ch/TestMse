#include "cm_thread_critical_region.h"

namespace cm{

CAutoLock::CAutoLock(CBaseLock* lock) : lock_(lock)
{
	if(lock_)
		lock_->Lock(); 
}

CAutoLock::~CAutoLock()
{
	if(lock_)
	{
		lock_->Unlock();
	}
}

CBaseLock* CreateLock()
{
#ifdef _WIN32
	return new CWinCSLock();
#else
	return new CLinuxMutexLock();
#endif
}

}
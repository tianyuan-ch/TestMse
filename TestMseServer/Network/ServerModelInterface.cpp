
#include "ServerModelInterface.h"



#ifdef WIN32
#include "IOCP/IOCPModel.h"

CServerModelInterface* NewServer(CServerModelUser *pUser)
{
	return new CIOCPModel(pUser);
}

#else

#include "epoll_server/EPollModel.h"

CServerModelInterface* NewServer(CServerModelUser *pUser)
{
	return new CEPollModel(pUser);
}



#endif
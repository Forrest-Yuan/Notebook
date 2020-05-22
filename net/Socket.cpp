#include <list>
#include <atomic>
#include <unistd.h>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "errno.h"
#include "log.h"
#include "Socket.h"

Socket::Socket() : state_(false), socketFd_(-1)
{

}

Socket::~Socket()
{

}

int32_t Socket::init()
{
	return 0;
}

int32_t Socket::uinit()
{
	if (socketFd_ >= 0)
	{
		close(socketFd_);
	}

	socketFd_ = -1;
	state_ = false;
	return 0;
}

int32_t Socket::ServerInit()
{
	int32_t socketLis_ = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == socketLis_) {
		LOGE("socket created");
		return -1;
	}
	
	int flags  = fcntl(socketLis_, F_GETFL, 0);
	fcntl(socketLis_, F_SETFL, flags&~O_NONBLOCK);
	
	struct sockaddr_in server;    
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(8899);
	server.sin_addr.s_addr = INADDR_ANY;

	int32_t bindres = bind(socketLis_, (struct sockaddr*)&server, sizeof(server));
	if(-1 == bindres) {
		LOGE("socket bind error");
		return -1;
	}

	int32_t listenres = listen(socketLis_, SOMAXCONN);
	if(-1 == listenres) {
		LOGE("socket listen error");
		return -1;
	}

	return 0;
}

int32_t Socket::serverAccept()
{
	struct sockaddr_in peerServer;
	int32_t acceptfd = -1;
	socklen_t len = sizeof(peerServer);
	acceptfd = accept(socketLis_, (struct sockaddr*)NULL, NULL);
	//if(-1 == acceptfd) {
	//	LOGE("socket accept error %s", strerror(errno));
	//	return -1;
	//}
	socketFd_ = acceptfd;
LOGI("Forrest socket connected");	
	state_ = true;
	return 0;
}

bool Socket::connectServer(std::string strIP, int16_t port)
{
	if((strIP.empty()) || (port <= 0))
	{
		LOGE("bad server address");
		return false;
	}
	
	uinit();
    int cliFd = socket(PF_INET, SOCK_STREAM, 0);
    if (cliFd < 0)
    {
        LOGI("socket error!");
        return false;
    }
    //connect
    struct sockaddr_in servAddr = { 0 };
    servAddr.sin_family = PF_INET;
    servAddr.sin_port = htons(port);
    //servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servAddr.sin_addr.s_addr = inet_addr(strIP.c_str());

    int ret = connect(cliFd, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (ret < 0)
    {
        LOGI("connect error ret %d! strIP.c_str() %s port %d", ret, strIP.c_str(), port);
        close(cliFd);
        return false;
    }
    socketFd_ = cliFd;
	state_ = true;

	LOGI("Connected to server success");
    return true;
}

bool Socket::sendMsg(int8_t* pData, uint32_t length)
{
    if (socketFd_ <= 0) 
	{
        return false;
    }

	uint32_t offset = 0;
	int32_t sendLen = 0;
	do
	{
		sendLen = send(socketFd_, (int8_t *)(pData + offset), length - offset, 0);
		if(sendLen < 0)
		{
			 close(socketFd_);
			 socketFd_ = -1;
			 state_ = false;
			return false;
		}

		offset += sendLen;
	}while(offset < length);

    return true;
}

bool Socket::recvMsg(int8_t* pData, uint32_t length)
{
	if((socketFd_ <= 0) || (NULL == pData) || (length <= 0))
	{
		LOGE("invalid socket, socketFd_ %d pData %p length %u", socketFd_, pData, length);
        return false;
    }

	uint32_t offset = 0;
	int32_t recvLen = 0;
	do
	{
		recvLen = recv(socketFd_, (int8_t*)(pData + offset), length - offset, 0);
		if(0 == recvLen)
		{
			LOGI("read closed");
			state_ = false;
			return false;
		}
		else if( recvLen < 0)
		{
			if(errno == EINTR || errno == EWOULDBLOCK)
			{
				//LOGE("read error = %s", strerror(errno));
				recvLen = 0;
				usleep(1000);
			}
			else
			{
				state_ = false;
				return false;
			}
		}

		offset += recvLen;
	}while(offset < length);

	return true;
}
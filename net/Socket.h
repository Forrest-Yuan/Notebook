#ifndef _RW_RECADAS_SOCKET_H_
#define _RW_RECADAS_SOCKET_H_

class Socket
{
public:

	Socket();
	~Socket();

	int32_t init();
	int32_t uinit();
	int32_t ServerInit();
	int32_t serverAccept();
	bool connectServer(std::string strIP, int16_t port);
	
	bool sendMsg(int8_t* pData, uint32_t len);
	bool recvMsg(int8_t* pData, uint32_t len);
	int state() { return state_; };
private:

	int32_t socketFd_;
	int32_t socketLis_;
	std::atomic<bool> state_;//当前连接所属状态，状态机
};

#endif

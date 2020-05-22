#ifndef _RW_RECADAS_VIRTUAL_CAMERA_H_
#define _RW_RECADAS_VIRTUAL_CAMERA_H_

class VirtualCameraThread : public BaseThread
{
public:
#define MAX_SIZE_OF_TEMP_MEMORY 1024*1024
static const int IMAGE_POOL_SIZE = 6;

#pragma pack(1)
	typedef struct MSG_HEAD
	{
		uint8_t syncHead1;
		uint8_t syncHead2;
		//uint32_t dataLen;
		uint16_t dataLen;
		MSG_HEAD()
		{
			syncHead1 = 0x5A;
			syncHead2 = 0x5A;
			dataLen = 0;
		}
	}msg_head;
	typedef struct MSG_BODY
	{
		uint8_t channelID;
		uint32_t frameID;
		MSG_BODY()
		{
			channelID = 0;
			frameID = 0;
		}
	}mgs_body;
#pragma pack()

	VirtualCameraThread(std::string IP, int16_t port);
	virtual ~VirtualCameraThread();

	virtual int32_t init(void* priData);
	virtual void uninit();

	virtual void run();
private:
	int32_t getFrame(uint8_t* pbuf, uint32_t bufLen);
	int32_t decodeFrame(uint8_t* pbuf, MSG_HEAD*pHead, MSG_BODY* pBody, uint8_t** pJpg);
	int32_t jpgToYuv(uint8_t* pbuf, uint32_t jpgLen, SmartRecoImage& image);
	int32_t setSmartImage(MSG_HEAD head, MSG_BODY body, SmartRecoImage& image);

private:
	uint8_t* tempMem_;
	Socket* socket_;
	std::string strIP_;
	int16_t port_;
	ImagePool imagePool_;
	MjpgDecoder*   _mjpgDecoder;
};
#endif

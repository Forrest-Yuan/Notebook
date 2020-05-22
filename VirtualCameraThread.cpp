//#include "sfd_face.h"
#include <stdio.h>
#include <memory>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>


#include "log.h"
#include "BaseThread.h"
#include "ImagePool.h"
#include "AppContexts.h"

#include "Socket.h"
#include "jpg_queue.h"
#include "img_store.h"
#include "VirtualCameraThread.h"

#include "NetService.h"

VirtualCameraThread::VirtualCameraThread(std::string IP, int16_t port) : socket_(nullptr)
									, tempMem_(nullptr)
									, strIP_(IP)
									, port_(port)
{
	//strIP_.assign("192.168.100.100");
	//port_ = 8899;
}

VirtualCameraThread::~VirtualCameraThread()
{
	uninit();
}

int32_t VirtualCameraThread::init(void* priData)
{
	if(nullptr == tempMem_)
	{
		tempMem_ = (uint8_t*)malloc(MAX_SIZE_OF_TEMP_MEMORY);
		if(nullptr == tempMem_)
		{
			LOGE("alloc mem fail");
			return -1;
		}
		else
		{
			memset(tempMem_, 0, MAX_SIZE_OF_TEMP_MEMORY);
		}
	}

	int ret = imagePool_.init(appCtx()->idemConfig->videoWidth(IdemConfig::adas),
								  appCtx()->idemConfig->videoWidth(IdemConfig::adas),
								  RecoImage::NV21_TYPE, IMAGE_POOL_SIZE + 3 );
	if( 0 != ret )
	{
		LOGE("image pool init failed!");
		return -2;
	}

	socket_ = new Socket();
	if(nullptr == socket_)
	{
		LOGE("alloc socket fail");
		return -1;
	}	
	socket_->init();
	_mjpgDecoder = MjpgDecoder::getInstance(appCtx()->idemConfig->videoWidth(IdemConfig::adas), 
												appCtx()->idemConfig->videoHeight(IdemConfig::adas));
	return 0;
}

void VirtualCameraThread::uninit()
{
	if(nullptr != tempMem_)
	{
		free(tempMem_);
		tempMem_ = nullptr;
	}

	if(nullptr != socket_)
	{
		delete socket_;
		socket_ = nullptr;
	}
}

int32_t VirtualCameraThread::getFrame(uint8_t* pbuf, uint32_t bufLen)
{
	memset(pbuf, 0, bufLen);

	bool ret = socket_->recvMsg((int8_t*)pbuf, sizeof(MSG_HEAD));
	if(!ret)
	{
		LOGE("recv data error");
		return -1;
	}
	
	MSG_HEAD* pMsgHead = (MSG_HEAD*)pbuf;
	uint32_t dataLen = ntohs(pMsgHead->dataLen);
	if((dataLen <=0) || (dataLen > ((1280 * 720) + sizeof(MSG_BODY))))
	{
		LOGE("received bad data, dataLen %u", dataLen);
		return -1;
	}
	
	if(!((0x5A == pMsgHead->syncHead1) && (0x5A == pMsgHead->syncHead2)))
	{
		LOGE("received bad data");
		return -1;
	}
	
	ret = socket_->recvMsg((int8_t*)(pbuf + sizeof(MSG_HEAD)), dataLen);	
	if(!ret)
	{
		LOGE("recv data error");
		return -1;
	}
#if 0
	// save the alarm image
	{
		static uint64_t cnt = 0;
		std::string filename("/mnt/sdcard/over_");
		uint64_t localTime = time(NULL);
		filename += file_util::getTimeString(localTime); 
		filename += file_util::getTimeString(++cnt); 
		filename += ".jpg";
		int f = open(filename.c_str(),	O_WRONLY | O_CREAT | O_TRUNC);

		if( f < 0 )// (-1 == f)
		{
			LOGE("store_image: open file failed=%d %s! filename=%s\n", errno, strerror( errno ),filename.c_str());
			return -1;
		}
	
		int jpg_size = image->getDataSize();
		int len = write(f, pbuf + sizeof(MSG_HEAD) + sizeof(MSG_BODY), dataLen - sizeof(MSG_BODY));
		if (len == jpg_size)
		{
			fsync(f);
		}
		close(f);

	}
#endif

	return 0;
}

int32_t VirtualCameraThread::decodeFrame(uint8_t* pbuf, MSG_HEAD*pHead, MSG_BODY* pBody, uint8_t** pJpg)
{
	if((NULL == pbuf) || (NULL == pJpg))
	{
		LOGE("bad address, pbuf %p pJpg %p", pbuf, pJpg);
		return -1;
	}

	*pHead = *((MSG_HEAD*)pbuf);
	*pBody = *(MSG_BODY*)((pbuf + sizeof(MSG_HEAD)));
	*pJpg = pbuf + sizeof(MSG_HEAD) + sizeof(MSG_BODY);

	return 0;
}

int32_t VirtualCameraThread::jpgToYuv(uint8_t* pbuf, uint32_t jpgLen, SmartRecoImage& image)
{
	if(image.isValid())
	{
		int32_t mjpgRet = _mjpgDecoder->decoder_write(pbuf, jpgLen);
		if (mjpgRet < 0)
		{
			LOGE("decoder_write error");
			return -1;
		}
		_mjpgDecoder->decoder_decode();

		mjpgRet = _mjpgDecoder->decoder_read(image->getData(), image->getImageSize(), appCtx()->idemConfig->videoWidth(IdemConfig::adas), 
												appCtx()->idemConfig->videoHeight(IdemConfig::adas));
		if (mjpgRet < 0)
		{
			LOGE("decoder_read error");
			return -1;
		}
	}

#if 0
		static int pCnt = 0;
		char sCnt[8];
		char sscore[16];
		int m_ptz_fp = 0;
		sprintf(sCnt, "%d", pCnt++);
		int64_t stimeStamp = file_util::getMilliSecond();
		sprintf(sscore, "_%lld", stimeStamp);
		std::string fileName = "/mnt/sdcard/log/";
		fileName += sCnt;
		fileName += sscore;
		fileName += ".yuv";
		
		m_ptz_fp = open(fileName.c_str(),  O_WRONLY | O_CREAT | O_TRUNC);
		if (-1 != m_ptz_fp)
		{	
			write(m_ptz_fp, image->getData(), image->getImageSize());
			fsync(m_ptz_fp);
			close(m_ptz_fp);
		}
		else
		{
			LOGE("open file error. file name %s", fileName.c_str());
		}
#endif
	return 0;
}

int32_t	VirtualCameraThread::setSmartImage(MSG_HEAD head, MSG_BODY body, SmartRecoImage& image)
{
	uint32_t frameID = ntohl(body.frameID);
	image->setFrameCount(frameID);

	uint32_t dataLen = ntohl(head.dataLen) - sizeof(MSG_BODY);
	image->setDataSize(dataLen);
	
	image->setTimeStamp(file_util::getMilliSecond());
	return 0;
}

void VirtualCameraThread::run()
{
	LOGI("*** tid = %ld ***", gettid());
	sleep(10);
	
	MSG_HEAD msgHead;
	MSG_BODY msgBody;
	uint8_t* pJpg = NULL;
	while(isRunning())
	{
		if(!socket_->state()){
			socket_->connectServer(strIP_, port_);
			sleep(1);
		}
		else
		{
			int ret = getFrame(tempMem_, MAX_SIZE_OF_TEMP_MEMORY);
			if(!ret)
			{
				decodeFrame(tempMem_, &msgHead, &msgBody, &pJpg);
				SmartRecoImage yuvImage = imagePool_.createImage();
				jpgToYuv(pJpg, ntohs(msgHead.dataLen) - sizeof(MSG_BODY), yuvImage);
				setSmartImage(msgHead, msgBody, yuvImage);
				
				appCtx()->bridgeEventDistribute().pushImage("EventImageVC", yuvImage); 
				//appCtx->getVCImageQueue()->push(yuvImage);				
			}
			else
			{
				LOGE("bad image");
			}
		}
		usleep(1000);
	}
}

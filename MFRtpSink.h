#pragma once

#include "MFPipeline.h"
#include "MFFilter.h"


class RTPSink;
class UsageEnvironment;
class H264VideoStreamFramer;
class RTSPServer;

class MFRtpSink :public MFFilter
{
	RTPSink* videoSink;
	UsageEnvironment* env;
	H264VideoStreamFramer* videoSource;
	RTSPServer* rtspServer;
	volatile char stopRTSP;

	std::thread* workerThread;

	INT64 videoFramesCount = 0;

	void Play();
	void ThreadProc();
	HRESULT WriteVideoSample(IMFSample* pSample, MFTIME duration);
public:

	MFRtpSink(MFPipeline* pipeline);
	~MFRtpSink();

	HRESULT Start();
	HRESULT Stop();
	//void AfterPlaying();
};


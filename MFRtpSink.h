#pragma once

#include "MFPipeline.h"
#include "MFFilter.h"


class MFRtpSink :public MFFilter
{
	std::thread* workerThread;

	INT64 videoFramesCount = 0;

	void ThreadProc();
	HRESULT WriteVideoSample(IMFSample* pSample, MFTIME duration);
public:
	MFRtpSink(MFPipeline* pipeline);
	~MFRtpSink();

	HRESULT Start();
};


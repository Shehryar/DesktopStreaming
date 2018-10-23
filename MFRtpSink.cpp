#include "MFRtpSink.h"
#include "UsageEnvironment.hh"
#include "BasicUsageEnvironment.hh"
#include "Groupsock.hh"
#include "liveMedia.hh"
#include "MFH264LiveSource.h"


MFRtpSink::MFRtpSink(MFPipeline* pipeline) :
	MFFilter(pipeline),
	videoFramesCount(0)
{
	_started = FALSE;
	Initiated = TRUE;
}


MFRtpSink::~MFRtpSink()
{
}

HRESULT MFRtpSink::Start()
{
	TraceD(L"RtpSink start");
	if (!Initiated)
	{
		Finished = TRUE;
		return E_FAIL;
	}

	_started = TRUE;

	workerThread = new std::thread(&MFRtpSink::ThreadProc, this);
	return S_OK;
}

void MFRtpSink::ThreadProc()
{
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

	in_addr dstAddr = { 127, 0, 0, 1 };
	Groupsock rtpGroupsock(*env, dstAddr, 1233, 255);
	rtpGroupsock.addDestination(dstAddr, 1234, 0);
	RTPSink * rtpSink = H264VideoRTPSink::createNew(*env, &rtpGroupsock, 96);

	MFH264LiveSource *mfH264Source = MFH264LiveSource::createNew(*env);
	rtpSink->startPlaying(*mfH264Source, NULL, NULL);

	/*while (!StopFlag)
	{
		if (!Pipeline->videoEncBuffer->empty())
		{
			IMFSample *vidsamp = NULL;

			vidsamp = Pipeline->videoEncBuffer->pop();
			if (vidsamp != NULL)
			{
				WriteVideoSample(vidsamp, 0);
				videoFramesCount++;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}*/

	// This function call does not return.
	env->taskScheduler().doEventLoop();

	TraceD(L"RtpSink end of thread");
}


HRESULT MFRtpSink::WriteVideoSample(IMFSample* pSample, MFTIME duration)
{
	HRESULT hr = S_OK;

	if (pSample == NULL)
		return E_POINTER;


	/*LONGLONG llTimeStamp = 0;
	pSample->GetSampleTime(&llTimeStamp);

	if (firstVideoSample)
	{
		baseVideoTime = llTimeStamp;
		firstVideoSample = FALSE;
	}

	llTimeStamp -= baseVideoTime;

	hr = pSample->SetSampleTime(llTimeStamp);

	MFTIME tempDuration = duration;
	if (tempDuration == 0)
		MFFrameRateToAverageTimePerFrame(m_videoFormat.FrameRateNum, m_videoFormat.FrameRateDen, (PUINT64)&tempDuration);

	EnterCriticalSection(&cs);
	if (SUCCEEDED(hr))
		hr = pSinkWriter->WriteSample(video_stream, pSample);

	if (SUCCEEDED(hr))
		videoDuration += tempDuration;
	LeaveCriticalSection(&cs);*/

	return hr;
}
#include "MFRtpSink.h"
#include "UsageEnvironment.hh"
#include "BasicUsageEnvironment.hh"


MFRtpSink::MFRtpSink(MFPipeline* pipeline) :
	MFFilter(pipeline),
	videoFramesCount(0)
{
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

	while (!StopFlag)
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
	}
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
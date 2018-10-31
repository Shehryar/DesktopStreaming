#include "MFH264LiveSource.h"
#include "GroupsockHelper.hh"

void MFH264LiveSource::TraceD(LPCTSTR lpszFormat, ...) const
{
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);

	::OutputDebugString(szBuffer);
	
	va_end(args);
}

bool MFH264LiveSource::isH264VideoStreamFramer() const {
	return true;
}

MFH264LiveSource::MFH264LiveSource(UsageEnvironment& env, MFPipeline *pipeline) :
	FramedSource(env),
	Pipeline(pipeline)
{
}


MFH264LiveSource::~MFH264LiveSource()
{
}

void MFH264LiveSource::doGetNextFrame()
{
	IMFSample *vidsamp = NULL;
	//while (Pipeline->videoEncBuffer->empty())
	while(true)
	{
		if (Pipeline->videoEncBuffer->empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); 
		else
		{
			vidsamp = Pipeline->videoEncBuffer->pop();
			if (vidsamp != NULL)
			{
			//	EnterCriticalSection(&cs);
				WriteVideoSample(vidsamp, 0);
				SafeRelease(&vidsamp);
			//	LeaveCriticalSection(&cs);
				break;
				//videoFramesCount++;
			}
		}
		
		/*if (Pipeline->videoEncBuffer->empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));*/
	}

	TraceD(L"MFH264LiveSource::doGetNextFrame\n");
}

HRESULT MFH264LiveSource::WriteVideoSample(IMFSample* pSample, MFTIME duration)
{
	HRESULT hr = S_OK;

	if (pSample == NULL)
		return E_POINTER;

	

	LONGLONG llTimeStamp = 0;
	pSample->GetSampleTime(&llTimeStamp);

	IMFMediaBuffer *buf = NULL;
	DWORD bufLength;
	hr = pSample->ConvertToContiguousBuffer(&buf);
	if (FAILED(hr))
		return hr;

	hr = buf->GetCurrentLength(&bufLength);
	if (FAILED(hr))
		return hr;

	BYTE * rawBuffer = NULL;

	////auto now = GetTickCount();

	////printf("Writing sample %i, spacing %I64dms, sample time %I64d, sample duration %I64d, sample size %i.\n", _frameCount, now - _lastSendAt, llVideoTimeStamp, llSampleDuration, bufLength);

	fFrameSize = bufLength;
	fDurationInMicroseconds = 0;
	gettimeofday(&fPresentationTime, NULL);

	buf->Lock(&rawBuffer, NULL, NULL);
	memmove(fTo, rawBuffer, fFrameSize);

	FramedSource::afterGetting(this);
	
	buf->Unlock();

	SafeRelease(&buf);
	
	
	/*if (firstVideoSample)
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
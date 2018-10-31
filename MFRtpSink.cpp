#include "MFRtpSink.h"
#include "UsageEnvironment.hh"
#include "BasicUsageEnvironment.hh"
#include "Groupsock.hh"
#include <GroupsockHelper.hh>
#include "liveMedia.hh"
#include "MFH264LiveSource.h"

char const* inputFileName = "test.264";

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

HRESULT MFRtpSink::Stop()
{
	MFFilter::Stop();

	stopRTSP = 1;

	return S_OK;
}

//void afterPlaying(void* clientData) 
//{
//	MFRtpSink *pSink = (MFRtpSink *)clientData;
//	pSink->AfterPlaying();
//}

void MFRtpSink::Play() 
{
	MFH264LiveSource *mfH264Source = MFH264LiveSource::createNew(*env, Pipeline);
	videoSource = H264VideoStreamFramer::createNew(*env, mfH264Source);
	//videoSink->startPlaying(*videoSource, NULL, NULL);
	videoSink->startPlaying(*mfH264Source, NULL, NULL);
}

//void  MFRtpSink::AfterPlaying() 
//{
//	*env << "...done reading from file\n";
//	videoSink->stopPlaying();
//	Medium::close(videoSource);
//	// Note that this also closes the input file that this source read from.
//
//	// Start playing once again:
//	Play();
//}

void MFRtpSink::ThreadProc()
{
	
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	env = BasicUsageEnvironment::createNew(*scheduler);

	// Create 'groupsocks' for RTP and RTCP:
	struct in_addr destinationAddress;
	destinationAddress.s_addr = chooseRandomIPv4SSMAddress(*env);
	// Note: This is a multicast address.  If you wish instead to stream
	// using unicast, then you should use the "testOnDemandRTSPServer"
	// test program - not this test program - as a model.


	const unsigned short rtpPortNum = 18888;
	const unsigned short rtcpPortNum = rtpPortNum + 1;
	const unsigned char ttl = 255;

	const Port rtpPort(rtpPortNum);
	const Port rtcpPort(rtcpPortNum);

	Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
	rtpGroupsock.multicastSendOnly(); // we're a SSM source
	Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);
	rtcpGroupsock.multicastSendOnly(); // we're a SSM source

	// Create a 'H264 Video RTP' sink from the RTP 'groupsock':
	OutPacketBuffer::maxSize = 100000;
	videoSink = H264VideoRTPSink::createNew(*env, &rtpGroupsock, 96);


	// Create (and start) a 'RTCP instance' for this RTP sink:
	const unsigned estimatedSessionBandwidth = 500; // in kbps; for RTCP b/w share
	const unsigned maxCNAMElen = 100;
	unsigned char CNAME[maxCNAMElen + 1];
	gethostname((char*)CNAME, maxCNAMElen);
	CNAME[maxCNAMElen] = '\0'; // just in case
	RTCPInstance* rtcp = RTCPInstance::createNew(*env, &rtcpGroupsock,
			estimatedSessionBandwidth, CNAME,
			videoSink, NULL /* we're a server */,
			True /* we're a SSM source */);

	// Note: This starts RTCP running automatically
	
	rtspServer = RTSPServer::createNew(*env, 8554);
	if (rtspServer == NULL) {
		*env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
		exit(1);
	}

	ServerMediaSession* sms	= ServerMediaSession::createNew(*env, "testStream", inputFileName,
			"Session streamed by \"testH264VideoStreamer\"", 	True /*SSM*/);
	sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
	rtspServer->addServerMediaSession(sms);

	char* url = rtspServer->rtspURL(sms);
	*env << "Play this stream using the URL \"" << url << "\"\n";
	delete[] url;

	// Start the streaming:
	*env << "Beginning streaming...\n";
	Play();

	stopRTSP = 0;
	env->taskScheduler().doEventLoop(&stopRTSP); // does not return


	//in_addr dstAddr = { 127, 0, 0, 1 };
	//Groupsock rtpGroupsock(*env, dstAddr, 1233, 255);
	//rtpGroupsock.addDestination(dstAddr, 1234, 0);
	//RTPSink * rtpSink = H264VideoRTPSink::createNew(*env, &rtpGroupsock, 96);

	//MFH264LiveSource *mfH264Source = MFH264LiveSource::createNew(*env, Pipeline);
	//rtpSink->startPlaying(*mfH264Source, NULL, NULL);

	///*while (!StopFlag)
	//{
	//	if (!Pipeline->videoEncBuffer->empty())
	//	{
	//		IMFSample *vidsamp = NULL;

	//		vidsamp = Pipeline->videoEncBuffer->pop();
	//		if (vidsamp != NULL)
	//		{
	//			WriteVideoSample(vidsamp, 0);
	//			videoFramesCount++;
	//		}
	//	}

	//	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	//}*/

	//// This function call does not return.
	//env->taskScheduler().doEventLoop();

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
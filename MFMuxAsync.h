#pragma once
#include <cstddef>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#include "RingBuffer.h"
#include "MFPipeline.h"
#include <thread>
#include "MFFilter.h"


class MFMuxAsync : public MFFilter
{
	LPWSTR lpstrFileName = nullptr;
	IMFSinkWriter* pSinkWriter = nullptr;

	MFTIME videoDuration = 0, audioDuration = 0;
	CRITICAL_SECTION cs;

	INT64 videoFramesCount = 0;
	INT64 audioFramesCount = 0;

	BOOL bUseHardwareEncoder = FALSE;
	
	VFVideoMediaType m_videoFormat;

	//UINT nVideoBitRate = 0;
	//UINT nVideoFrameSize = 0;

	/*
	Main thread code
	*/
	void ThreadProc();

	/*
	Audio and video stream codes
	*/
	DWORD video_stream;
	DWORD audio_stream;

	/*
	Check if there is an audio or video stream
	*/
	bool hasVideo;
	bool hasAudio;

	BOOL firstVideoSample;
	BOOL firstAudioSample;

	LONGLONG baseVideoTime;
	LONGLONG baseAudioTime;

	/*
	Thread reference
	*/
	std::thread* muxThread;

	IMFMediaType* videoInMT, *videoOutMT, *audioInMT, *audioOutMT;
	
	int Init();
	HRESULT StartWriteStreams();
public:
	MFMuxAsync(MFPipeline* pipeline, LPCWSTR lpszSaveFileName, VFVideoMediaType videoFormat, BOOL useHardwareEncoder);
	~MFMuxAsync();
	
	HRESULT WriteVideoSample(IMFSample* pSample, MFTIME duration);
	HRESULT WriteAudioSample(IMFSample* pSample, MFTIME duration);
	HRESULT ShutdownAndSaveFile() const;

	HRESULT AddVideoStream(IMFMediaType* mt); //H.264
	HRESULT AddAudioStream(IMFMediaType* mt); //AAC

	/*
	Start muxing and sending streams
	*/
	HRESULT Start();

	/*
	Wait for muxer thread to finish
	*/
	int Join() const;

	BOOL Finished;
};


#include "MFMuxAsync.h"
#include <Shlwapi.h>
#include <mfapi.h>
#include <Mferror.h>

#define SAFE_RELEASE(IUNK_PTR) if ((IUNK_PTR) != NULL) {(IUNK_PTR)->Release();(IUNK_PTR) = NULL;}

MFMuxAsync::MFMuxAsync(MFPipeline* pipeline, LPCWSTR lpszSaveFileName, VFVideoMediaType videoFormat, BOOL useHardwareEncoder)
	: MFFilter(pipeline),
	lpstrFileName(nullptr),
	pSinkWriter(nullptr),
	videoDuration(0),
	audioDuration(0),
	videoFramesCount(0),
	audioFramesCount(0),
	bUseHardwareEncoder(FALSE),
	video_stream(0),
	audio_stream(0),
	hasVideo(FALSE),
	hasAudio(FALSE),
	firstVideoSample(TRUE),
	firstAudioSample(TRUE),
	baseVideoTime(0),
	baseAudioTime(0),
	muxThread(nullptr),
	videoInMT(nullptr),
	videoOutMT(nullptr),
	audioInMT(nullptr),
	audioOutMT(nullptr),
	Finished(FALSE)
{
	lpstrFileName = StrDupW(lpszSaveFileName);
	InitializeCriticalSectionEx(&cs, 0, CRITICAL_SECTION_NO_DEBUG_INFO);
	_started = FALSE;

	m_videoFormat = videoFormat;
	bUseHardwareEncoder = useHardwareEncoder;

	this->Init();
}

HRESULT MFMuxAsync::AddVideoStream(IMFMediaType* mt)
{
	if (pSinkWriter == NULL || mt == NULL)
		return E_UNEXPECTED;

	MFCreateMediaType(&videoInMT);
	mt->CopyAllItems(videoInMT);

	MFCreateMediaType(&videoOutMT);
	mt->CopyAllItems(videoOutMT);

	//MFCreateMediaType(&pVideoOutType);
	//pVideoInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	//pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	//pVideoInType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	//pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	//pVideoInType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	//pVideoOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	//pVideoOutType->SetUINT32(MF_MT_AVG_BITRATE, dwBitRate);
	//MFSetAttributeSize(pVideoOutType, MF_MT_FRAME_SIZE, dwWidth, dwHeight);
	//MFSetAttributeRatio(pVideoOutType, MF_MT_FRAME_RATE, dwFps, 1);
	//MFSetAttributeRatio(pVideoOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	//MFSetAttributeSize(pVideoInType, MF_MT_FRAME_SIZE, dwWidth, dwHeight);
	//MFSetAttributeRatio(pVideoInType, MF_MT_FRAME_RATE, dwFps, 1);
	//MFSetAttributeRatio(pVideoInType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

	HRESULT hr = pSinkWriter->AddStream(videoOutMT, &video_stream);
	if (SUCCEEDED(hr))
		hr = pSinkWriter->SetInputMediaType(video_stream, videoInMT, NULL);

	/*dwImagePitch = dwWidth * 4;
	dwVideoWidth = dwWidth;
	dwVideoHeight = dwHeight;
	dwVideoFps = dwFps;
	nVideoBitRate = dwBitRate;
	nVideoFrameSize = dwImagePitch * dwHeight;*/

	hasVideo = true;

	return hr;
}

HRESULT MFMuxAsync::AddAudioStream(IMFMediaType* mt)
{
	if (pSinkWriter == NULL || mt == NULL)
		return E_UNEXPECTED;

	MFCreateMediaType(&audioInMT);
	mt->CopyAllItems(audioInMT);

	MFCreateMediaType(&audioOutMT);
	mt->CopyAllItems(audioOutMT);

	//MFCreateMediaType(&pAudioInType);
	//MFCreateMediaType(&pAudioOutType);
	//pAudioInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	//pAudioOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	//HRESULT hr = MFInitMediaTypeFromWaveFormatEx(pAudioInType, pwfx, sizeof WAVEFORMATEX);
	//if (FAILED(hr))
	//	return hr;
	//pAudioOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_MP3);
	//pAudioOutType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, nAvgBytesPerSec);
	//pAudioOutType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, pwfx->nSamplesPerSec);
	//pAudioOutType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, pwfx->nChannels >= 2 ? 2 : 1);
	//pAudioOutType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
	//pAudioOutType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
	//pAudioOutType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
	//pAudioOutType->SetUINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, TRUE);
	//pAudioOutType->SetUINT32(MF_MT_COMPRESSED, TRUE);

	HRESULT hr = pSinkWriter->AddStream(audioOutMT, &audio_stream);
	if (SUCCEEDED(hr))
		hr = pSinkWriter->SetInputMediaType(audio_stream, audioInMT, NULL);

	hasAudio = true;

	return hr;
}

MFMuxAsync::~MFMuxAsync()
{
	DeleteCriticalSection(&cs);
	if (lpstrFileName)
		CoTaskMemFree(lpstrFileName);

	SAFE_RELEASE(pSinkWriter);
}

int MFMuxAsync::Init()
{
	if (lpstrFileName == NULL)
		return E_POINTER;

	IMFAttributes* pAttr = NULL;
	MFCreateAttributes(&pAttr, 2);
	pAttr->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);

	if (bUseHardwareEncoder)
		pAttr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

	pAttr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);

	DeleteFileW(lpstrFileName);
	HRESULT hr = MFCreateSinkWriterFromURL(lpstrFileName, NULL, pAttr, &pSinkWriter);
	pAttr->Release();

	if (hr == S_OK)
	{
		Initiated = TRUE;
	}

	return hr;
}

HRESULT MFMuxAsync::Start()
{
	if (!Initiated)
	{
		Finished = TRUE;
		return E_FAIL;
	}

	_started = TRUE;

	muxThread = new std::thread(&MFMuxAsync::ThreadProc, this);
	return S_OK;
}

int MFMuxAsync::Join() const
{
	if (this->muxThread)
		this->muxThread->join();

	return S_OK;
}

HRESULT MFMuxAsync::StartWriteStreams()
{
	Finished = FALSE;
	return pSinkWriter->BeginWriting();
}

HRESULT MFMuxAsync::WriteVideoSample(IMFSample* pSample, MFTIME duration)
{
	HRESULT hr = S_OK;

	if (pSample == NULL)
		return E_POINTER;

	if (pSinkWriter == NULL)
		return S_OK;

	LONGLONG llTimeStamp = 0;
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
	LeaveCriticalSection(&cs);

	return hr;
}

HRESULT MFMuxAsync::WriteAudioSample(IMFSample* pSample, MFTIME duration)
{
	if (pSample == NULL)
		return E_POINTER;

	if (pSinkWriter == NULL)
		return S_OK;

	HRESULT hr = S_OK;

	LONGLONG llTimeStamp = 0;
	pSample->GetSampleTime(&llTimeStamp);

	if (firstAudioSample)
	{
		baseAudioTime = llTimeStamp;
		firstAudioSample = FALSE;
	}

	llTimeStamp -= baseAudioTime;

	hr = pSample->SetSampleTime(llTimeStamp);

	//MFTIME tempDuration = duration;
	//if (tempDuration == 0)
	//	MFFrameRateToAverageTimePerFrame(dwVideoFps, 1, (PUINT64)&tempDuration);

	EnterCriticalSection(&cs);
	if (SUCCEEDED(hr))
		hr = pSinkWriter->WriteSample(audio_stream, pSample);

	//if (SUCCEEDED(hr))
	//	videoDuration += tempDuration;
	LeaveCriticalSection(&cs);

	return hr;
}

void MFMuxAsync::ThreadProc()
{
	HRESULT hr = StartWriteStreams();
	if (!SUCCEEDED(hr))
	{
		TraceE(L"Unable to start muxer. Exiting.");
		return;
	}

	while (!(StopFlag && Pipeline->videoEncBuffer && Pipeline->videoEncBuffer->empty() && (!hasAudio || (Pipeline->audioEncBuffer && Pipeline->audioEncBuffer->empty()))))
	{
		IMFSample *vidsamp = NULL, *audsamp = NULL;

		if (hasVideo && !Pipeline->videoEncBuffer->empty())
		{
			vidsamp = Pipeline->videoEncBuffer->pop();
			if (vidsamp != NULL)
			{
				WriteVideoSample(vidsamp, 0);
				videoFramesCount++;
			}
		}

		if (hasAudio && Pipeline->audioEncBuffer && !Pipeline->audioEncBuffer->empty())
		{
			audsamp = Pipeline->audioEncBuffer->pop();
			if (audsamp != NULL)
			{
				WriteAudioSample(audsamp, 0);
				audioFramesCount++;
			}
		}

		if (Pipeline->videoEncBuffer->empty() && (hasAudio && Pipeline->audioEncBuffer && Pipeline->audioEncBuffer->empty()))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		SafeRelease(&vidsamp);
		SafeRelease(&audsamp);
	}

	TraceE(L"Muxer written %lld video frames and %lld audio frames\n", videoFramesCount, audioFramesCount);

	Finished = TRUE;
	//Sleep(1000);

	/*hr = ShutdownAndSaveFile();
	if (!SUCCEEDED(hr))
	{
		XTraceE(L"Unable to save file. Exiting.");
		return E_FAIL;
	}*/
}

HRESULT MFMuxAsync::ShutdownAndSaveFile() const
{
	HRESULT hr =  pSinkWriter->Finalize();
	if (!SUCCEEDED(hr))
	{
		TraceE(L"Unable to save file. Exiting.");
		return hr;
	}

	return hr;
}

#include "MFMSH264Encoder.h"

#include "MFUtils.h"

#include <codecapi.h>
#include <Shlwapi.h>

#include <debugapi.h>
#include <icrsint.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>

#include "VFDebug.h"
#include "MFCodecList.h"
#include "CodecAPIHelper.h"
#include <VersionHelpers.h>
#include <atlbase.h>

#pragma comment(lib, "wmcodecdspuuid.lib")
MFMSH264Encoder::MFMSH264Encoder(MFPipeline* pipeline, const VFVideoMediaType sourceMediaType, const VFMFVideoEncoderSettings settings) :
	MFVideoEncoder(pipeline, sourceMediaType, settings),
	CodecAPIHelper(nullptr),
	_encoder(nullptr),
	_inType(nullptr),
	_firstSample(TRUE),
	_baseTime(0),
	_outFramesCount(0),
	_inFramesCount(0),
	_encodeThread(nullptr)
{
	this->OutputMediaType = nullptr;

	CodecAPIHelper = new ::CodecAPIHelper(pipeline);

	if (sourceMediaType.Width > 1920 || sourceMediaType.Height > 1080)
	{
		TraceE(L"MS H264 encoder: unsupported resolution %dx%d. Use v10 or NVENC or QVS or AMD engines.", sourceMediaType.Width, sourceMediaType.Height);
		return;
	}

	this->Init();
}

MFMSH264Encoder::~MFMSH264Encoder()
{
	//encoderCb->Release();

	SafeRelease(&_encoder);
	SafeRelease(&_inType);
	SafeRelease(&OutputMediaType);

	delete CodecAPIHelper;
}

void MFMSH264Encoder::ProcessData()
{
	if (_encoder == nullptr)
	{
		TraceE(L"Unable to process data in video encoder.");
		return;
	}

	Finished = FALSE;
	StopFlag = FALSE;
	_firstSample = TRUE;
	_baseTime = 0;
	_outFramesCount = 0;
	_inFramesCount = 0;	

	//TraceD(L"Start Video Encoder\n");
	_encoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
	_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
	_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

	//get the size of the output buffer processed by the encoder.
	//There is only one output so the output stream id is 0.
	memset(&_outStreamInfo, 0, sizeof(MFT_OUTPUT_STREAM_INFO));
	ZeroMemory(&_outStreamInfo, sizeof(MFT_OUTPUT_STREAM_INFO));
	TESTHR(_encoder->GetOutputStreamInfo(0, &_outStreamInfo));

	ProcessInput();

	Finished = TRUE;
}

HRESULT MFMSH264Encoder::ProcessInput()
{
	while (!(StopFlag && Pipeline->videoCapBuffer->empty()))
	{
		IMFSample* pSample = nullptr;
		pSample = Pipeline->videoCapBuffer->pop();
		if (pSample == nullptr)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
			continue;
		}

		if (_inFramesCount % Settings.MaxKeyFrameSpacing == 0)
		{
			// ReSharper disable once CppExpressionWithoutSideEffects
			ForceKeyFrame();
		}

		//TraceD(L"Push video frame %d to encoder\n", _inFramesCount++);
		Encode(pSample);

		SafeReleaseSample(&pSample);

		if (StopFlag)
		{
			Sleep(1000);
			continue;
		}
	}

	return S_OK;
}

HRESULT MFMSH264Encoder::Encode(IMFSample* m_pSample)
{
	if (!m_pSample)
	{
		return E_INVALIDARG;
	}

	if (!_encoder)
	{
		return MF_E_NOT_INITIALIZED;
	}

	//Send input to the encoder.
	LONGLONG llTimeStamp = 0;
	HRESULT hr;

	TESTHR(hr = m_pSample->GetSampleTime(&llTimeStamp));
	if (_firstSample)
	{
		_baseTime = llTimeStamp;
		_firstSample = FALSE;
	}

	// rebase the time stamp
	llTimeStamp -= _baseTime;

	TESTHR(hr = m_pSample->SetSampleTime(llTimeStamp));
	TESTHR(hr = _encoder->ProcessInput(0, m_pSample, 0));

	ProcessOutput();

	return hr;
}

void MFMSH264Encoder::ProcessOutput()
{
	DWORD dwStatus = 0;

	IMFMediaBuffer* pBufferOut = nullptr;
	IMFSample* pSampleOut = nullptr;

	while (true)
	{
		//if (*STOP_FLAG && in_buffer->empty())
		//{
		//	return;
		//}

		DWORD flags = 0;
		HRESULT hr = _encoder->GetOutputStatus(&flags);
		if (flags != MFT_OUTPUT_STATUS_SAMPLE_READY && hr != E_NOTIMPL)
		{
			break;
		}

		//Generate the output sample
		MFT_OUTPUT_DATA_BUFFER mftOutputData;
		ZeroMemory(&mftOutputData, sizeof(mftOutputData));
		TESTHR(hr = MFCreateMemoryBuffer(_outStreamInfo.cbSize, &pBufferOut));
		TESTHR(hr = MFCreateSample(&pSampleOut));
		TESTHR(hr = pSampleOut->AddBuffer(pBufferOut));
		mftOutputData.pSample = pSampleOut;
		mftOutputData.dwStreamID = 0;
		const HRESULT hrRes = _encoder->ProcessOutput(0, 1, &mftOutputData, &dwStatus);

		//If more input is needed there was no output to process. Return and repeat
		if (hrRes == S_OK)
		{
			//TraceD(L"Get videoframe %d from encoder...Done.\n", _outFramesCount);
			//IMFMediaBuffer* pOutBuffer = NULL;
			//mftOutputData.pSample->GetBufferByIndex(0, &pOutBuffer);

			DWORD totalLength = 0;
			mftOutputData.pSample->GetTotalLength(&totalLength);
			//XTraceD(L"Output Processed: l: %d", totalLength);

			//mftOutputData.pSample->AddRef();
			Pipeline->videoEncBuffer->push(mftOutputData.pSample);

			//SafeRelease(&pOutBuffer);
			//SafeReleaseSample(&mftOutputData.pSample);
			//pSampleOut = nullptr;
			SafeRelease(&mftOutputData.pEvents);

			//TraceD(L"Get video frame %d from encoder...FREE.\n", _outFramesCount);

			_outFramesCount++;
		}
		else if (hrRes == MF_E_TRANSFORM_NEED_MORE_INPUT)
		{

		}
		else
		{
			TraceE(L"Video Encoder: Process output failed: %d", hrRes);
		}

		SafeRelease(&pBufferOut);

		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
		{
			//Do not log valid error
		}
	}
}

void MFMSH264Encoder::Join() const
{
	if (_encodeThread)
	{
		_encodeThread->join();
	}
}

HRESULT MFMSH264Encoder::Start()
{
	if (!Initiated)
	{
		return E_FAIL;
	}

	StopFlag = FALSE;
	_encodeThread = new std::thread(&MFMSH264Encoder::ProcessData, this);

	return S_OK;
}

HRESULT MFMSH264Encoder::Stop()
{
	StopFlag = TRUE;

	while (!Pipeline->videoCapBuffer->empty())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
		continue;
	}

	if (_encoder == nullptr)
	{
		Finished = TRUE;
		return S_OK;
	}

	_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
	_encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);
	_encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, NULL);
	
	ProcessOutput();

	Sleep(500);

	IMFShutdown* shutdown;
	if (_encoder && SUCCEEDED(_encoder->QueryInterface(&shutdown)))
	{
		shutdown->Shutdown();
	}

	return S_OK;
}

HRESULT MFMSH264Encoder::ForceKeyFrame() const
{
	if (codecAPI)
	{
		return CodecAPIHelper->SetVideoForceKeyFrame(codecAPI);
	}

	return E_FAIL;
}

bool MFMSH264Encoder::Init()
{
	HRESULT hr;

	// Create IMFTransform for h.264 encoder
	if (IsWindows8OrGreater())
	{
		CComPtr<IUnknown> spXferUnk;
		hr = CoCreateInstance(CLSID_CMSH264EncoderMFT, nullptr, CLSCTX_INPROC_SERVER, IID_IUnknown, (void**)&spXferUnk);
		
		if (SUCCEEDED(hr))
		{
			hr = spXferUnk->QueryInterface(IID_PPV_ARGS(&_encoder));
		}

		if (FAILED(hr))
		{
			_encoder = nullptr;
		}
	}
	else
	{
		UINT32 count = 0;
		IMFActivate ** activate = nullptr;
		MFT_REGISTER_TYPE_INFO info;

		info.guidMajorType = MFMediaType_Video;
		info.guidSubtype = MFVideoFormat_H264;
		const UINT32 flags = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_TRANSCODE_ONLY | MFT_ENUM_FLAG_SORTANDFILTER;

		TESTHR(hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
			flags,
			nullptr,
			&info,
			&activate,
			&count));

		if (count == 0) 
		{
			goto done;
		}

		TESTHR(hr = activate[count - 1]->ActivateObject(IID_PPV_ARGS(&_encoder)));

	done:
		for (UINT32 idx = 0; idx < count; idx++)
		{
			activate[idx]->Release();
		}

		CoTaskMemFree(activate);
	}

	//Enable async mode
	IMFAttributes *pAttributes = nullptr;
	TESTHR(hr = _encoder->GetAttributes(&pAttributes));
	TESTHR(hr = pAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE));

	SafeRelease(&pAttributes);

	//Get Stream info
	DWORD inMin = 0, inMax = 0, outMin = 0, outMax = 0;
	_encoder->GetStreamLimits(&inMin, &inMax, &outMin, &outMax);

	DWORD inStreamsCount = 0, outStreamsCount = 0;
	_encoder->GetStreamCount(&inStreamsCount, &outStreamsCount);

	const auto inStreams = new DWORD[inStreamsCount];
	const auto outStreams = new DWORD[outStreamsCount];

	hr = _encoder->GetStreamIDs(inStreamsCount, inStreams, outStreamsCount, outStreams);

	if (hr != S_OK)
	{
		if (hr == E_NOTIMPL)
		{
			inStreams[0] = 0;
			outStreams[0] = 0;
		}
		else
		{
			TraceE(L"Video Encoder: Unable to get MFT encoder stream IDs.\n");
			return hr;
		}
	}

	//Set types
	MFCreateMediaType(&OutputMediaType);

	OutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	OutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	MFSetAttributeSize(OutputMediaType, MF_MT_FRAME_SIZE, InputMediaTypeInfo.Width, InputMediaTypeInfo.Height);
	MFSetAttributeRatio(OutputMediaType, MF_MT_FRAME_RATE, InputMediaTypeInfo.FrameRateNum, InputMediaTypeInfo.FrameRateDen);

	if (Settings.CustomAspectRatioX > 0 && Settings.CustomAspectRatioY > 0)
	{
		MFSetAttributeRatio(OutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, Settings.CustomAspectRatioX, Settings.CustomAspectRatioY);
	}

	OutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, Settings.InterlaceMode);
	OutputMediaType->SetUINT32(MF_MT_MPEG2_PROFILE, Settings.H264Profile);
	OutputMediaType->SetUINT32(MF_MT_MPEG2_LEVEL, Settings.H264Level);

	//int bitrate = (int)(InputMediaTypeInfo.Width * InputMediaTypeInfo.Height * InputMediaTypeInfo.FrameRateNum * 1.0 * 0.07f);
	OutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, Settings.AvgBitrate * 1024);
	OutputMediaType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, Settings.MaxKeyFrameSpacing); //10

	//ApplySettings();

	hr = _encoder->SetOutputType(outStreams[0], OutputMediaType, 0);

	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Failed to set encoder output type.\n");
		return false;
	}

	GUID format;
	for (int i = 0; ; i++)
	{
		hr = _encoder->GetInputAvailableType(inStreams[0], i, &_inType);
		if (hr != S_OK) break;

		_inType->GetGUID(MF_MT_SUBTYPE, &format);

		if (format == MFVideoFormat_NV12) 
			break;

		SafeRelease(&_inType);
	}

	if (_inType == nullptr)
	{
		TraceE(L"Video Encoder: Failed to get input type.\n");
		return false;
	}

	hr = _encoder->SetInputType(inStreams[0], _inType, 0);
	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Failed to set input type.\n");
		return false;
	}

	if (hr == S_OK)
	{
		Initiated = TRUE;
	}	

	return SUCCEEDED(hr);
}

HRESULT MFMSH264Encoder::ApplySettings()
{
	const HRESULT hr = _encoder->QueryInterface(IID_PPV_ARGS(&codecAPI)); 
	if (hr == S_OK)
	{
		//TESTHR(CodecAPIHelper->SetAdaptiveMode(codecAPI, Settings.AdaptiveMode));

		TESTHR(CodecAPIHelper->SetCommonRateControlMode(codecAPI, Settings.RateControlMode));

		/*if (Settings.MaxBitrate > 0)
		{
			TESTHR(CodecAPIHelper->SetCommonMaxBitRate(codecAPI, Settings.MaxBitrate * 1024));
		}*/

		if (Settings.AvgBitrate > 0)
		{
			TESTHR(CodecAPIHelper->SetCommonMeanBitRate(codecAPI, Settings.AvgBitrate * 1024));
		}
		/*
		if (Settings.Quality > 0)
		{
			TESTHR(CodecAPIHelper->SetCommonQuality(codecAPI, Settings.Quality));
		}

		TESTHR(CodecAPIHelper->SetLowLatencyMode(codecAPI, Settings.LowLatencyMode));
		TESTHR(CodecAPIHelper->SetEncH264CABACEnable(codecAPI, Settings.CABAC));

		if (Settings.QualityVsSpeed > 0)
		{
			TESTHR(CodecAPIHelper->SetCommonQualityVsSpeed(codecAPI, Settings.QualityVsSpeed));
		}

		if (Settings.QPUsed)
		{
			TESTHR(CodecAPIHelper->SetVideoEncodeFrameTypeQP(codecAPI, Settings.FrameTypeQP));
			TESTHR(CodecAPIHelper->SetVideoEncodeQP(codecAPI, Settings.QP));
			TESTHR(CodecAPIHelper->SetVideoMinQP(codecAPI, Settings.MinQP));
			TESTHR(CodecAPIHelper->SetVideoMaxQP(codecAPI, Settings.MaxQP));
		}*/

		/*if (Settings.MPVGOPSize > 0)
		{
			TESTHR(CodecAPIHelper->SetEncMPVGOPSize(codecAPI, Settings.MPVGOPSize));
		}

		if (Settings.MaxNumRefFrame > 0)
		{
			TESTHR(CodecAPIHelper->SetVideoMaxNumRefFrame(codecAPI, Settings.MaxNumRefFrame));
		}

		if (Settings.DefaultBPictureCount > -1)
		{
			TESTHR(CodecAPIHelper->SetMPVDefaultBPictureCount(codecAPI, Settings.DefaultBPictureCount));
		}*/

		//TESTHR(CodecAPIHelper->SetNumWorkerThreads(codecAPI, -1));

		/*	CODECAPI_AVEncSliceControlMode : VT_UI4 0, default VT_UI4 2, minimal VT_UI4 2, maximal VT_UI4 2, step VT_UI4 0, modifiable
		CODECAPI_AVEncSliceControlSize : VT_UI4 0, minimal VT_UI4 0, maximal VT_UI4 8160, step VT_UI4 1, modifiable
		CODECAPI_AVEncVideoTemporalLayerCount : default VT_UI4 1, minimal VT_UI4 1, maximal VT_UI4 3, step VT_UI4 1, modifiable
		CODECAPI_AVEncH264SPSID
		CODECAPI_AVEncVideoContentType*/
	}

	return hr;
}
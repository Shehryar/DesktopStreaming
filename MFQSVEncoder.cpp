#include "MFQSVEncoder.h"
#include "MFUtils.h"

#include <codecapi.h>
#include <Shlwapi.h>
#include <icrsint.h>

#include "VFDebug.h"
#include "MFCodecList.h"
#include "CodecAPIHelper.h"

MFQSVEncoder::MFQSVEncoder(MFPipeline* pipeline, VFVideoMediaType videoInfo, VFMFVideoEncoderSettings settings) :
	MFVideoEncoder(pipeline, videoInfo, settings),
	CodecAPIHelper(nullptr),
	pEncoder(nullptr),
	pInType(nullptr),
	pEvGenerator(nullptr),
	encoderCb(nullptr)	
{
	CodecAPIHelper = new ::CodecAPIHelper(pipeline);

	this->pEncoder = nullptr;
	this->pInType = nullptr;
	this->OutputMediaType = nullptr;
	this->pEvGenerator = nullptr;

	this->Init();
}

MFQSVEncoder::~MFQSVEncoder()
{
	SafeRelease(&pEncoder);
	SafeRelease(&pInType);
	SafeRelease(&OutputMediaType);
	SafeRelease(&pEvGenerator);

	delete CodecAPIHelper;
}

HRESULT MFQSVEncoder::Start()
{
	if (!Initiated)
	{
		Finished = TRUE;
		return E_FAIL;
	}

	_started = TRUE;

	StopFlag = FALSE;
	Finished = FALSE;

	//TraceD(L"Start Video Encoder\n");
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

	return 0;
}

HRESULT MFQSVEncoder::Stop()
{
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
	pEncoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);

	StopFlag = TRUE;

	Sleep(500);

	IMFShutdown* shutdown;
	if (pEncoder && SUCCEEDED(pEncoder->QueryInterface(&shutdown)))
	{
		shutdown->Shutdown();
	}

	return S_OK;
}

HRESULT MFQSVEncoder::ForceKeyFrame() const
{
	if (codecAPI)
	{
		return CodecAPIHelper->SetVideoForceKeyFrame(codecAPI);
	}

	return E_FAIL;
}

HRESULT MFQSVEncoder::ApplySettings()
{
	//Settings.QualityVsSpeed = 75;
	Settings.AdaptiveMode = VFMFAdaptiveMode_FrameRate;

	const HRESULT hr = pEncoder->QueryInterface(IID_ICodecAPI, (void**)&codecAPI);
	if (hr == S_OK)
	{	
		//TESTHR(CodecAPIHelper->SetAdaptiveMode(codecAPI, Settings.AdaptiveMode));
		//TESTHR(CodecAPIHelper::SetAdaptiveMode(codecAPI, eAVEncAdaptiveMode_FrameRate));

		TESTHR(CodecAPIHelper->SetCommonRateControlMode(codecAPI, Settings.RateControlMode));

		if (Settings.MaxBitrate > 0 && 
			Settings.RateControlMode != VFMFCommonRateControlMode_Quality &&
			Settings.RateControlMode != VFMFCommonRateControlMode_CBR)
		{
			TESTHR(CodecAPIHelper->SetCommonMaxBitRate(codecAPI, Settings.MaxBitrate * 1024));
		}

		if (Settings.AvgBitrate > 0)
		{
			TESTHR(CodecAPIHelper->SetCommonMeanBitRate(codecAPI, Settings.AvgBitrate * 1024));
		}

		if (Settings.Quality > 0 && Settings.RateControlMode == VFMFCommonRateControlMode_Quality)
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
		}

		if (Settings.MPVGOPSize > 0)
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
		}

	/*		CODECAPI_AVEncSliceControlMode : VT_UI4 0, default VT_UI4 2, minimal VT_UI4 2, maximal VT_UI4 2, step VT_UI4 0, modifiable
		CODECAPI_AVEncSliceControlSize : VT_UI4 0, minimal VT_UI4 0, maximal VT_UI4 8160, step VT_UI4 1, modifiable
		CODECAPI_AVEncVideoTemporalLayerCount : default VT_UI4 1, minimal VT_UI4 1, maximal VT_UI4 3, step VT_UI4 1, modifiable*/
	}

	return hr;
}

bool MFQSVEncoder::Init()
{
	IMFActivate *codecActivate;

	MFCodecList codecs;

	codecs.Enumerate(MFMediaType_Video, MFVideoFormat_H264, TRUE);

	HRESULT hr = codecs.GetQSVH264Encoder(&codecActivate);

	if (FAILED(hr))
	{
		TraceE(L"Intel QSV H.264 encoder not found.");
		return false;
	}
	/*
	MFT_REGISTER_TYPE_INFO out_type = { 0 };
	out_type.guidMajorType = MFMediaType_Video;
	out_type.guidSubtype = MFVideoFormat_H264;

	IMFActivate **ppActivate = NULL;
	UINT32 count = 0;

	hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
	MFT_ENUM_FLAG_HARDWARE,
	NULL, &out_type,
	&ppActivate,
	&count
	);*/

	/*if (FAILED(hr) || count == 0)
	{
	XTrace("Video Encoder: Specified encoder not found!");
	return E_FAIL;1);
	}*/

	hr = codecActivate->ActivateObject(IID_PPV_ARGS(&pEncoder));

	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Unable to activate encoder");
		return false;
	}

	// used encoder CLSID
	//GUID clsid;
	//hr = ppActivate[0]->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &clsid);

	/*for (UINT32 i = 0; i < count; i++)
	{
	ppActivate[i]->Release();
	}

	CoTaskMemFree(ppActivate);*/

	codecs.Clear();

	//Enable async mode
	IMFAttributes *pAttributes = nullptr;
	pEncoder->GetAttributes(&pAttributes);
	pAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);

	SafeRelease(&pAttributes);

	//Get Stream info
	DWORD inMin = 0, inMax = 0, outMin = 0, outMax = 0;
	pEncoder->GetStreamLimits(&inMin, &inMax, &outMin, &outMax);

	DWORD inStreamsCount = 0, outStreamsCount = 0;
	pEncoder->GetStreamCount(&inStreamsCount, &outStreamsCount);

	const auto inStreams = new DWORD[inStreamsCount];
	const auto outStreams = new DWORD[outStreamsCount];

	hr = pEncoder->GetStreamIDs(inStreamsCount, inStreams, outStreamsCount, outStreams);

	if (hr != S_OK)
	{
		if (hr == E_NOTIMPL)
		{
			inStreams[0] = 0;
			outStreams[0] = 0;
		}
		else
		{
			TraceE(L"Video Encoder: Unable to get MFT encoder stream IDs");
			return false;
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
	OutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, Settings.AvgBitrate * 1024);
	OutputMediaType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, Settings.MaxKeyFrameSpacing); //10

	ApplySettings();

	hr = pEncoder->SetOutputType(outStreams[0], OutputMediaType, 0);

	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Failed to set encoder output type");
		return false;
	}	

	GUID format;
	for (int i = 0; ; i++)
	{
		hr = pEncoder->GetInputAvailableType(inStreams[0], i, &pInType);
		if (hr != S_OK) break;

		pInType->GetGUID(MF_MT_SUBTYPE, &format);

		/*if (IsEqualGUID(format, *sourceVideoFormat))
		{
		break;
		}*/

		if (format == MFVideoFormat_NV12) break;

		//if (format == MFVideoFormat_UYVY) break;
		//if (format == MFVideoFormat_IYUV) break;
		SafeRelease(&pInType);
	}

	if (pInType == nullptr)
	{
		TraceE(L"Video Encoder: Failed to get input type");
		return false;
	}

	UINT32 w, h, fps, den;
	MFGetAttributeSize(pInType, MF_MT_FRAME_SIZE, &w, &h);
	MFGetAttributeRatio(pInType, MF_MT_FRAME_RATE, &fps, &den);

	hr = pEncoder->SetInputType(inStreams[0], pInType, 0);
	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Failed to set input type");
		return false;
	}

	hr = pEncoder->QueryInterface(IID_PPV_ARGS(&pEvGenerator));
	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Failed to expose interface");
		return false;
	}

	encoderCb = new EncoderEventCallback(Pipeline, this);
	pEvGenerator->BeginGetEvent(encoderCb, nullptr);

	if (hr == S_OK)
	{
		Initiated = TRUE;
	}

	return SUCCEEDED(hr);
}

MFQSVEncoder::EncoderEventCallback::EncoderEventCallback(MFPipeline *pipeline, MFQSVEncoder *pEncoder) : ref_count(0)
{
	Pipeline = pipeline;
	pEncodeH264 = pEncoder;

	m_nInFramesCount = 0;
	m_nOutFramesCount = 0;

	_firstSample = TRUE;
	_baseTime = 0;
}

MFQSVEncoder::EncoderEventCallback::~EncoderEventCallback()
{

}

STDMETHODIMP MFQSVEncoder::EncoderEventCallback::QueryInterface(REFIID _riid, void** pp_v)
{
	static const QITAB _qit[] =
	{
		QITABENT(EncoderEventCallback, IMFAsyncCallback),{ 0 }
	};
	return QISearch(this, _qit, _riid, pp_v);
}

STDMETHODIMP_(ULONG) MFQSVEncoder::EncoderEventCallback::AddRef()
{
	return InterlockedIncrement(&ref_count);
}

STDMETHODIMP_(ULONG) MFQSVEncoder::EncoderEventCallback::Release()
{
	const long result = InterlockedDecrement(&ref_count);

	if (result == 0)
	{
		delete this;
	}

	return result;
}

STDMETHODIMP MFQSVEncoder::EncoderEventCallback::GetParameters(DWORD *p_flags, DWORD *p_queue)
{
	return E_NOTIMPL;
}

STDMETHODIMP MFQSVEncoder::EncoderEventCallback::Invoke(IMFAsyncResult *pAsyncResult)
{
	if (pEncodeH264->StopFlag && Pipeline->videoCapBuffer->empty())
	{
		pEncodeH264->Finished = TRUE;
		return S_OK;
	}

	if (pEncodeH264->StopFlag)
	{
		Sleep(500);
	}

	//TraceD(L"Video encoder callback\n");

	IMFMediaEvent *pMediaEvent = nullptr;
	MediaEventType evType = MEUnknown;
	HRESULT hr = S_OK;

	DWORD status;

	TESTHR(pEncodeH264->pEvGenerator->EndGetEvent(pAsyncResult, &pMediaEvent));

	if (pMediaEvent == nullptr)
	{
		pEncodeH264->Finished = TRUE;
		return S_OK;
	}

	TESTHR(pMediaEvent->GetType(&evType));
	TESTHR(pMediaEvent->GetStatus(&hr));

	TESTHR(hr);

	if (evType == METransformNeedInput)
	{
		if (Pipeline->videoCapBuffer->empty())
		{
			IMFSample *pSamplex = nullptr;
			hr = pEncodeH264->pEncoder->ProcessInput(0, pSamplex, 0); // put NULL, try again.
			if (pSamplex == nullptr) {
				// if NULL, sleep and do again later.
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}

			pEncodeH264->pEvGenerator->BeginGetEvent(this, nullptr);
			return S_OK;
		}

		//while (pEncodeH264->in_buffer->empty())
		//{
		//	if (*STOP_FLAG)
		//	{
		//		return S_OK;
		//	}

		//	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
		//	Sleep(5);
		//}

		//EnterCriticalSection(m_videoCapCrit);
		IMFSample *pSample = Pipeline->videoCapBuffer->pop();
		//LeaveCriticalSection(m_videoCapCrit);

		//while (pSample == NULL)
		//{			
		//	pEncodeH264->in_buffer->pop(pSample);
		//}

		if (pEncodeH264->StopFlag)
		{
			pEncodeH264->Finished = TRUE;
			return S_OK;
		}

		LONGLONG llTimeStamp = 0;
		hr = pSample->GetSampleTime(&llTimeStamp);
		if (_firstSample)
		{
			_baseTime = llTimeStamp;
			_firstSample = FALSE;
		}

		// rebase the time stamp
		llTimeStamp -= _baseTime;

		hr = pSample->SetSampleTime(llTimeStamp);

		DWORD size = 0;
		pSample->GetTotalLength(&size);

		if (m_nInFramesCount % pEncodeH264->Settings.MaxKeyFrameSpacing == 0)
		{
			pEncodeH264->ForceKeyFrame();
		}

		//pEncodeH264->TraceD(L"Push video frame %d to encoder\n", ++m_nInFramesCount);
		hr = pEncodeH264->pEncoder->ProcessInput(0, pSample, 0);
		if (FAILED(hr))
		{
			pEncodeH264->TraceE(L"Video Encoder: Process Input Failed");
		}

		if (pSample)
		{
			IMFMediaBuffer* buf;
			pSample->GetBufferByIndex(0, &buf);
			SafeRelease(&buf);
		}

		SafeRelease(&pSample);
	}
	else if (evType == METransformHaveOutput)
	{
		MFT_OUTPUT_DATA_BUFFER outDataBuffer;
		outDataBuffer.dwStatus = 0;
		outDataBuffer.dwStreamID = 0;
		outDataBuffer.pEvents = nullptr;
		outDataBuffer.pSample = nullptr;

		//pEncodeH264->TraceD(L"Get video frame %d from encoder\n", ++m_nOutFramesCount);
		hr = pEncodeH264->pEncoder->ProcessOutput(0, 1, &outDataBuffer, &status);
		if (SUCCEEDED(hr))
		{
			//pEncodeH264->TraceD(L"Get videoframe %d from encoder...Done.\n", m_nOutFramesCount);
			IMFMediaBuffer* pOutBuffer = nullptr;
			outDataBuffer.pSample->GetBufferByIndex(0, &pOutBuffer);

			DWORD totalLength = 0;
			outDataBuffer.pSample->GetTotalLength(&totalLength);
			//XTrace("Output Processed: l: " << totalLength);

			outDataBuffer.pSample->AddRef();
			Pipeline->videoEncBuffer->push(outDataBuffer.pSample);

			//m_Mux->WriteVideoSample(outDataBuffer.pSample, 0);


			SafeRelease(&pOutBuffer);
			SafeRelease(&outDataBuffer.pSample);
			SafeRelease(&outDataBuffer.pEvents);

			//pEncodeH264->TraceD(L"Get video frame %d from encoder...FREE.\n", m_nOutFramesCount);
		}

		if (FAILED(hr))
		{
			if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
			{
				pEncodeH264->TraceD(L"!!! Video Encoder: TRANSFORM STREAM CHANGE\n");

				if (outDataBuffer.dwStatus & MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE)
				{
					pEncodeH264->TraceD(L"!!! New MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE event\n");
					IMFMediaType* pType;
					TESTHR(hr = pEncodeH264->pEncoder->GetOutputAvailableType(0, 0, &pType));
					TESTHR(hr = pEncodeH264->pEncoder->SetOutputType(0, pType, 0));
				}

				/*hr = pEncodeH264->pEncoder->SetOutputType(0, pEncodeH264->pOutType, 0);
				if (FAILED(hr))
				{
				XTrace("Video Encoder: Failed to set output type");
				}*/
			}
			else
			{
				pEncodeH264->TraceE(L"Video Encoder: Process output failed");
			}
		}
	}
	else if (evType == METransformDrainComplete)
	{
		pEncodeH264->TraceD(L"New METransformDrainComplete event");
		TESTHR(hr = pEncodeH264->pEncoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0));
		//SetEvent(mEventDrainComplete);
	}
	else if (evType == MEError)
	{
		PROPVARIANT pValue;
		TESTHR(hr = pMediaEvent->GetValue(&pValue));
		pEncodeH264->TraceE(L"MEError, value: %u", pValue.vt);
		//error = true;
		//SetEvent(mEventNeedInput);
	}
	else
	{
		PROPVARIANT pValue;
		TESTHR(hr = pMediaEvent->GetValue(&pValue));
		pEncodeH264->TraceE(L"Unknown event type: %lu, Value: %u", evType, pValue.vt);
	}

	//pMediaEvent->Release();
	pEncodeH264->pEvGenerator->BeginGetEvent(this, nullptr);
	return S_OK;
}



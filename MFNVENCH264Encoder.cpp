#include "MFNVENCH264Encoder.h"

#include "MFUtils.h"

#include <atlbase.h>
#include <codecapi.h>
#include <Shlwapi.h>
//#include <boost/lockfree/policies.hpp>
//#include <boost/lockfree/queue.hpp>
#include <debugapi.h>
#include <icrsint.h>

#include "VFDebug.h"
#include "MFCodecList.h"
#include "CodecAPIHelper.h"

MFNVENCH264Encoder::MFNVENCH264Encoder(MFPipeline* pipeline, const VFVideoMediaType sourceMediaType, const VFMFVideoEncoderSettings settings) :
	MFVideoEncoder(pipeline, sourceMediaType, settings)
{
	CodecAPIHelper = new ::CodecAPIHelper(pipeline);

	this->pEncoder = NULL;
	this->pInType = NULL;
	this->OutputMediaType = NULL;
	this->pEvGenerator = NULL;

	this->Init();
}

MFNVENCH264Encoder::~MFNVENCH264Encoder()
{
	//encoderCb->Release();

	SafeRelease(&pEncoder);
	SafeRelease(&pInType);
	SafeRelease(&OutputMediaType);
	SafeRelease(&pEvGenerator);

	delete CodecAPIHelper;
}

HRESULT MFNVENCH264Encoder::Start()
{
	if (!Initiated)
	{
		return E_FAIL;
	}

	Finished = FALSE;
	StopFlag = FALSE;

	//TraceD(L"Start Video Encoder\n");
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

	return S_OK;
}

HRESULT MFNVENCH264Encoder::Stop()
{
	if (pEncoder == nullptr)
	{
		Finished = TRUE;
		return S_OK;
	}

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

HRESULT MFNVENCH264Encoder::ForceKeyFrame() const
{
	if (codecAPI)
	{
		return CodecAPIHelper->SetVideoForceKeyFrame(codecAPI);
	}

	return E_FAIL;
}

HRESULT MFNVENCH264Encoder::ApplySettings()
{
	HRESULT hr = pEncoder->QueryInterface(IID_ICodecAPI, (void**)&codecAPI);
	if (hr == S_OK)
	{
		TESTHR(CodecAPIHelper->SetCommonRateControlMode(codecAPI, Settings.RateControlMode));

		if (Settings.MaxBitrate > 0)
		{
			TESTHR(CodecAPIHelper->SetCommonMaxBitRate(codecAPI, Settings.MaxBitrate * 1024));
		}

		if (Settings.AvgBitrate > 0)
		{
			TESTHR(CodecAPIHelper->SetCommonMeanBitRate(codecAPI, Settings.AvgBitrate * 1024));
		}

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
		}

		if (Settings.MPVGOPSize > 0)
		{
			TESTHR(CodecAPIHelper->SetEncMPVGOPSize(codecAPI, Settings.MPVGOPSize));
		}

		if (Settings.MaxNumRefFrame > 0)
		{
			TESTHR(CodecAPIHelper->SetVideoMaxNumRefFrame(codecAPI, Settings.MaxNumRefFrame));
		}

		//CODECAPI_AVEncVideoLTRBufferControl : VT_UI4 0, values{ VT_I4 65537, VT_I4 65538 }
		//CODECAPI_AVEncVideoMarkLTRFrame:
		//CODECAPI_AVEncVideoUseLTRFrame:

		//CODECAPI_AVEncSliceControlMode : VT_UI4 2, minimal VT_UI4 0, maximal VT_UI4 2, step VT_UI4 1
		//CODECAPI_AVEncSliceControlSize : VT_UI4 0, minimal VT_UI4 0, maximal VT_UI4 3, step VT_UI4 1

		//CODECAPI_AVEncVideoMeanAbsoluteDifference : VT_UI4 0

		//CODECAPI_AVEncVideoROIEnabled : VT_UI4 0
		//CODECAPI_AVEncVideoTemporalLayerCount : minimal VT_UI4 1, maximal VT_UI4 3, step VT_UI4 1
	}

	return hr;
}

bool MFNVENCH264Encoder::Init()
{
	HRESULT hr;

	IMFActivate *codecActivate;
	MFCodecList codecs;

	GUID mediaFormat = MFVideoFormat_H264;

	codecs.Enumerate(MFMediaType_Video, MFVideoFormat_H264, TRUE);
	hr = codecs.GetNVENCH264Encoder(&codecActivate);

	if (FAILED(hr))
	{
		TraceE(L"NVENC H.264 encoder not found.\n");
		return false;
	}

	hr = codecActivate->ActivateObject(IID_PPV_ARGS(&pEncoder));

	if (FAILED(hr))
	{
		TraceE(L"NVENC H.264 encoder Unable to activate encoder.\n");
		return hr;
	}

	codecs.Clear();


	//Enable async mode
	IMFAttributes *pAttributes = NULL;
	hr = pEncoder->GetAttributes(&pAttributes);
	hr = pAttributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);

	SafeRelease(&pAttributes);

	//Get Stream info
	DWORD inMin = 0, inMax = 0, outMin = 0, outMax = 0;
	pEncoder->GetStreamLimits(&inMin, &inMax, &outMin, &outMax);

	DWORD inStreamsCount = 0, outStreamsCount = 0;
	pEncoder->GetStreamCount(&inStreamsCount, &outStreamsCount);

	DWORD *inStreams = new DWORD[inStreamsCount];
	DWORD *outStreams = new DWORD[outStreamsCount];

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
			TraceE(L"Video Encoder: Unable to get MFT encoder stream IDs.\n");
			return hr;
		}
	}

	//Set types
	MFCreateMediaType(&OutputMediaType);

	OutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	OutputMediaType->SetGUID(MF_MT_SUBTYPE, mediaFormat);
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
		TraceE(L"Video Encoder: Failed to set encoder output type.\n");
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

		if (format == MFVideoFormat_NV12)
			break;

		//if (format == MFVideoFormat_UYVY) break;
		//if (format == MFVideoFormat_IYUV) break;
		SafeRelease(&pInType);
	}

	if (pInType == NULL)
	{
		TraceE(L"Video Encoder: Failed to get input type.\n");
		return false;
	}

	hr = pEncoder->SetInputType(inStreams[0], pInType, 0);
	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Failed to set input type.\n");
		return false;
	}

	hr = pEncoder->QueryInterface(IID_PPV_ARGS(&pEvGenerator));
	if (FAILED(hr))
	{
		TraceE(L"Video Encoder: Failed to expose interface.\n");
		return false;
	}

	encoderCb = new EncoderEventCallback(Pipeline, this);
	pEvGenerator->BeginGetEvent(encoderCb, NULL);

	if (hr == S_OK)
	{
		Initiated = TRUE;
	}

	return SUCCEEDED(hr);
}

MFNVENCH264Encoder::EncoderEventCallback::EncoderEventCallback(MFPipeline *pipeline, MFNVENCH264Encoder *pEncoder)
{
	Pipeline = pipeline;
	pEncodeH264 = pEncoder;

	m_nInFramesCount = 0;
	m_nOutFramesCount = 0;

	_firstSample = TRUE;
	_baseTime = 0;
	_lastTimestamp = 0;
}

MFNVENCH264Encoder::EncoderEventCallback::~EncoderEventCallback()
{

}

STDMETHODIMP MFNVENCH264Encoder::EncoderEventCallback::QueryInterface(REFIID _riid, void** pp_v)
{
	static const QITAB _qit[] =
	{
		QITABENT(EncoderEventCallback, IMFAsyncCallback),{ 0 }
	};

	return QISearch(this, _qit, _riid, pp_v);
}

STDMETHODIMP_(ULONG) MFNVENCH264Encoder::EncoderEventCallback::AddRef()
{
	return InterlockedIncrement(&ref_count);
}

STDMETHODIMP_(ULONG) MFNVENCH264Encoder::EncoderEventCallback::Release()
{
	long result = InterlockedDecrement(&ref_count);

	if (result == 0)
	{
		delete this;
	}

	return result;
}

STDMETHODIMP MFNVENCH264Encoder::EncoderEventCallback::GetParameters(DWORD *p_flags, DWORD *p_queue)
{
	return E_NOTIMPL;
}

LONGLONG MFNVENCH264Encoder::CurrentPosition()
{
	return encoderCb->CurrentPosition();
}

LONGLONG MFNVENCH264Encoder::EncoderEventCallback::CurrentPosition()
{
	return _lastTimestamp;
}

STDMETHODIMP MFNVENCH264Encoder::EncoderEventCallback::Invoke(IMFAsyncResult *pAsyncResult)
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

	//pEncodeH264->TraceD(L"Video encoder callback\n");

	IMFMediaEvent *pMediaEvent = NULL;
	MediaEventType evType = MEUnknown;
	HRESULT hr = S_OK;

	DWORD status;

	TESTHR(pEncodeH264->pEvGenerator->EndGetEvent(pAsyncResult, &pMediaEvent));

	TESTHR(pMediaEvent->GetType(&evType));
	TESTHR(pMediaEvent->GetStatus(&hr));

	TESTHR(hr);

	if (evType == METransformNeedInput)
	{
		while (!pEncodeH264->StopFlag && Pipeline->videoCapBuffer->empty())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		IMFSample *pSample = Pipeline->videoCapBuffer->pop();

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

		_lastTimestamp = llTimeStamp;

		hr = pSample->SetSampleTime(llTimeStamp);

		//pEncodeH264->TraceD(L"Push video frame %d to encoder\n", ++m_nInFramesCount);

		if (m_nInFramesCount % pEncodeH264->Settings.MaxKeyFrameSpacing == 0)
		{
			pEncodeH264->ForceKeyFrame();
		}

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
		outDataBuffer.pEvents = 0;
		outDataBuffer.pSample = NULL;

		//pEncodeH264->TraceD(L"Get video frame %d from encoder\n", ++m_nOutFramesCount);
		hr = pEncodeH264->pEncoder->ProcessOutput(0, 1, &outDataBuffer, &status);
		if (SUCCEEDED(hr))
		{
			//TraceD(L"Get videoframe %d from encoder...Done.\n", m_nOutFramesCount);
			IMFMediaBuffer* pOutBuffer = NULL;
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

			//TraceD(L"Get video frame %d from encoder...FREE.\n", m_nOutFramesCount);
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
	pEncodeH264->pEvGenerator->BeginGetEvent(this, NULL);
	return S_OK;
}



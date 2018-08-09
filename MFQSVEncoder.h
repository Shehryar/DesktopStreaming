#pragma once

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#include <iostream>
#include <cstdio>
#include <thread>

#include "RingBuffer.h"
#include "MFPipeline.h"
#include "MFFilter.h"
#include "MFVideoEncoder.h"
#include "CodecAPIHelper.h"

class MFQSVEncoder : public MFVideoEncoder
{
public:
	/*
	Constructor
	in_buffer:     raw video input samples
	out_buffer:    encoded video output samples
	frameWidth:    video width
	frameHeight:   video height
	frameRate:     video frame rate
	frameAspect:   video frame aspect
	bitrate:       desired output bitrate
	*/
	MFQSVEncoder(MFPipeline* pipeline, VFVideoMediaType videoInfo, const VFMFVideoEncoderSettings settings);
	~MFQSVEncoder();

	/*
	Start encoding
	*/
	HRESULT Start() override;
	HRESULT Stop() override;
	HRESULT ForceKeyFrame() const;
private:
	CodecAPIHelper * CodecAPIHelper;
	HRESULT ApplySettings();

	/*
	Inner class for encoding media (Async MFT)
	*/
	class EncoderEventCallback : public IMFAsyncCallback {
	public:
		EncoderEventCallback(MFPipeline* pipeline, MFQSVEncoder *pEncoder);
		virtual ~EncoderEventCallback();

		STDMETHODIMP QueryInterface(REFIID _riid, void** pp_v);

		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		STDMETHODIMP GetParameters(DWORD* p_flags, DWORD* p_queue);
		STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);
	private:
		MFQSVEncoder *pEncodeH264;
		long ref_count;

		LONGLONG m_nInFramesCount, m_nOutFramesCount;
		MFPipeline* Pipeline;

		BOOL _firstSample;
		LONGLONG _baseTime;

		inline void TESTHR(HRESULT _hr) const
		{
			pEncodeH264->TESTHR(_hr);
		}
	};

	/*
	Finds and configures an MFT encoder
	*/
	bool Init();

	/*
	Media Foundation Environment and variables
	*/
	IMFTransform *pEncoder;
	IMFMediaType *pInType;
	IMFMediaEventGenerator *pEvGenerator;
	EncoderEventCallback *encoderCb;
	MFT_INPUT_STREAM_INFO inStreamInfo;
	MFT_OUTPUT_STREAM_INFO outStreamInfo;
};

#pragma once

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#include <Strmif.h>

#include <iostream>
#include <cstdio>
#include <thread>

#include "RingBuffer.h"
#include "MFPipeline.h"
#include "MFFilter.h"
#include "MFVideoEncoder.h"
//#include "MFColorSpaceConverter.h"
#include "CodecAPIHelper.h"


class MFNVENCH264Encoder : public MFVideoEncoder
{
public:
	MFNVENCH264Encoder(MFPipeline* pipeline, VFVideoMediaType sourceMediaType, VFMFVideoEncoderSettings settings);
	~MFNVENCH264Encoder();

	/*
	Start encoding
	*/
	HRESULT Start() override;
	HRESULT Stop() override;

	HRESULT ForceKeyFrame() const;

	LONGLONG CurrentPosition() override;
private:
	CodecAPIHelper * CodecAPIHelper;
	HRESULT ApplySettings();
	/*
	Inner class for encoding media (Async MFT)
	*/
	class EncoderEventCallback : public IMFAsyncCallback {
	public:
		EncoderEventCallback(MFPipeline* pipeline, MFNVENCH264Encoder *pEncoder);
		virtual ~EncoderEventCallback();

		STDMETHODIMP QueryInterface(REFIID _riid, void** pp_v);

		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		STDMETHODIMP GetParameters(DWORD* p_flags, DWORD* p_queue);
		STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult);

		LONGLONG CurrentPosition();
	private:
		MFNVENCH264Encoder *_encoderH264;
		long _refCount;

		LONGLONG _inFramesCount;
		LONGLONG _outFramesCount;
		MFPipeline* _pipeline;

		BOOL _firstSample;
		LONGLONG _baseTime;

		LONGLONG _lastTimestamp;

		inline void TESTHR(HRESULT _hr) const
		{
			_encoderH264->TESTHR(_hr);
		}
	};

	/*
	Finds and configures an MFT encoder
	*/
	bool Init();

	/*
	Media Foundation Environment and variables
	*/
	IMFTransform *_encoder;
	IMFMediaType *_inType;
	IMFMediaEventGenerator *_evGenerator;
	EncoderEventCallback *_encoderCb;
	MFT_INPUT_STREAM_INFO _inStreamInfo;
	MFT_OUTPUT_STREAM_INFO _outStreamInfo;
};

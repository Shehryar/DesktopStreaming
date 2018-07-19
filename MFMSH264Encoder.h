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


class MFMSH264Encoder : public MFVideoEncoder
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
	MFMSH264Encoder(MFPipeline* pipeline, VFVideoMediaType sourceMediaType, VFMFVideoEncoderSettings settings);
	~MFMSH264Encoder();

	/*
	Start encoding
	*/
	HRESULT Start() override;
	HRESULT Stop() override;

	HRESULT ForceKeyFrame() const;

	/*
	Wait for encoder thread to finish
	*/
	void Join() const;
private:
	/*
	Finds and configures an MFT encoder
	*/
	bool Init();
	void ProcessData();
	HRESULT Encode(IMFSample* m_pSample);
	void ProcessOutput();
	HRESULT ProcessInput();

	CodecAPIHelper *CodecAPIHelper;
	HRESULT ApplySettings();
	/*
	Media Foundation Environment and variables
	*/
	IMFTransform *pEncoder;
	IMFMediaType *pInType;

	MFT_INPUT_STREAM_INFO inStreamInfo;
	MFT_OUTPUT_STREAM_INFO _outStreamInfo;

	BOOL _firstSample;
	LONGLONG _baseTime;
	LONGLONG _outFramesCount;
	LONGLONG _inFramesCount;

	/*
	Thread reference
	*/
	std::thread *encodeThread;
};

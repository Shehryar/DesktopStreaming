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


class MFMSAACEncoder : public MFFilter 
{
public:
	/*
	Constructor:
	in_buffer:      input buffer (raw audio samples)
	out_buffer:     (out) output buffer (encoded audio samples)
	sps:            audio samples per second
	bitrate:        desired average bitrate
	*/
	MFMSAACEncoder(MFPipeline* pipeline, VFAudioMediaType audioFormat, int bitrate);
	~MFMSAACEncoder();

	/*
	Initialize Media Foundation environment and variables
	*/
	int Init();

	/*
	Start encoding
	*/
	HRESULT Start() override;

	/*
	Wait for encoder thread to finish
	*/
	void Join() const;

	VFAudioMediaType AudioFormat{};

	static IMFSample* PCMToMFSample(RAWAudioFrame* frame);
private:
	/*
	Audio encoder thread main code
	*/
	void ProcessData();

	/*
	Searches for and configures an audio encoding MFT
	*/
	bool FindEncoder();

	/*
	Media info
	*/
	int bitrate;

	/*
	Microsoft Media Foundation environment and variables
	*/
	IMFTransform *pEncoder;
	IMFMediaType *pInType;
	IMFMediaEventGenerator *pEvGenerator;
	MFT_INPUT_STREAM_INFO inStreamInfo{};
	MFT_OUTPUT_STREAM_INFO outStreamInfo{};

	/*
	Thread reference
	*/
	std::thread *encodeThread;

	BOOL _firstSample;
	LONGLONG _baseTime;
};



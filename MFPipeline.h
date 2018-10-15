#pragma once

#include "RingBuffer.h"

//#include "MFCaptureSource.h"
//#include "MFAudioEncoder.h"
//#include "MFH264Encoder.h"
//#include "FFMPEGMuxStreamer.h"
#include "MFUtils.h"
//#include "Intf.h"
//#include "MFFilter.h"
//#include "MFAudioRenderer.h"
//#include "MFVideoEncoder.h"
//#include "MFMuxAsync.h"
//#include "MFCaptureSourceAsync.h"
//#include "MFMux.h"

#define VFASYNC 1
#define VFAUDIO 1
#define VFENC 1
#define VFMUX 1

class MFCaptureSource;
class MFMSAACEncoder;
class MFCaptureSourceAsync;
class MFVideoEncoder;
class MFMuxAsync;
class MFAudioRenderer;
class MFRtpSink;
class MFFilter;

struct MFPipeline
{
	MFRingBuffer* videoCapBuffer;
	BOOL videoCapBufferEnabled;
	MFRingBuffer* audioCapBuffer;
	BOOL audioCapBufferEnabled;

	MFRingBuffer* videoEncBuffer;
	MFRingBuffer* audioEncBuffer;
	
	MFCaptureSource *audcap;
	MFFilter *audenc;

	MFCaptureSourceAsync *vidcap;
	MFVideoEncoder *videnc;
	
	MFMuxAsync *mux;

	MFAudioRenderer* audioRenderer;

	MFRtpSink *rtpsink;

	BOOL HAS_AUDIO;
	BOOL VIDEO_READY_FLAG;
	BOOL AUDIO_READY_FLAG;

	INT64 lastAudioTS;

	ERRORCALLBACK ErrorCB;
};

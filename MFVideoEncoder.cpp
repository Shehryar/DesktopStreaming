#include "MFVideoEncoder.h"
#include "MFUtils.h"

MFVideoEncoder::MFVideoEncoder(MFPipeline* pipeline, VFVideoMediaType videoInfo, VFMFVideoEncoderSettings settings)
	: MFFilter(pipeline), Finished(0)
{
	codecAPI = nullptr;
	Settings = settings;
	_started = FALSE;

	this->InputMediaTypeInfo = videoInfo;
	StopFlag = FALSE;
	Initiated = FALSE;
}


MFVideoEncoder::~MFVideoEncoder()
{
	SafeRelease(&codecAPI);
}

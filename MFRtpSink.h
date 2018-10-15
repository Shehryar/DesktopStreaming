#pragma once

#include "MFPipeline.h"
#include "MFFilter.h"


class MFRtpSink :public MFFilter
{
public:
	MFRtpSink(MFPipeline* pipeline);
	~MFRtpSink();
};


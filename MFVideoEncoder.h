#pragma once

#include <Strmif.h>

#include "Structs.h"
#include "MFFilter.h"

class MFVideoEncoder : public MFFilter
{
protected:
	ICodecAPI* codecAPI;	
	BOOL _started;
public:
	MFVideoEncoder(MFPipeline* pipeline, VFVideoMediaType videoInfo, VFMFVideoEncoderSettings settings);
	~MFVideoEncoder();

	VFMFVideoEncoderSettings Settings;
	VFVideoMediaType InputMediaTypeInfo;
		
	virtual HRESULT Start() 
	{ 
		return S_OK;
	}

	BOOL IsStarted() const
	{
		return _started;
	}

	virtual LONGLONG CurrentPosition()
	{
		return 0;
	}

	BOOL Finished;
};


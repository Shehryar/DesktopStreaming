#pragma once
#include "liveMedia.hh"
#include "MFPipeline.h"

class MFH264LiveSource :
	public FramedSource
{
private:
	MFPipeline* Pipeline;
	CRITICAL_SECTION cs;


	void MFH264LiveSource::TraceD(LPCTSTR lpszFormat, ...) const;
	HRESULT WriteVideoSample(IMFSample* pSample, MFTIME duration);
public:
	static MFH264LiveSource* createNew(UsageEnvironment& env, MFPipeline *pipeline)
	{
		return new MFH264LiveSource(env, pipeline);
	}

	bool isH264VideoStreamFramer() const;

	MFH264LiveSource(UsageEnvironment& env, MFPipeline *pipeline);
	~MFH264LiveSource();
	virtual void doGetNextFrame();	
};


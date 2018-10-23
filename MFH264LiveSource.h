#pragma once
#include "liveMedia.hh"

class MFH264LiveSource :
	public FramedSource
{
private:
	void MFH264LiveSource::TraceD(LPCTSTR lpszFormat, ...) const;
public:
	static MFH264LiveSource* createNew(UsageEnvironment& env)
	{
		return new MFH264LiveSource(env);
	}

	bool isH264VideoStreamFramer() const;

	MFH264LiveSource(UsageEnvironment& env); 
	~MFH264LiveSource();
	virtual void doGetNextFrame();	
};


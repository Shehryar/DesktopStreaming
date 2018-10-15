#pragma once

#include "MFPipeline.h"
//#include "../../MFMux/Intf.h"

class MFFilter
{
protected:
	MFPipeline* Pipeline;
	BOOL StopFlag;

	BOOL _started;
public:
	MFFilter(MFPipeline* pipeline);
	virtual ~MFFilter();

	virtual HRESULT Start();
	virtual HRESULT Stop();

	BOOL Finished;
	BOOL Initiated;

	IMFMediaType *OutputMediaType;

	void TraceE(LPCTSTR lpszFormat, ...) const;
	void TraceD(LPCTSTR lpszFormat, ...) const;

	inline void TESTHR(HRESULT _hr)
	{
		if FAILED(_hr)
			TraceE(L"TESTHR failed: %u\n", _hr);
	}

	BOOL IsStarted() const
	{
		return _started;
	}
};


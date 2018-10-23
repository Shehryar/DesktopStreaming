#include "MFH264LiveSource.h"


void MFH264LiveSource::TraceD(LPCTSTR lpszFormat, ...) const
{
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);

	::OutputDebugString(szBuffer);
	
	va_end(args);
}

bool MFH264LiveSource::isH264VideoStreamFramer() const {
	return true;
}

MFH264LiveSource::MFH264LiveSource(UsageEnvironment& env) :
	FramedSource(env)
{
}


MFH264LiveSource::~MFH264LiveSource()
{
}

void MFH264LiveSource::doGetNextFrame()
{
	TraceD(L"MFH264LiveSource::doGetNextFrame");
}
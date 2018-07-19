#include "MFFilter.h"

MFFilter::MFFilter(MFPipeline* pipeline) : Initiated(FALSE),
                                           OutputMediaType(nullptr)
{
	Pipeline = pipeline;
	StopFlag = FALSE;
	Finished = FALSE;
}


MFFilter::~MFFilter()
{
}

HRESULT MFFilter::Start()
{
	return E_FAIL;
}

HRESULT MFFilter::Stop()
{
	StopFlag = TRUE;

	return S_OK;
}

void MFFilter::TraceE(LPCTSTR lpszFormat, ...) const
{
#ifdef _DEBUG
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);

	if (Pipeline && Pipeline->ErrorCB)
	{
		Pipeline->ErrorCB(nullptr, 0, LL_ERROR, szBuffer);
	}
	else
	{
		::OutputDebugString(szBuffer);
	}

	va_end(args);
#else
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);

	if (Pipeline->ErrorCB)
	{
		Pipeline->ErrorCB(nullptr, 0, LL_ERROR, szBuffer);
	}
	else
	{
		wprintf(szBuffer);
	}

	va_end(args);
#endif
}

void MFFilter::TraceD(LPCTSTR lpszFormat, ...) const
{
#ifdef _DEBUG
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);

	if (Pipeline && Pipeline->ErrorCB)
	{
		Pipeline->ErrorCB(nullptr, 0, LL_DEBUG, szBuffer);
	}
	else
	{
		::OutputDebugString(szBuffer);
	}

	va_end(args);
#else
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);

	if (Pipeline->ErrorCB)
	{
		Pipeline->ErrorCB(nullptr, 0, LL_DEBUG, szBuffer);
	}
	else
	{
		wprintf(szBuffer);
	}

	va_end(args);
#endif
}

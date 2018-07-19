#include "VFDebug.h"


void XTrace0(LOG_LEVEL level, LPCTSTR lpszText)
{
#ifdef _DEBUG
	::OutputDebugString(lpszText);
#else
	wprintf(lpszText);
#endif
}

void XTrace(LOG_LEVEL level, LPCTSTR lpszFormat, ...)
{
	if (level < CURRENT_LOG_LEVEL)
		return;

#ifdef _DEBUG
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);
	::OutputDebugString(szBuffer);
	va_end(args);
#else
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);
	wprintf(szBuffer);
	va_end(args);	
#endif
}

void XTraceE(LPCTSTR lpszFormat, ...)
{
	if (LL_ERROR < CURRENT_LOG_LEVEL)
		return;

#ifdef _DEBUG
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);
	::OutputDebugString(szBuffer);
	va_end(args);
#else
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);
	wprintf(szBuffer);
	va_end(args);
#endif
}

void XTraceD(LPCTSTR lpszFormat, ...)
{
	if (LL_DEBUG < CURRENT_LOG_LEVEL)
		return;

#ifdef _DEBUG
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);
	::OutputDebugString(szBuffer);
	va_end(args);
#else
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);
	wprintf(szBuffer);
	va_end(args);
#endif
}
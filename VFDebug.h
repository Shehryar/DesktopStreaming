#pragma once

#include <stdio.h>
#include <windows.h>
#include "VFMFCaptureTypes.h"

#ifdef _DEBUG
#define XTRACE XTrace
#else
#define XTRACE
#endif

#define CURRENT_LOG_LEVEL LL_ERROR

void XTrace0(LOG_LEVEL level, LPCTSTR lpszText);
void XTrace(LOG_LEVEL level, LPCTSTR lpszFormat, ...);

void XTraceE(LPCTSTR lpszFormat, ...);
void XTraceD(LPCTSTR lpszFormat, ...);

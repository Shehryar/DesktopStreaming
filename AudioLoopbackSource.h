#pragma once

#include <Windows.h>
#include "MFPipeline.h"

// dangerous macros!
#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define MAX(a, b) ((a) > (b) ? (a) : (b))
extern bool bDiscontinuityDetected;
extern bool bVeryFirstPacket;

HRESULT LoopbackCaptureSetup(int* channels, int* bps, int* sampleRate, int* bufferSize, int* blockAlign);
HRESULT LoopbackCaptureStart(MFPipeline* pipeline);
HRESULT LoopbackCaptureTakeFromBuffer(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly, LONG* totalBytesWrote);
void LoopbackCaptureClear();
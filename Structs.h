#pragma once

#include <Windows.h>

struct VFVideoMediaType
{
	UINT32 Width;
	UINT32 Height;

	UINT32 FrameRateNum;
	UINT32 FrameRateDen;

	UINT32 FrameRateMinNum;
	UINT32 FrameRateMinDen;

	UINT32 FrameRateMaxNum;
	UINT32 FrameRateMaxDen;

	GUID SubType;
	WCHAR SubTypeString[10];
	WCHAR Name[50];
};

struct VFAudioMediaType
{
	UINT32 BPS;
	UINT32 Channels;
	UINT32 SampleRate;

	GUID SubType;
	WCHAR SubTypeString[10];
	WCHAR Name[50];
};

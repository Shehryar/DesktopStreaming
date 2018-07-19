#pragma once

#include <Windows.h>
#include <Strmif.h>
#include "VFMFCaptureTypes.h"

#include "MFPipeline.h"

class CodecAPIHelper
{
	MFPipeline * Pipeline;
	static HRESULT SetBOOL(ICodecAPI* api, GUID key, BOOL value);
	static HRESULT SetULONG(ICodecAPI* api, GUID key, ULONG value);
	static HRESULT SetQP(ICodecAPI* api, GUID key, H264QP &value, bool pack);
	static HRESULT SetULONGLONG(ICodecAPI* api, GUID key, ULONGLONG value);
	void TraceE(LPCTSTR lpszFormat, ...) const;
public:
	CodecAPIHelper(MFPipeline* pipeline);
	~CodecAPIHelper();

	HRESULT SetAdaptiveMode(ICodecAPI* api, ULONG value) const;
	HRESULT SetCommonRateControlMode(ICodecAPI* api, ULONG value) const;
	HRESULT SetCommonMaxBitRate(ICodecAPI* api, ULONG value) const;
	HRESULT SetCommonMeanBitRate(ICodecAPI* api, ULONG value) const;
	HRESULT SetCommonQuality(ICodecAPI* api, ULONG value) const;
	HRESULT SetLowLatencyMode(ICodecAPI* api, BOOL value) const;
	HRESULT SetEncH264CABACEnable(ICodecAPI* api, BOOL value) const;
	HRESULT SetCommonQualityVsSpeed(ICodecAPI* api, ULONG value) const;
	HRESULT SetVideoEncodeQP(ICodecAPI* api, H264QP value) const;
	HRESULT SetVideoMinQP(ICodecAPI* api, ULONG value) const;
	HRESULT SetVideoMaxQP(ICodecAPI* api, ULONG value) const;
	HRESULT SetEncMPVGOPSize(ICodecAPI* api, ULONG value) const;
	HRESULT SetVideoEncodeFrameTypeQP(ICodecAPI* api, H264QP value) const;
	HRESULT SetVideoForceKeyFrame(ICodecAPI* api) const;
	HRESULT SetVideoMaxNumRefFrame(ICodecAPI* api, ULONG value) const;
	HRESULT SetMPVDefaultBPictureCount(ICodecAPI* api, ULONG value) const;
	HRESULT SetNumWorkerThreads(ICodecAPI* api, LONG value) const;
};


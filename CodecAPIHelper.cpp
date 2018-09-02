#include "CodecAPIHelper.h"
#include <codecapi.h>
#include <corecrt_wstdio.h>
#include <xlocmon>


CodecAPIHelper::CodecAPIHelper(MFPipeline* pipeline)
{
	Pipeline = pipeline;
}


CodecAPIHelper::~CodecAPIHelper()
{
	Pipeline = nullptr;
}

HRESULT CodecAPIHelper::SetBOOL(ICodecAPI* api, GUID key, BOOL value)
{
	VARIANT v;
	v.vt = VT_BOOL;
	v.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
	
	HRESULT hr = api->SetValue(&key, &v);
	//VariantClear(&v);

	return hr;
}

HRESULT CodecAPIHelper::SetULONG(ICodecAPI* api, GUID key, const ULONG value)
{
	VARIANT var;
	VariantInit(&var);
	var.vt = VT_UI4;
	var.ulVal = value;

	HRESULT hr = api->SetValue(&key, &var);
	//VariantClear(&var); 

	return hr;
}

HRESULT CodecAPIHelper::SetQP(ICodecAPI* api, GUID key, H264QP &value, bool pack)
{
	VARIANT var;
	VariantInit(&var);
	var.vt = VT_UI8;
	var.ullVal = value.Pack(pack);

	HRESULT hr = api->SetValue(&key, &var);
	//VariantClear(&var);

	return hr;	
}

HRESULT CodecAPIHelper::SetULONGLONG(ICodecAPI* api, GUID key, ULONGLONG value)
{
	VARIANT var;
	VariantInit(&var);
	var.vt = VT_UI8;
	var.ullVal = value;

	HRESULT hr = api->SetValue(&key, &var);
	//VariantClear(&var);

	return hr;
}

HRESULT CodecAPIHelper::SetMPVDefaultBPictureCount(ICodecAPI* api, ULONG value) const
{
	const HRESULT hr = SetULONG(api, CODECAPI_AVEncMPVDefaultBPictureCount, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncMPVDefaultBPictureCount.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetAdaptiveMode(ICodecAPI* api, ULONG value) const
{
	const HRESULT hr = SetULONG(api, CODECAPI_AVEncAdaptiveMode, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncAdaptiveMode.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetCommonRateControlMode(ICodecAPI* api, ULONG value) const
{
	const HRESULT hr = SetULONG(api, CODECAPI_AVEncCommonRateControlMode, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncCommonRateControlMode.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetCommonMaxBitRate(ICodecAPI* api, ULONG value) const
{
	const HRESULT hr = SetULONG(api, CODECAPI_AVEncCommonMaxBitRate, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncCommonMaxBitRate.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetCommonMeanBitRate(ICodecAPI* api, ULONG value) const
{
	const HRESULT hr = SetULONG(api, CODECAPI_AVEncCommonMeanBitRate, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncCommonMeanBitRate.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetCommonQuality(ICodecAPI* api, ULONG value) const
{
	const HRESULT hr = SetULONG(api, CODECAPI_AVEncCommonQuality, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncCommonQuality.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetLowLatencyMode(ICodecAPI* api, BOOL value) const
{
	const HRESULT hr = SetBOOL(api, CODECAPI_AVLowLatencyMode, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVLowLatencyMode.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetEncH264CABACEnable(ICodecAPI* api, BOOL value) const
{
	const HRESULT hr = SetBOOL(api, CODECAPI_AVEncH264CABACEnable, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncH264CABACEnable.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetCommonQualityVsSpeed(ICodecAPI* api, ULONG value) const
{
	const HRESULT hr = SetULONG(api, CODECAPI_AVEncCommonQualityVsSpeed, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncCommonQualityVsSpeed.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetVideoEncodeQP(ICodecAPI* api, H264QP value) const
{
	HRESULT hr = SetQP(api, CODECAPI_AVEncVideoEncodeQP, value, true);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncVideoEncodeQP.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetVideoEncodeFrameTypeQP(ICodecAPI* api, H264QP value) const
{
	HRESULT hr = SetQP(api, CODECAPI_AVEncVideoEncodeFrameTypeQP, value, false);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncVideoEncodeFrameTypeQP.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetVideoMinQP(ICodecAPI* api, ULONG value) const
{
	HRESULT hr = SetULONG(api, CODECAPI_AVEncVideoMinQP, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncVideoMinQP.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetVideoMaxQP(ICodecAPI* api, ULONG value) const
{
	HRESULT hr = SetULONG(api, CODECAPI_AVEncVideoMaxQP, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncVideoMaxQP.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetEncMPVGOPSize(ICodecAPI* api, ULONG value) const
{
	HRESULT hr = SetULONG(api, CODECAPI_AVEncMPVGOPSize, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncMPVGOPSize.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetVideoForceKeyFrame(ICodecAPI* api) const
{
	HRESULT hr = SetULONG(api, CODECAPI_AVEncVideoForceKeyFrame, 1);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncVideoForceKeyFrame.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetVideoMaxNumRefFrame(ICodecAPI* api, ULONG value) const
{
	HRESULT hr = SetULONG(api, CODECAPI_AVEncVideoMaxNumRefFrame, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncVideoMaxNumRefFrame.\n");
	}

	return hr;
}

HRESULT CodecAPIHelper::SetNumWorkerThreads(ICodecAPI* api, LONG value) const
{
	if (value == -1)
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		value = sysinfo.dwNumberOfProcessors;
	}

	HRESULT hr = SetULONG(api, CODECAPI_AVEncNumWorkerThreads, value);
	if (hr != S_OK)
	{
		TraceE(L"Unable to set AVEncNumWorkerThreads.\n");
	}

	return hr;
}

void CodecAPIHelper::TraceE(LPCTSTR lpszFormat, ...) const
{
#ifdef _DEBUG
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	_vsnwprintf(szBuffer, 511, lpszFormat, args);

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
#pragma once

#include <windows.h>
#include <mfapi.h>

//#include <opencv2/core/core.hpp>
//#include <opencv2/core/types.hpp>
//#include <opencv2/imgproc/imgproc.hpp>

#include "MFUtils.h"
#include <cstdint>

//using namespace cv;

class MFColorConverter
{
	ERRORCALLBACK ErrorCB;

	uint8_t * source_planes[4];
	int source_stride[4];

	uint8_t* dest_planes[4];
	int dest_stride[4];

	struct SwsContext *img_convert_ctx;
	void TraceE(LPCTSTR lpszFormat, ...) const;

	inline void TESTHR(HRESULT _hr)
	{
		if FAILED(_hr)
			TraceE(L"TESTHR failed: %u\n", _hr);
	}
public:
	MFColorConverter(ERRORCALLBACK errorCB);
	~MFColorConverter();
	void BGR24ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest, BOOL flip);
	IMFSample* BGR24ToNV12S(RAWVideoFrame* frame, BOOL flip);

	void RGB24ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest);
	IMFSample* RGB24ToNV12S(RAWVideoFrame* frame);
	IMFSample* BGR24ToNV12SS(IMFSample* srcSample, int width, int height);

	void YUY2ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest);

	void NV12ToBGR24(BYTE* pSource, const int width, const int height, BYTE* pDest);
	HRESULT NV12ToBGR24S(IMFSample* pSource, const int width, const int height, BYTE* pDest);	

	void YUY2ToBGR24(BYTE* pSource, const int width, const int height, BYTE* pDest);
	HRESULT YUY2ToBGR24S(IMFSample* pSample, const int width, const int height, BYTE* pDest);
	IMFSample* YUY2ToNV12SS(IMFSample* srcSample, int width, int height);

	void BGR24ToBGR24Flip(BYTE* pSource, const int width, const int height, BYTE* pDest);
	HRESULT BGR24ToBGR24FlipS(IMFSample* pSample, const int width, const int height, BYTE* pDest);

	IMFSample* BGR32ToNV12S(RAWVideoFrame* frame, BOOL flip);
	void BGR32ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest, BOOL flip);
};


#include "MFColorConverter.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
}

int GetStrideRGB24(int width)
{
	const int stride = ((width * 3) - 1) / 4 * 4 + 4;
	return stride;
}

int GetStrideRGB32(int width)
{
	const int stride = ((width * 4) - 1) / 4 * 4 + 4;
	return stride;
}

int GetStrideNV12(int width)
{
	return width * 1.5;
}

MFColorConverter::MFColorConverter(ERRORCALLBACK errorCB) :
	img_convert_ctx(nullptr)
{
	memset(source_planes, 0, sizeof(uint8_t*) * 4);
	memset(source_stride, 0, sizeof(int) * 4);

	memset(dest_planes, 0, sizeof(uint8_t*) * 4);
	memset(dest_stride, 0, sizeof(int) * 4);

	ErrorCB = errorCB;
}

MFColorConverter::~MFColorConverter()
{
	av_freep(source_planes);
	av_freep(dest_planes);

	if (img_convert_ctx != nullptr)
	{
		sws_freeContext(img_convert_ctx);
	}
}

void MFColorConverter::TraceE(LPCTSTR lpszFormat, ...) const
{
#ifdef _DEBUG
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer
	auto nBuf = _vsnwprintf(szBuffer, 511, lpszFormat, args);

	if (ErrorCB)
	{
		ErrorCB(nullptr, 0, LL_ERROR, szBuffer);
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

	if (ErrorCB)
	{
		ErrorCB(nullptr, 0, LL_ERROR, szBuffer);
	}
	else
	{
		wprintf(szBuffer);
	}

	va_end(args);
#endif
}

HRESULT MFColorConverter::NV12ToBGR24S(IMFSample* pSample, const int width, const int height, BYTE* pDest)
{
	HRESULT hr;

	IMFMediaBuffer *pBuffer;
	DWORD dwBufferSize;
	BYTE* pBufferPtr;
	TESTHR(hr = pSample->GetBufferByIndex(0, &pBuffer));
	TESTHR(hr = pBuffer->GetMaxLength(&dwBufferSize));
	TESTHR(hr = pBuffer->Lock(&pBufferPtr, NULL, NULL));

	NV12ToBGR24(pBufferPtr, width, height, pDest);

	//Mat myuv(height + height / 2, width, CV_8UC1, pBufferPtr);

	//if(output == nullptr)
	//{
	//	output = new Mat(height, width, CV_8UC3, pDest);
	//}
	//else if (output->data != pDest)
	//{
	//	delete output;

	//	output = new Mat(height, width, CV_8UC3, pDest);
	//}

	//cvtColor(myuv, *output, CV_YUV2BGR_NV12);

	TESTHR(hr = pBuffer->Unlock());
	SafeRelease(&pBuffer);

	return hr;
}

HRESULT MFColorConverter::YUY2ToBGR24S(IMFSample* pSample, const int width, const int height, BYTE* pDest)
{
	HRESULT hr;

	IMFMediaBuffer *pBuffer;
	DWORD dwBufferSize;
	BYTE* pBufferPtr;
	TESTHR(hr = pSample->GetBufferByIndex(0, &pBuffer));
	TESTHR(hr = pBuffer->GetMaxLength(&dwBufferSize));
	TESTHR(hr = pBuffer->Lock(&pBufferPtr, NULL, NULL));

	YUY2ToBGR24(pBufferPtr, width, height, pDest);

	TESTHR(hr = pBuffer->Unlock());
	SafeRelease(&pBuffer);

	return hr;
}

void MFColorConverter::BGR24ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest, BOOL flip)
{
	if (source_planes[0] == nullptr)
	{
		if (av_image_alloc(source_planes, source_stride, width, height, AV_PIX_FMT_RGB24, 1) < 0)
		{
			return;
		}
	}

	memcpy(source_planes[0], pSource, width * height * 3);

	if (dest_planes[0] == nullptr)
	{
		if (av_image_alloc(dest_planes, dest_stride, width, height, AV_PIX_FMT_NV12, 1) < 0)
		{
			return;
		}
	}

	if (img_convert_ctx == nullptr)
	{
		img_convert_ctx = sws_getContext(
			width, height,
			AV_PIX_FMT_BGR24,
			width, height,
			AV_PIX_FMT_NV12,
			SWS_POINT, NULL, NULL, NULL);
	}

	if (flip)
	{
		if (source_stride[0] > 0)
		{
			source_stride[0] *= -1;
		}

		uint8_t * source_planes2[4] = {};
		source_planes2[0] = source_planes[0] + abs(source_stride[0]) * (height - 1);

		sws_scale(img_convert_ctx, (const uint8_t **)(source_planes2), source_stride, 0, height, dest_planes, dest_stride);
	}
	else
	{
		sws_scale(img_convert_ctx, (const uint8_t **)(source_planes), source_stride, 0, height, dest_planes, dest_stride);
	}

	memcpy(pDest, dest_planes[0], width * height * 1.5);
}

void MFColorConverter::BGR24ToBGR24Flip(BYTE* pSource, const int width, const int height, BYTE* pDest)
{
	if (source_planes[0] == nullptr)
	{
		if (av_image_alloc(source_planes, source_stride, width, height, AV_PIX_FMT_RGB24, 1) < 0)
		{
			return;
		}
	}

	memcpy(source_planes[0], pSource, width * height * 3);

	if (dest_planes[0] == nullptr)
	{
		if (av_image_alloc(dest_planes, dest_stride, width, height, AV_PIX_FMT_RGB24, 1) < 0)
		{
			return;
		}
	}

	if (img_convert_ctx == nullptr)
	{
		img_convert_ctx = sws_getContext(
			width, height,
			AV_PIX_FMT_RGB24,
			width, height,
			AV_PIX_FMT_RGB24,
			SWS_POINT, NULL, NULL, NULL);
	}

	if (source_stride[0] > 0)
	{
		source_stride[0] *= -1;
	}

	uint8_t * source_planes2[4] = {};
	source_planes2[0] = source_planes[0] + abs(source_stride[0]) * (height - 1);

	sws_scale(img_convert_ctx, (const uint8_t **)(source_planes2), source_stride, 0, height, dest_planes, dest_stride);


	memcpy(pDest, dest_planes[0], width * height * 3);
}

HRESULT MFColorConverter::BGR24ToBGR24FlipS(IMFSample* pSample, const int width, const int height, BYTE* pDest)
{
	HRESULT hr;

	IMFMediaBuffer *pBuffer;
	DWORD dwBufferSize;
	BYTE* pBufferPtr;
	TESTHR(hr = pSample->GetBufferByIndex(0, &pBuffer));
	TESTHR(hr = pBuffer->GetMaxLength(&dwBufferSize));
	TESTHR(hr = pBuffer->Lock(&pBufferPtr, NULL, NULL));

	BGR24ToBGR24Flip(pBufferPtr, width, height, pDest);

	TESTHR(hr = pBuffer->Unlock());
	SafeRelease(&pBuffer);

	return hr;
}


void MFColorConverter::RGB24ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest)
{
	if (source_planes[0] == nullptr)
	{
		if (av_image_alloc(source_planes, source_stride, width, height, AV_PIX_FMT_RGB24, 1) < 0)
		{
			return;
		}
	}

	memcpy(source_planes[0], pSource, width * height * 3);

	if (dest_planes[0] == nullptr)
	{
		if (av_image_alloc(dest_planes, dest_stride, width, height, AV_PIX_FMT_NV12, 1) < 0)
		{
			return;
		}
	}

	if (img_convert_ctx == nullptr)
	{
		img_convert_ctx = sws_getContext(
			width, height,
			AV_PIX_FMT_RGB24,
			width, height,
			AV_PIX_FMT_NV12,
			SWS_POINT, nullptr, nullptr, nullptr);
	}

	sws_scale(img_convert_ctx, (const uint8_t **)(source_planes), source_stride, 0, height, dest_planes, dest_stride);

	memcpy(pDest, dest_planes[0], width * height * 1.5);
}

IMFSample* MFColorConverter::BGR24ToNV12SS(IMFSample* srcSample, int width, int height)
{
	if (srcSample == nullptr)
		return nullptr;

	LONGLONG sampleTime = 0, sampleDuration = 0;
	HRESULT hr = srcSample->GetSampleTime(&sampleTime);
	hr = srcSample->GetSampleDuration(&sampleDuration);

	IMFMediaBuffer *srcBuffer = NULL;
	BYTE* srcBufferData = NULL;

	DWORD  cbMaxLength = 0;
	DWORD cbCurrentLength = 0;

	hr = srcSample->GetBufferByIndex(0, &srcBuffer);

	if (SUCCEEDED(hr))
		hr = srcBuffer->GetCurrentLength(&cbCurrentLength);
	hr = srcBuffer->GetMaxLength(&cbMaxLength);
	hr = srcBuffer->Lock(&srcBufferData, NULL, NULL);
	if (!SUCCEEDED(hr))
	{
		XTraceE(L"Unable to lock buffer to copy!\n");
		SafeRelease(&srcBuffer);
		return NULL;
	}

	IMFSample *newSample = NULL;
	IMFMediaBuffer *newBuffer = NULL;

	const DWORD cbBuffer = GetStrideRGB24(width) * height;

	BYTE *pData = NULL;

	// Create a new memory buffer.
	hr = MFCreateMemoryBuffer(cbBuffer, &newBuffer);

	// Lock the buffer and copy the video frame to the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->Lock(&pData, NULL, NULL);
	}

	BGR24ToNV12((BYTE*)srcBufferData, width, height, pData, true);

	hr = srcBuffer->Unlock();
	SafeRelease(&srcBuffer);

	if (newBuffer)
	{
		newBuffer->Unlock();
	}

	// Set the data length of the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->SetCurrentLength(cbBuffer);
	}

	// Create a media sample and add the buffer to the sample.
	if (SUCCEEDED(hr))
	{
		hr = MFCreateSample(&newSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->AddBuffer(newBuffer);
	}

	// Set the time stamp and the duration.
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleTime(sampleTime);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleDuration(sampleDuration);
	}

	SafeRelease(&newBuffer);

	return newSample;
}


IMFSample* MFColorConverter::YUY2ToNV12SS(IMFSample* srcSample, int width, int height)
{
	if (srcSample == nullptr)
		return nullptr;

	LONGLONG sampleTime = 0, sampleDuration = 0;
	HRESULT hr = srcSample->GetSampleTime(&sampleTime);
	hr = srcSample->GetSampleDuration(&sampleDuration);

	IMFMediaBuffer *srcBuffer = NULL;
	BYTE* srcBufferData = NULL;

	DWORD  cbMaxLength = 0;
	DWORD cbCurrentLength = 0;

	hr = srcSample->GetBufferByIndex(0, &srcBuffer);

	if (SUCCEEDED(hr))
		hr = srcBuffer->GetCurrentLength(&cbCurrentLength);
	hr = srcBuffer->GetMaxLength(&cbMaxLength);
	hr = srcBuffer->Lock(&srcBufferData, NULL, NULL);
	if (!SUCCEEDED(hr))
	{
		XTraceE(L"Unable to lock buffer to copy!\n");
		SafeRelease(&srcBuffer);
		return NULL;
	}

	IMFSample *newSample = NULL;
	IMFMediaBuffer *newBuffer = NULL;

	const DWORD cbBuffer = GetStrideRGB24(width) * height;

	BYTE *pData = NULL;

	// Create a new memory buffer.
	hr = MFCreateMemoryBuffer(cbBuffer, &newBuffer);

	// Lock the buffer and copy the video frame to the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->Lock(&pData, NULL, NULL);
	}

	YUY2ToNV12((BYTE*)srcBufferData, width, height, pData);

	hr = srcBuffer->Unlock();
	SafeRelease(&srcBuffer);

	if (newBuffer)
	{
		newBuffer->Unlock();
	}

	// Set the data length of the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->SetCurrentLength(cbBuffer);
	}

	// Create a media sample and add the buffer to the sample.
	if (SUCCEEDED(hr))
	{
		hr = MFCreateSample(&newSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->AddBuffer(newBuffer);
	}

	// Set the time stamp and the duration.
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleTime(sampleTime);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleDuration(sampleDuration);
	}

	SafeRelease(&newBuffer);

	return newSample;
}

void MFColorConverter::YUY2ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest)
{
	if (source_planes[0] == nullptr)
	{
		if (av_image_alloc(source_planes, source_stride, width, height, AV_PIX_FMT_YUYV422, 1) < 0)
		{
			return;
		}
	}

	memcpy(source_planes[0], pSource, width * height * 2);

	if (dest_planes[0] == nullptr)
	{
		if (av_image_alloc(dest_planes, dest_stride, width, height, AV_PIX_FMT_NV12, 1) < 0)
		{
			return;
		}
	}

	if (img_convert_ctx == nullptr)
	{
		img_convert_ctx = sws_getContext(
			width, height,
			AV_PIX_FMT_YUYV422,
			width, height,
			AV_PIX_FMT_NV12,
			SWS_POINT, nullptr, nullptr, nullptr);
	}

	sws_scale(img_convert_ctx, (const uint8_t **)(source_planes), source_stride, 0, height, dest_planes, dest_stride);

	memcpy(pDest, dest_planes[0], width * height * 1.5);
}

void MFColorConverter::NV12ToBGR24(BYTE* pSource, const int width, const int height, BYTE* pDest)
{
	if (source_planes[0] == nullptr)
	{
		if (av_image_alloc(source_planes, source_stride, width, height, AV_PIX_FMT_NV12 , 1) < 0)
		{
			return;
		}
	}

	memcpy(source_planes[0], pSource, width * height);
	memcpy(source_planes[1], pSource + width * height, width * height / 2);

	if (dest_planes[0] == nullptr)
	{
		if (av_image_alloc(dest_planes, dest_stride, width, height, AV_PIX_FMT_RGB24, 1) < 0)
		{
			return;
		}
	}

	if (img_convert_ctx == nullptr)
	{
		img_convert_ctx = sws_getContext(
			width, height,
			AV_PIX_FMT_NV12,
			width, height,
			AV_PIX_FMT_BGR24,
			SWS_POINT, NULL, NULL, NULL);
	}

	sws_scale(img_convert_ctx, (const uint8_t **)(source_planes), source_stride, 0, height, dest_planes, dest_stride);

	memcpy(pDest, dest_planes[0], width * height * 3);
}

void MFColorConverter::YUY2ToBGR24(BYTE* pSource, const int width, const int height, BYTE* pDest)
{
	if (source_planes[0] == nullptr)
	{
		if (av_image_alloc(source_planes, source_stride, width, height, AV_PIX_FMT_YUYV422, 1) < 0)
		{
			return;
		}
	}

	memcpy(source_planes[0], pSource, width * height * 2);
	//memcpy(source_planes[1], pSource + width * height, width * height / 2);

	if (dest_planes[0] == nullptr)
	{
		if (av_image_alloc(dest_planes, dest_stride, width, height, AV_PIX_FMT_RGB24, 1) < 0)
		{
			return;
		}
	}

	if (img_convert_ctx == nullptr)
	{
		img_convert_ctx = sws_getContext(
			width, 
			height,
			AV_PIX_FMT_YUYV422,
			width, 
			height,
			AV_PIX_FMT_BGR24,
			SWS_POINT,
			NULL,
			NULL,
			NULL);
	}

	sws_scale(img_convert_ctx, (const uint8_t **)(source_planes), source_stride, 0, height, dest_planes, dest_stride);

	memcpy(pDest, dest_planes[0], width * height * 3);
}

IMFSample* MFColorConverter::BGR24ToNV12S(RAWVideoFrame* frame, BOOL flip)
{
	if (frame == nullptr)
		return nullptr;

	IMFSample *newSample = NULL;
	IMFMediaBuffer *newBuffer = NULL;

	const DWORD cbBuffer = frame->Info.Width * 1.5 * frame->Info.Height;

	BYTE *pData = NULL;

	// Create a new memory buffer.
	HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &newBuffer);

	// Lock the buffer and copy the video frame to the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->Lock(&pData, NULL, NULL);
	}
	if (SUCCEEDED(hr))
	{
		BGR24ToNV12((BYTE*)frame->Buffer, frame->Info.Width, frame->Info.Height, pData, flip);
	}

	if (newBuffer)
	{
		newBuffer->Unlock();
	}

	// Set the data length of the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->SetCurrentLength(cbBuffer);
	}

	// Create a media sample and add the buffer to the sample.
	if (SUCCEEDED(hr))
	{
		hr = MFCreateSample(&newSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->AddBuffer(newBuffer);
	}

	// Set the time stamp and the duration.
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleTime(frame->Timestamp);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleDuration(frame->Duration);
	}

	SafeRelease(&newBuffer);

	return newSample;
}

void MFColorConverter::BGR32ToNV12(BYTE* pSource, const int width, const int height, BYTE* pDest, BOOL flip)
{
	if (source_planes[0] == nullptr)
	{
		if (av_image_alloc(source_planes, source_stride, width, height, AV_PIX_FMT_RGB32, 1) < 0)
		{
			return;
		}
	}

	memcpy(source_planes[0], pSource, width * height * 4);

	if (dest_planes[0] == nullptr)
	{
		if (av_image_alloc(dest_planes, dest_stride, width, height, AV_PIX_FMT_NV12, 1) < 0)
		{
			return;
		}
	}

	if (img_convert_ctx == nullptr)
	{
		img_convert_ctx = sws_getContext(
			width, height,
			AV_PIX_FMT_RGB32,
			width, height,
			AV_PIX_FMT_NV12,
			SWS_POINT, NULL, NULL, NULL);
	}

	if (flip)
	{
		if (source_stride[0] > 0)
		{
			source_stride[0] *= -1;
		}

		uint8_t * source_planes2[4] = {};
		source_planes2[0] = source_planes[0] + abs(source_stride[0]) * (height - 1);

		sws_scale(img_convert_ctx, (const uint8_t **)(source_planes2), source_stride, 0, height, dest_planes, dest_stride);
	}
	else
	{
		sws_scale(img_convert_ctx, (const uint8_t **)(source_planes), source_stride, 0, height, dest_planes, dest_stride);
	}

	memcpy(pDest, dest_planes[0], width * height * 1.5);
}

IMFSample* MFColorConverter::BGR32ToNV12S(RAWVideoFrame* frame, BOOL flip)
{
	if (frame == nullptr)
		return nullptr;

	IMFSample *newSample = NULL;
	IMFMediaBuffer *newBuffer = NULL;

	const DWORD cbBuffer = frame->Info.Width * 1.5 * frame->Info.Height;

	BYTE *pData = NULL;

	// Create a new memory buffer.
	HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &newBuffer);

	// Lock the buffer and copy the video frame to the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->Lock(&pData, NULL, NULL);
	}
	if (SUCCEEDED(hr))
	{
		BGR32ToNV12((BYTE*)frame->Buffer, frame->Info.Width, frame->Info.Height, pData, flip);
	}

	if (newBuffer)
	{
		newBuffer->Unlock();
	}

	// Set the data length of the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->SetCurrentLength(cbBuffer);
	}

	// Create a media sample and add the buffer to the sample.
	if (SUCCEEDED(hr))
	{
		hr = MFCreateSample(&newSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->AddBuffer(newBuffer);
	}

	// Set the time stamp and the duration.
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleTime(frame->Timestamp);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleDuration(frame->Duration);
	}

	SafeRelease(&newBuffer);

	return newSample;
}

IMFSample* MFColorConverter::RGB24ToNV12S(RAWVideoFrame* frame)
{
	if (frame == nullptr)
		return nullptr;

	IMFSample *newSample = NULL;
	IMFMediaBuffer *newBuffer = NULL;

	const DWORD cbBuffer = frame->Info.Width * 1.5 * frame->Info.Height;

	BYTE *pData = NULL;

	// Create a new memory buffer.
	HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &newBuffer);

	// Lock the buffer and copy the video frame to the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->Lock(&pData, NULL, NULL);
	}

	if (SUCCEEDED(hr))
	{
		RGB24ToNV12((BYTE*)frame->Buffer, frame->Info.Width, frame->Info.Height, pData);
	}

	if (newBuffer)
	{
		newBuffer->Unlock();
	}

	// Set the data length of the buffer.
	if (SUCCEEDED(hr))
	{
		hr = newBuffer->SetCurrentLength(cbBuffer);
	}

	// Create a media sample and add the buffer to the sample.
	if (SUCCEEDED(hr))
	{
		hr = MFCreateSample(&newSample);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->AddBuffer(newBuffer);
	}

	// Set the time stamp and the duration.
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleTime(frame->Timestamp);
	}
	if (SUCCEEDED(hr))
	{
		hr = newSample->SetSampleDuration(frame->Duration);
	}

	SafeRelease(&newBuffer);

	return newSample;
}
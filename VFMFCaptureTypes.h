#pragma once
#include <windows.h>
#include "VFMFCaptureEnums.h"

enum LOG_LEVEL
{
	LL_DEBUG = 0,

	LL_ERROR = 1,
};

//Callback Interface
typedef HRESULT(_stdcall *ERRORCALLBACK) (void* handle, DWORD handle_id, LOG_LEVEL level, LPCWSTR text);

struct VFMFBaseAudioInfo
{
	UINT32 BPS;
	UINT32 Channels;
	UINT32 SampleRate;
	VFMFAudioFormat Format;
};

struct VFMFBaseVideoInfo
{
	UINT32 Width;
	UINT32 Height;
	VFMFVideoColorSpace ColorSpace;
	UINT32 Stride;	
	UINT32 FrameRateNum;
	UINT32 FrameRateDen;
};

struct VFMFSourceSettings
{
	WCHAR VideoCaptureDevice[50];
	VFMFBaseVideoInfo VideoInfo;

	WCHAR AudioCaptureDevice[50];
	VFMFBaseAudioInfo AudioInfo;
	double AudioVolume;
};

struct H264QP 
{
	UINT16 DefaultQp;
	UINT16 I;
	UINT16 P;
	UINT16 B;

	UINT64 Pack(bool packDefault) {
		int shift = packDefault ? 0 : 16;
		UINT64 packedQp;
		if (packDefault)
			packedQp = DefaultQp;

		packedQp |= I << shift;
		shift += 16;
		packedQp |= P << shift;
		shift += 16;
		packedQp |= B << shift;

		return packedQp;
	}
};

struct VFMFVideoEncoderSettings
{
	VFMFVideoEncoder Encoder;

	// Specifies the average encoded bit rate, in bits per second. This property applies only to constant bit rate (CBR) and variable bit rate (VBR) encoding modes.
	INT32 AvgBitrate;

	// Specifies the maximum bit rate, in bits per second. This property applies only to constant bit rate (CBR) and variable bit rate (VBR) encoding modes.
	INT32 MaxBitrate;

	// Specifies the quality level for encoding. This property controls the quality level when the encoder is not using a constrained bit rate. 65, 0-100
	INT32 Quality;

	//Enables or disables CABAC (context-adaptive binary arithmetic coding) for H.264 entropy coding.
	BOOL CABAC;

	VFMFH264VProfile H264Profile;
	VFMFH264VLevel H264Level;

	// Specifies the rate control mode.
	VFMFCommonRateControlMode RateControlMode;
	VFMFVideoInterlaceMode InterlaceMode;
	INT32 CustomAspectRatioX;
	INT32 CustomAspectRatioY;
	VFMFAdaptiveMode AdaptiveMode;

	// Low-latency mode is useful for real-time communications or live capture, when latency should be minimized. However, low-latency mode might also reduce the decoding or encoding quality.
	// The encoder is expected to not add any sample delay due to frame reordering in encoding process, and one input sample shall produce one output sample.B slices / frames can be present 
	// as long as they do not introduce any frame re - ordering in the encoder.
	BOOL LowLatencyMode;

	INT32 MaxKeyFrameSpacing;

	// Specifies the tradeoff between encoding quality and speed. This property is valid in all rate control modes. 33, 0-100
	INT32 QualityVsSpeed;

	// Specifies the maximum number of pictures from one group-of-pictures (GOP) header to the next GOP header. 50
	INT32 MPVGOPSize;

	// TRUE is QP used
	BOOL QPUsed;

	// Specifies the quantization parameter (QP) for video encoding.
	H264QP QP;

	// Specifies the minimum quantization parameter(QP) for video encoding in variable-QP mode. 0, 0-51
	INT32 MinQP;

	// Specifies the maximum QP supported by the encoder. 51, 0-51.
	INT32 MaxQP;

	// Specifies the frame types (I, P, or B) that the quantization parameter (QP) is applied to.
	// Range for I, P, B frames is [0, 51].
	H264QP FrameTypeQP;

	// Specifies the maximum reference frames supported by the encoder. 2, [0, 16]
	INT32 MaxNumRefFrame;

	// Sets the maximum number of consecutive B frames in the output bitstream. Valid values are:
	// 0: Do not use B frames(default).
	// 1 : Use one B frame.
	// 2 : Use two B frames.
	INT32 DefaultBPictureCount;

	static void SetDefaults(VFMFVideoEncoderSettings* settings)
	{
		settings->AvgBitrate = 2000;
		settings->Encoder = VIDEO_ENCODER_MS_H264;
		settings->AdaptiveMode = VFMFAdaptiveMode_None;
		settings->CustomAspectRatioX = 0;
		settings->CustomAspectRatioY = 0;
		settings->H264Profile = VFMFH264VProfile_Main;
		settings->H264Level = VFMFH264VLevel4_2;
		settings->InterlaceMode = VFMFVideoInterlace_Progressive;
		settings->LowLatencyMode = FALSE;
		settings->MaxBitrate = 3000;
		settings->MaxKeyFrameSpacing = 10;
		settings->Quality = 75;
		settings->CABAC = TRUE;
		settings->QPUsed = FALSE;
	}
};

struct VFMFAudioEncoderSettings
{
	VFMFAudioEncoder Encoder;
	UINT32 Bitrate;
	double AmplifyLevel;

	static void SetDefaults(VFMFAudioEncoderSettings* settings)
	{
		settings->Bitrate = 128;
		settings->Encoder = AUDIO_ENCODER_MS_AAC;
		settings->AmplifyLevel = 1.0f;
	}
};

struct VFMFOutputSettings
{
	VFMFVideoEncoderSettings Video;
	VFMFAudioEncoderSettings Audio;
	WCHAR Filename[256];
};

struct RAWVideoFrame
{
	BYTE* Buffer;
	int BufferSize;
	VFMFBaseVideoInfo Info;
	LONGLONG Timestamp;
	LONGLONG Duration;
};

struct RAWAudioFrame
{
	BYTE* Buffer;
	int BufferSize;
	VFMFBaseAudioInfo Info;
	LONGLONG Timestamp;
	LONGLONG Duration;
};

struct EncodersAvailableInfo
{
	BOOL WIN_H264;
	BOOL QSV_H264;
	BOOL QSV_H265;
	BOOL NVENC_H264;
	BOOL NVENC_H265;
	BOOL WIN_AAC;
	BOOL AMD_H264;
	BOOL AMD_H265;
};

typedef int(__stdcall * VideoFrameCallback)(RAWVideoFrame* frame, LONGLONG timestamp);
typedef int(__stdcall * AudioFrameCallback)(RAWAudioFrame* frame, LONGLONG timestamp);

#pragma once

enum VFMFCommonRateControlMode 
{
	// Constant bit rate (CBR) encoding.
	VFMFCommonRateControlMode_CBR = 0,

	// Constrained variable bit rate (VBR) encoding.
	VFMFCommonRateControlMode_PeakConstrainedVBR = 1,

	// Unconstrained VBR encoding.
	VFMFCommonRateControlMode_UnconstrainedVBR = 2,

	// Quality-based VBR encoding. The encoder selects the bit rate to match a specified quality level. 
	VFMFCommonRateControlMode_Quality = 3,

	// Low delay VBR encoding.H.264 extension. Requires Windows 8 for CPU encoder.
	VFMFCommonRateControlMode_LowDelayVBR = 4,

	// Global VBR encoding.H.264 extension.	Requires Windows 8 for CPU encoder.
	VFMFCommonRateControlMode_GlobalVBR = 5,

	// Global low delay VBR encoding.H.264 extension. Requires Windows 8 for CPU encoder.
	VFMFCommonRateControlMode_GlobalLowDelayVBR = 6
};

enum VFMFAdaptiveMode
{
	VFMFAdaptiveMode_None = 0,
	VFMFAdaptiveMode_Resolution = 1,
	VFMFAdaptiveMode_FrameRate = 2
};

enum VFMFVideoInterlaceMode 
{
	VFMFVideoInterlace_Unknown = 0,
	VFMFVideoInterlace_Progressive = 2,
	VFMFVideoInterlace_FieldInterleavedUpperFirst = 3,
	VFMFVideoInterlace_FieldInterleavedLowerFirst = 4,
	VFMFVideoInterlace_FieldSingleUpper = 5,
	VFMFVideoInterlace_FieldSingleLower = 6,
	VFMFVideoInterlace_MixedInterlaceOrProgressive = 7
};

enum VFMFH264VProfile 
{
  VFMFH264VProfile_unknown                    = 0, 
  VFMFH264VProfile_Simple                     = 66, 
  VFMFH264VProfile_Base                       = 66, 
  VFMFH264VProfile_Main                       = 77, 
  VFMFH264VProfile_High                       = 100, 
  VFMFH264VProfile_422                        = 122, 
  VFMFH264VProfile_High10                     = 110, 
  VFMFH264VProfile_444                        = 144, 
  VFMFH264VProfile_Extended                   = 88, 
  VFMFH264VProfile_ScalableBase               = 83, 
  VFMFH264VProfile_ScalableHigh               = 86, 
  VFMFH264VProfile_MultiviewHigh              = 118, 
  VFMFH264VProfile_StereoHigh                 = 128, 
  VFMFH264VProfile_ConstrainedBase            = 256, 
  VFMFH264VProfile_UCConstrainedHigh          = 257, 
  VFMFH264VProfile_UCScalableConstrainedBase  = 258, 
  VFMFH264VProfile_UCScalableConstrainedHigh  = 259
};

enum VFMFMPEG2Profile
{
	VFMFMPEG2Profile_Simple = 1,
	VFMFMPEG2Profile_Main,
	VFMFMPEG2Profile_SNRScalable,
	VFMFMPEG2Profile_SpatiallyScalable,
	VFMFMPEG2Profile_High
};

enum VFMFH264VLevel 
{
	VFMFH264VLevel1 = 10,
	VFMFH264VLevel1_b = 11,
	VFMFH264VLevel1_1 = 11,
	VFMFH264VLevel1_2 = 12,
	VFMFH264VLevel1_3 = 13,
	VFMFH264VLevel2 = 20,
	VFMFH264VLevel2_1 = 21,
	VFMFH264VLevel2_2 = 22,
	VFMFH264VLevel3 = 30,
	VFMFH264VLevel3_1 = 31,
	VFMFH264VLevel3_2 = 32,
	VFMFH264VLevel4 = 40,
	VFMFH264VLevel4_1 = 41,
	VFMFH264VLevel4_2 = 42,
	VFMFH264VLevel5 = 50,
	VFMFH264VLevel5_1 = 51,
	VFMFH264VLevel5_2 = 51
};

enum VFMFMPEG2Level 
{
	VFMFMPEG2Level_Low = 1,
	VFMFMPEG2Level_Main,
	VFMFMPEG2Level_High1440,
	VFMFMPEG2Level_High
};

enum VFMFVideoEncoder
{
	VIDEO_ENCODER_MS_H264,

	VIDEO_ENCODER_QSV_H264,

	VIDEO_ENCODER_NVENC_H264,
	
	VIDEO_ENCODER_AMD_H264
};

enum VFMFAudioEncoder
{
	AUDIO_ENCODER_MS_AAC
};

enum VFMFVideoColorSpace
{
	ColorSpace_RGB = 0,
	ColorSpace_BGR = 1,
	ColorSpace_RGBA = 2,
	ColorSpace_NV12 = 3
};

enum VFMFAudioFormat
{
	AudioFormat_PCM8,
	AudioFormat_PCM16,
	AudioFormat_PCM24,
	AudioFormat_PCM32,
	AudioFormat_IEEE32
};
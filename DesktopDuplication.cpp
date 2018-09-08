// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//wprintf_s(L"Usage: %s inputaudio.mp3, %s output.wm*, %Encoding Type: CBR, VBR\n");

// ReSharper disable CppInconsistentNaming

#include <iostream> 
#include <mfapi.h>
#include <mfobjects.h>
#include <climits>
#include <Mferror.h>
#include <mftransform.h>
#include "OutputManager.h"
#include "DisplayManager.h"
#include "DuplicationManager.h"
#include "ThreadManager.h"
#include "wmcodecdsp.h" // Windows Media DSP interfaces
#include "Evr.h"
#include "MFMSH264Encoder.h"
#include "MFNVENCH264Encoder.h"
#include "MFColorConverter.h"
#include "MFMuxAsync.h"
#include "MFMSAACEncoder.h"

#include "AudioLoopbackSource.h"

#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>
#include "MFQSVEncoder.h"
#include "MFCodecList.h"
#include "DirectSoundSilenceOutput.h"

using namespace std::chrono;

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define AUDIO_ENCODER 1
#define FAKE_COORDINATES 0

//
// Globals
//
OUTPUTMANAGER OutMgr;
//IUnknown *spTransformUnk = NULL;
//IMFTransform *pTransform = NULL;
//IMFMediaType *pMFTInputMediaType = NULL, *pMFTOutputMediaType = NULL;
DWORD mftStatus = 0;
//IMFSample *videoSample = NULL;

//MFMSH264Encoder *videoEncoder;
MFPipeline _pipeline;
MFColorConverter _colorConv(nullptr);

DirectSoundSilenceOutput _silenceGenerator;

// Below are lists of errors expect from Dxgi API calls when a transition event like mode change, PnpStop, PnpStart
// desktop switch, TDR or session disconnect/reconnect. In all these cases we want the application to clean up the threads that process
// the desktop updates and attempt to recreate them.
// If we get an error that is not on the appropriate list then we exit the application

// These are the errors we expect from general Dxgi API due to a transition
HRESULT SystemTransitionsExpectedErrors[] = {
												DXGI_ERROR_DEVICE_REMOVED,
												DXGI_ERROR_ACCESS_LOST,
												static_cast<HRESULT>(WAIT_ABANDONED),
												S_OK                                    // Terminate list with zero valued HRESULT
};

// These are the errors we expect from IDXGIOutput1::DuplicateOutput due to a transition
HRESULT CreateDuplicationExpectedErrors[] = {
												DXGI_ERROR_DEVICE_REMOVED,
												static_cast<HRESULT>(E_ACCESSDENIED),
												DXGI_ERROR_UNSUPPORTED,
												DXGI_ERROR_SESSION_DISCONNECTED,
												S_OK                                    // Terminate list with zero valued HRESULT
};

// These are the errors we expect from IDXGIOutputDuplication methods due to a transition
HRESULT FrameInfoExpectedErrors[] = {
										DXGI_ERROR_DEVICE_REMOVED,
										DXGI_ERROR_ACCESS_LOST,
										S_OK                                    // Terminate list with zero valued HRESULT
};

// These are the errors we expect from IDXGIAdapter::EnumOutputs methods due to outputs becoming stale during a transition
HRESULT EnumOutputsExpectedErrors[] = {
										  DXGI_ERROR_NOT_FOUND,
										  S_OK                                    // Terminate list with zero valued HRESULT
};


//
// Forward Declarations
//
DWORD WINAPI DDProc(_In_ void* Param);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool ProcessCmdline(_Out_ INT* Output);
void ShowHelp();
void printLn(const char *outputStr);

//
// Class for progressive waits
//
typedef struct
{
	UINT    WaitTime;
	UINT    WaitCount;
}WAIT_BAND;

#define WAIT_BAND_COUNT 3
#define WAIT_BAND_STOP 0

class DYNAMIC_WAIT
{
public:
	DYNAMIC_WAIT();
	~DYNAMIC_WAIT();

	void Wait();

private:

	static const WAIT_BAND   m_WaitBands[WAIT_BAND_COUNT];

	// Period in seconds that a new wait call is considered part of the same wait sequence
	static const UINT       m_WaitSequenceTimeInSeconds = 2;

	UINT                    m_CurrentWaitBandIdx;
	UINT                    m_WaitCountInCurrentBand;
	LARGE_INTEGER           m_QPCFrequency;
	LARGE_INTEGER           m_LastWakeUpTime;
	BOOL                    m_QPCValid;
};
const WAIT_BAND DYNAMIC_WAIT::m_WaitBands[WAIT_BAND_COUNT] = {
																 {250, 20},
																 {2000, 60},
																 {5000, WAIT_BAND_STOP}   // Never move past this band
};

DYNAMIC_WAIT::DYNAMIC_WAIT() : m_CurrentWaitBandIdx(0), m_WaitCountInCurrentBand(0)
{
	m_QPCValid = QueryPerformanceFrequency(&m_QPCFrequency);
	m_LastWakeUpTime.QuadPart = 0L;
}
  
DYNAMIC_WAIT::~DYNAMIC_WAIT()
{
}

void DYNAMIC_WAIT::Wait()
{
	LARGE_INTEGER CurrentQPC = { 0 };

	// Is this wait being called with the period that we consider it to be part of the same wait sequence
	QueryPerformanceCounter(&CurrentQPC);
	if (m_QPCValid && (CurrentQPC.QuadPart <= (m_LastWakeUpTime.QuadPart + (m_QPCFrequency.QuadPart * m_WaitSequenceTimeInSeconds))))
	{
		// We are still in the same wait sequence, lets check if we should move to the next band
		if ((m_WaitBands[m_CurrentWaitBandIdx].WaitCount != WAIT_BAND_STOP) && (m_WaitCountInCurrentBand > m_WaitBands[m_CurrentWaitBandIdx].WaitCount))
		{
			m_CurrentWaitBandIdx++;
			m_WaitCountInCurrentBand = 0;
		}
	}
	else
	{
		// Either we could not get the current time or we are starting a new wait sequence
		m_WaitCountInCurrentBand = 0;
		m_CurrentWaitBandIdx = 0;
	}

	// Sleep for the required period of time
	Sleep(m_WaitBands[m_CurrentWaitBandIdx].WaitTime);

	// Record the time we woke up so we can detect wait sequences
	QueryPerformanceCounter(&m_LastWakeUpTime);
	m_WaitCountInCurrentBand++;
}


//
// Program entry point
//
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ INT nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MFUtils::Startup();

	const INT FRAME_RATE = 60;

	//HRESULT encoderResult = S_OK;
	INT SingleOutput;

	memset(&_pipeline, 0, sizeof(MFPipeline));

	// Synchronization
	const bool CmdResult = ProcessCmdline(&SingleOutput);
	if (!CmdResult)
	{
		ShowHelp();
		return 0;
	}

	// Event used by the threads to signal an unexpected error and we want to quit the app

	const HANDLE UnexpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!UnexpectedErrorEvent)
	{
		ProcessFailure(nullptr, L"UnexpectedErrorEvent creation failed", L"Error", E_UNEXPECTED);
		return 0;
	}

	// Event for when a thread encounters an expected error
	const HANDLE ExpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!ExpectedErrorEvent)
	{
		ProcessFailure(nullptr, L"ExpectedErrorEvent creation failed", L"Error", E_UNEXPECTED);
		return 0;
	}

	// Event to tell spawned threads to quit
	const HANDLE TerminateThreadsEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!TerminateThreadsEvent)
	{
		ProcessFailure(nullptr, L"TerminateThreadsEvent creation failed", L"Error", E_UNEXPECTED);
		return 0;
	}

	// Load simple cursor
	HCURSOR Cursor = nullptr;
	Cursor = LoadCursor(nullptr, IDC_ARROW);
	if (!Cursor)
	{
		ProcessFailure(nullptr, L"Cursor load failed", L"Error", E_UNEXPECTED);
		return 0;
	}

	// Register class
	WNDCLASSEXW Wc;
	Wc.cbSize = sizeof(WNDCLASSEXW);
	Wc.style = CS_HREDRAW | CS_VREDRAW;
	Wc.lpfnWndProc = WndProc;
	Wc.cbClsExtra = 0;
	Wc.cbWndExtra = 0;
	Wc.hInstance = hInstance;
	Wc.hIcon = nullptr;
	Wc.hCursor = Cursor;
	Wc.hbrBackground = nullptr;
	Wc.lpszMenuName = nullptr;
	Wc.lpszClassName = L"ddasample";
	Wc.hIconSm = nullptr;
	if (!RegisterClassExW(&Wc))
	{
		ProcessFailure(nullptr, L"Window class registration failed", L"Error", E_UNEXPECTED);
		return 0;
	}

	// Create window
	RECT WindowRect = { 0, 0, 800, 600 };
	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);
	const HWND window_handle = CreateWindowW(L"ddasample", L"DXGI desktop duplication sample",
		WS_OVERLAPPEDWINDOW,
		0, 0,
		WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top,
		nullptr, nullptr, hInstance, nullptr);
	if (!window_handle)
	{
		ProcessFailure(nullptr, L"Window creation failed", L"Error", E_FAIL);
		return 0;
	}

	DestroyCursor(Cursor);

	ShowWindow(window_handle, nCmdShow);
	UpdateWindow(window_handle);

	printLn("THIS IS A TEST");
	THREADMANAGER ThreadMgr;
	RECT DeskBounds;
	UINT OutputCount;

	// Message loop (attempts to update screen when no other messages to process)
	MSG msg = { 0 };
	bool FirstTime = true;
	bool Occluded = true;
	DYNAMIC_WAIT DynamicWait;

	while (WM_QUIT != msg.message)
	{
		DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == OCCLUSION_STATUS_MSG)
			{
				// Present may not be occluded now so try ag  ain
				Occluded = false;
			}
			else
			{
				// Process window messages
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else if (WaitForSingleObjectEx(UnexpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0)
		{
			// Unexpected error occurred so exit the application
			break;
		}
		else if (FirstTime || WaitForSingleObjectEx(ExpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0)
		{
			if (!FirstTime)
			{
				// Terminate other threads
				SetEvent(TerminateThreadsEvent);
				ThreadMgr.WaitForThreadTermination();
				ResetEvent(TerminateThreadsEvent);
				ResetEvent(ExpectedErrorEvent);

				// Clean up
				ThreadMgr.Clean();
				OutMgr.CleanRefs();

				// As we have encountered an error due to a system transition we wait before trying again, using this dynamic wait
				// the wait periods will get progressively long to avoid wasting too much system resource if this state lasts a long time
				DynamicWait.Wait();
			}
			else
			{
				// First time through the loop  so nothing to clean up
				FirstTime = false;
			}

			// Re-initialize
			Ret = OutMgr.InitOutput(window_handle, SingleOutput, &OutputCount, &DeskBounds);
			if (Ret == DUPL_RETURN_SUCCESS)
			{
				if (_pipeline.videnc == nullptr)
				{
<<<<<<< HEAD
=======
					//VFMFVideoEncoder videoEncoder = VIDEO_ENCODER_NVENC_H264;
					VFMFVideoEncoder videoEncoder = VIDEO_ENCODER_QSV_H264;

>>>>>>> 0c2a790c7023041a20768f135a642e0be49ceb81
					//buffers
					_pipeline.videoCapBuffer = new MFRingBuffer(25);
					_pipeline.videoEncBuffer = new MFRingBuffer(25);

					VFVideoMediaType mt{}; 

					if (FAKE_COORDINATES)
					{
						mt.Width = 1920; 
						mt.Height = 1080;
					}
					else
					{
						mt.Width = DeskBounds.right - DeskBounds.left;
						mt.Height = DeskBounds.bottom - DeskBounds.top;
					}

					mt.FrameRateNum = FRAME_RATE;
					mt.FrameRateDen = 1;
					mt.SubType = MFVideoFormat_NV12;

					VFMFVideoEncoderSettings settings{};
					settings.H264Profile = VFMFH264VProfile_Main;
					settings.H264Level = VFMFH264VLevel4_2;
					settings.AvgBitrate = 2000;
					settings.Encoder = VIDEO_ENCODER_MS_H264;
					settings.MaxKeyFrameSpacing = 10;
					settings.InterlaceMode = VFMFVideoInterlace_Progressive;
					settings.MaxBitrate = 3000;
					settings.Quality = 75;
					settings.CABAC = false;
					settings.AdaptiveMode = VFMFAdaptiveMode_None;
					settings.RateControlMode = VFMFCommonRateControlMode_CBR;

					// enumerating codecs, use GPU encoder if available, H264 MS CPU if not available
					MFCodecList _videoCodecs;
					_videoCodecs.Enumerate(MFMediaType_Video, MFVideoFormat_H264, TRUE);
				
					if (_videoCodecs.IsNVENCH264EncoderAvailable())
					{
						settings.Encoder = VIDEO_ENCODER_NVENC_H264;
					}
					else if (_videoCodecs.IsQSVH264EncoderAvailable())
					{
						settings.Encoder = VIDEO_ENCODER_QSV_H264;
					}
					else if (_videoCodecs.IsNVENCH264EncoderAvailable())
					{
						settings.Encoder = VIDEO_ENCODER_NVENC_H264;
					}
					else if (_videoCodecs.IsAMDH264EncoderAvailable())
					{
						settings.Encoder = VIDEO_ENCODER_AMD_H264;
					}

					BOOL hwEncoder = FALSE;											

					switch (settings.Encoder)
					{
					case VIDEO_ENCODER_MS_H264:
						_pipeline.videnc = new MFMSH264Encoder(&_pipeline, mt, settings);
						break;
					case VIDEO_ENCODER_QSV_H264:
						hwEncoder = TRUE;
						_pipeline.videnc = new MFQSVEncoder(&_pipeline, mt, settings);
						break;
					case VIDEO_ENCODER_NVENC_H264:
						hwEncoder = TRUE;
						_pipeline.videnc = new MFNVENCH264Encoder(&_pipeline, mt, settings);
						break;
					//case VIDEO_ENCODER_AMD_H264:
					//	hwEncoder = TRUE;
					//	_pipeline.videnc = new MFAMDH264Encoder(&_pipeline, mt, settings);
					//	break;
					default:;
					}

					if (_pipeline.videnc == nullptr)
					{
						DisplayMsg(L"Failed to set video encoder", L"Error", S_OK);
						return 1;
					}

					if (!_pipeline.videnc->Initiated)
					{
						DisplayMsg(L"Failed to initiate video encoder", L"Error", S_OK);
						return 1;
					}

					printLn("Output file set to c:\\vf\\output.mp4");
					_pipeline.mux = new MFMuxAsync(&_pipeline, L"c:\\vf\\output.mp4", mt, hwEncoder);
					if (_pipeline.mux == nullptr)
					{
						DisplayMsg(L"Failed to set muxer", L"Error", S_OK);
						return 1;
					}

					_pipeline.mux->AddVideoStream(_pipeline.videnc->OutputMediaType);

					if (!_pipeline.mux->Initiated)
					{
						DisplayMsg(L"Failed to initiate muxer", L"Error", S_OK);
						return 1;
					}
				}

				const HANDLE shared_handle = OutMgr.GetSharedHandle();
				if (shared_handle)
				{
					Ret = ThreadMgr.Initialize(SingleOutput, OutputCount, UnexpectedErrorEvent, ExpectedErrorEvent, TerminateThreadsEvent, shared_handle, &DeskBounds);
				}
				else
				{
					DisplayMsg(L"Failed to get handle of shared surface", L"Error", S_OK);
					Ret = DUPL_RETURN_ERROR_UNEXPECTED;
				}
			}

			if (AUDIO_ENCODER)
			{
				int channels = 0;
				int sampleRate = 0;
				const int bitrate = 128;
				int bufferSize = 0;
				int blockAlign = 0;
				int bps = 0;						

				// configure audio source (speakers)
				LoopbackCaptureSetup(&channels, &bps, &sampleRate, &bufferSize, &blockAlign);

				// configure encoder
				if (_pipeline.audenc == nullptr)
				{
					//buffers
					_pipeline.audioCapBuffer = new MFRingBuffer(200);
					_pipeline.audioEncBuffer = new MFRingBuffer(200);

					VFAudioMediaType mt{};
					
					mt.BPS = bps;
					mt.Channels = channels;
					mt.SampleRate = sampleRate;
					
					_pipeline.HAS_AUDIO = TRUE;

					_pipeline.audenc = new MFMSAACEncoder(&_pipeline, mt, bitrate);
					
					if (_pipeline.audenc == nullptr)
					{
						DisplayMsg(L"Failed to set audio encoder", L"Error", S_OK);
						return 1;
					}

					if (!_pipeline.audenc->Initiated)
					{
						DisplayMsg(L"Failed to initiate audio encoder", L"Error", S_OK);
						return 1;
					}					
										
					_pipeline.mux->AddAudioStream(_pipeline.audenc->OutputMediaType);
				}
			}

			_silenceGenerator.Start();

			LoopbackCaptureStart(&_pipeline);			

			while (_pipeline.audioCapBuffer->empty() || _pipeline.lastAudioTS < 500 * 10000)
			{
				Sleep(1);
			}

			_pipeline.videnc->Start();
			_pipeline.audenc->Start();

			_pipeline.mux->Start();

			// We start off in occluded state and we should immediate get a occlusion status window message
			Occluded = true;
		}
		else
		{
			// Nothing else to do, so try to present to write out to window if not occluded
			if (!Occluded)
			{
				Ret = OutMgr.UpdateApplicationWindow(ThreadMgr.GetPointerInfo(), &Occluded);
			}
		}

		// Check if for errors
		if (Ret != DUPL_RETURN_SUCCESS)
		{
			if (Ret == DUPL_RETURN_ERROR_EXPECTED)
			{
				// Some type of system transition is occurring so retry
				SetEvent(ExpectedErrorEvent);
			}
			else
			{
				// Unexpected error so exit
				break;
			}
		}
	}

	// Make sure all other threads have exited
	if (SetEvent(TerminateThreadsEvent))
	{
		ThreadMgr.WaitForThreadTermination();
	}

	// Clean up
	CloseHandle(UnexpectedErrorEvent);
	CloseHandle(ExpectedErrorEvent);
	CloseHandle(TerminateThreadsEvent);

	LoopbackCaptureClear();

	_silenceGenerator.Stop();

	if (_pipeline.videoCapBuffer)
	{
		while (!_pipeline.videoCapBuffer->empty())
		{
			Sleep(10);
		}
	}

	if (_pipeline.videnc)
	{
		_pipeline.videnc->Stop();
	}

	if (_pipeline.videnc)
	{
		while (!_pipeline.videnc->Finished)
		{
			Sleep(10);
		}
	}

	if (_pipeline.mux)
	{
		_pipeline.mux->Stop();

		while (!_pipeline.mux->Finished)
		{
			Sleep(10);
		}

		if (_pipeline.mux->ShutdownAndSaveFile() != S_OK)
		{
			printLn("Unable to save output file.");
		}
	}

	Sleep(2000);

	if (msg.message == WM_QUIT)
	{
		// For a WM_QUIT message we should return the wParam value
		return static_cast<INT>(msg.wParam);
	}

	return 0;
}

void printLn(const char *outputStr) {
	char msgbuffer[100];
	sprintf(msgbuffer, "%s \n", outputStr);
	OutputDebugStringA(msgbuffer);
}

void DEBUG_OUTPUT(LPCWSTR lpszFormat, ...)
{
	va_list args;
	va_start(args, lpszFormat);
	TCHAR szBuffer[512]; // get rid of this hard-coded buffer

	// ReSharper disable once CppDeprecatedEntity
	_vsnwprintf(szBuffer, 511, lpszFormat, args);

	::OutputDebugString(szBuffer);	

	va_end(args);
}

void DEBUG_OUTPUT_S(LPCWSTR msg)
{
	DEBUG_OUTPUT(L"%s", msg);
}

BYTE* GetImageData(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* texture, /*OUT*/ int* nWidth, /*OUT*/ int* nHeight, int* pitch) 
{
	if (texture)
	{
		D3D11_TEXTURE2D_DESC description;
		texture->GetDesc(&description);

		// Staging buffer/texture
		D3D11_TEXTURE2D_DESC CopyBufferDesc;
		CopyBufferDesc.Width = description.Width;
		CopyBufferDesc.Height = description.Height;
		CopyBufferDesc.MipLevels = 1;
		CopyBufferDesc.ArraySize = 1;
		CopyBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		CopyBufferDesc.SampleDesc.Count = 1;
		CopyBufferDesc.SampleDesc.Quality = 0;
		CopyBufferDesc.Usage = D3D11_USAGE_STAGING;
		CopyBufferDesc.BindFlags = 0;
		CopyBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		CopyBufferDesc.MiscFlags = 0;

		ID3D11Texture2D* texTemp = nullptr;
		HRESULT hr = device->CreateTexture2D(&CopyBufferDesc, nullptr, &texTemp);
		if (FAILED(hr))
		{
			
		}

		context->CopyResource(texTemp, texture);

		D3D11_MAPPED_SUBRESOURCE  mapped;
		//unsigned int subresource = 0;
		hr = context->Map(texTemp, 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr))
		{
			DEBUG_OUTPUT_S(L"GetImageData - Map - FAILED");
			texTemp->Release();
			texTemp = nullptr;
			return nullptr;
		}

		*nWidth = description.Width;
		*nHeight = description.Height;
		*pitch = mapped.RowPitch;

		auto source = (BYTE*)(mapped.pData);
		const auto dest = new BYTE[(*nWidth)*(*nHeight) * 4];
		auto destTemp = dest;

		for (int i = 0; i < *nHeight; ++i)
		{
			memcpy(destTemp, source, *nWidth * 4);
			source += *pitch;
			destTemp += *nWidth * 4;
		}

		context->Unmap(texTemp, 0);

		texTemp->Release();
		texTemp = nullptr;

		return dest;
	}
	// ReSharper disable once CppRedundantElseKeywordInsideCompoundStatement
	else
	{
		DEBUG_OUTPUT_S(L"GetImageData - texture null - FAILED \n");
		return nullptr;
	}
}

HRESULT EncodeFrame(_In_ ID3D11Device* device, _In_ ID3D11DeviceContext* context, _In_ ID3D11Texture2D *frameData, INT64 timestamp, INT64 duration)
{
	if (!_pipeline.videnc->Initiated)
	{
		return E_FAIL;
	}

	if (_pipeline.videoCapBuffer->isFull())
	{
		return S_OK;
	}

	D3D11_TEXTURE2D_DESC description;
	frameData->GetDesc(&description);

	int widthx = 0;
	int heightx = 0;
	int pitch = 0;

	BYTE* data = GetImageData(device, context, frameData, &widthx, &heightx, &pitch);
	
	const auto frame = new RAWVideoFrame();
	frame->Buffer = data;
	frame->BufferSize = heightx * pitch;
	frame->Info.Width = widthx;
	frame->Info.Height = heightx;
	frame->Info.Stride = pitch;
	
	IMFSample* videoSampleNV12 = _colorConv.BGR32ToNV12S(frame, false);
	videoSampleNV12->SetSampleTime(timestamp * 10000);
	videoSampleNV12->SetSampleDuration(duration * 10000);
		
	_pipeline.videoCapBuffer->push(videoSampleNV12);

	//SafeReleaseSample(&videoSampleNV12);
	//videoSampleNV12 = nullptr;

	free(data);

	return S_OK;

	//	// TODO
	//	// Send encDataBuffer to an rtp endpoint or udp endpoint
	//}
}

//
// Shows help
//
void ShowHelp()
{
	DisplayMsg(L"The following optional parameters can be used -\n  /output [all | n]\t\tto duplicate all outputs or the nth output\n  /?\t\t\tto display this help section",
		L"Proper usage", S_OK);
}

//
// Process command line parameters
//
bool ProcessCmdline(_Out_ INT* Output)
{
	*Output = -1;

	// __argv and __argc are global vars set by system
	for (UINT i = 1; i < static_cast<UINT>(__argc); ++i)
	{
		if ((strcmp(__argv[i], "-output") == 0) ||
			(strcmp(__argv[i], "/output") == 0))
		{
			if (++i >= static_cast<UINT>(__argc))
			{
				return false;
			}

			if (strcmp(__argv[i], "all") == 0)
			{
				*Output = -1;
			}
			else
			{
				*Output = atoi(__argv[i]);
			}

			continue;
		}
		else
		{
			return false;
		}
	}
	return true;
}

//
// Window message processor
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_SIZE:
	{
		// Tell output manager that window size has changed
		OutMgr.WindowResize();
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//
// Entry point for new duplication threads
//
DWORD WINAPI DDProc(_In_ void* Param)
{
	// Classes
	DISPLAYMANAGER DispMgr;
	DUPLICATIONMANAGER DuplMgr;

	// D3D objects
	ID3D11Texture2D* SharedSurf = nullptr;
	IDXGIKeyedMutex* KeyMutex = nullptr;

	// Data passed in from thread creation
	auto TData = reinterpret_cast<THREAD_DATA*>(Param);

	// Get desktop
	DUPL_RETURN Ret;
	HDESK CurrentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
	if (!CurrentDesktop)
	{
		// We do not have access to the desktop so request a retry
		SetEvent(TData->ExpectedErrorEvent);
		Ret = DUPL_RETURN_ERROR_EXPECTED;
		goto Exit;
	}

	// Attach desktop to this thread
	const bool DesktopAttached = SetThreadDesktop(CurrentDesktop) != 0;
	CloseDesktop(CurrentDesktop);
	CurrentDesktop = nullptr;
	if (!DesktopAttached)
	{
		// We do not have access to the desktop so request a retry
		Ret = DUPL_RETURN_ERROR_EXPECTED;
		goto Exit;
	}

	// New display manager
	DispMgr.InitD3D(&TData->DxRes);

	// Obtain handle to sync shared Surface
	HRESULT hr = TData->DxRes.Device->OpenSharedResource(TData->TexSharedHandle, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&SharedSurf));
	if (FAILED(hr))
	{
		Ret = ProcessFailure(TData->DxRes.Device, L"Opening shared texture failed", L"Error", hr, SystemTransitionsExpectedErrors);
		goto Exit;
	}

	hr = SharedSurf->QueryInterface(__uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&KeyMutex));
	if (FAILED(hr))
	{
		Ret = ProcessFailure(nullptr, L"Failed to get keyed mutex interface in spawned thread", L"Error", hr);
		goto Exit;
	}

	// Make duplication manager
	Ret = DuplMgr.InitDupl(TData->DxRes.Device, TData->Output);
	if (Ret != DUPL_RETURN_SUCCESS)
	{
		goto Exit;
	}

	// Get output description
	DXGI_OUTPUT_DESC DesktopDesc;
	RtlZeroMemory(&DesktopDesc, sizeof(DXGI_OUTPUT_DESC));
	DuplMgr.GetOutputDesc(&DesktopDesc);

	// Main duplication loop
	bool WaitToProcessCurrentFrame = false;
	FRAME_DATA CurrentData;

	const double frameRate = 10;
	const high_resolution_clock::time_point firstFrameTimestamp = high_resolution_clock::now();
	const INT64 frameDuration = 1000 / frameRate;
	INT64 frameNumber = 0;

	high_resolution_clock::time_point currentTimestamp = high_resolution_clock::time_point();

	while ((WaitForSingleObjectEx(TData->TerminateThreadsEvent, 0, FALSE) == WAIT_TIMEOUT))
	{
		if (!WaitToProcessCurrentFrame)
		{
			// Get new frame from desktop duplication
			bool TimeOut;
			Ret = DuplMgr.GetFrame(&CurrentData, &TimeOut);
			if (Ret != DUPL_RETURN_SUCCESS)
			{
				// An error occurred getting the next frame drop out of loop which
				// will check if it was expected or not
				break;
			}

			// Check for timeout
			if (TimeOut)
			{
				// No new frame at the moment
				continue;
			}
		}

		currentTimestamp = high_resolution_clock::now();
		//duration<double> time_span = duration_cast<duration<double>>(currentTimestamp - lastFrameTimestamp);
		duration<double> time_elapsed = duration_cast<duration<double>>(currentTimestamp - firstFrameTimestamp);

		while (time_elapsed.count() * 1000 < frameDuration * frameNumber)
		{
			Sleep(2);
			currentTimestamp = high_resolution_clock::now();
			time_elapsed = duration_cast<duration<double>>(currentTimestamp - firstFrameTimestamp);
			//time_span = duration_cast<duration<double>>(currentTimestamp - lastFrameTimestamp);
		}

		high_resolution_clock::time_point lastFrameTimestamp = high_resolution_clock::now();
		INT64 frameTimestamp = frameNumber * frameDuration;

		const auto timeElapsedMS = static_cast<INT64>(time_elapsed.count() * 1000);
		DEBUG_OUTPUT(L"Frame number: %lld, timestamp: %lld, timestamp diff: %lld\n", frameNumber, frameTimestamp, timeElapsedMS);

		if (frameTimestamp + frameDuration < timeElapsedMS)
		{
			frameTimestamp = timeElapsedMS;
		}

		frameNumber++;

		// We have a new frame so try and process it
		// Try to acquire keyed mutex in order to access shared surface
		hr = KeyMutex->AcquireSync(0, 1000);
		if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
		{
			// Can't use shared surface right now, try again later
			WaitToProcessCurrentFrame = true;
			continue;
		}
		else if (FAILED(hr))
		{
			// Generic unknown failure
			Ret = ProcessFailure(TData->DxRes.Device, L"Unexpected error acquiring KeyMutex", L"Error", hr, SystemTransitionsExpectedErrors);
			DuplMgr.DoneWithFrame();
			break;
		}

		// We can now process the current frame
		WaitToProcessCurrentFrame = false;

		// Get mouse info
		Ret = DuplMgr.GetMouse(TData->PtrInfo, &(CurrentData.FrameInfo), TData->OffsetX, TData->OffsetY);
		if (Ret != DUPL_RETURN_SUCCESS)
		{
			DuplMgr.DoneWithFrame();
			KeyMutex->ReleaseSync(1);
			break;
		}

		// We have the new frame with the mouse info - encode this data
		hr = EncodeFrame(TData->DxRes.Device, TData->DxRes.Context, CurrentData.Frame, frameTimestamp, frameDuration);

		// the video sample might need to be added to the IMFBytestream
		if (FAILED(hr)) {
			printLn("Encoding frame failed");
		}

		// encode function
		// Process new frame
		Ret = DispMgr.ProcessFrame(&CurrentData, SharedSurf, TData->OffsetX, TData->OffsetY, &DesktopDesc);
		if (Ret != DUPL_RETURN_SUCCESS)
		{
			DuplMgr.DoneWithFrame();
			KeyMutex->ReleaseSync(1);
			break;
		}

		// Release acquired keyed mutex
		hr = KeyMutex->ReleaseSync(1);
		if (FAILED(hr))
		{
			Ret = ProcessFailure(TData->DxRes.Device, L"Unexpected error releasing the keyed mutex", L"Error", hr, SystemTransitionsExpectedErrors);
			DuplMgr.DoneWithFrame();
			break;
		}

		// Release frame back to desktop duplication
		Ret = DuplMgr.DoneWithFrame();
		if (Ret != DUPL_RETURN_SUCCESS)
		{
			break;
		}
	}
Exit:
	if (Ret != DUPL_RETURN_SUCCESS)
	{
		if (Ret == DUPL_RETURN_ERROR_EXPECTED)
		{
			// The system is in a transition state so request the duplication be restarted
			SetEvent(TData->ExpectedErrorEvent);
		}
		else
		{
			// Unexpected error so exit the application
			SetEvent(TData->UnexpectedErrorEvent);
		}
	}

	if (SharedSurf)
	{
		SharedSurf->Release();
		SharedSurf = nullptr;
	}

	if (KeyMutex)
	{
		KeyMutex->Release();
		KeyMutex = nullptr;
	}

	return 0;
}

_Post_satisfies_(return != DUPL_RETURN_SUCCESS)
DUPL_RETURN ProcessFailure(_In_opt_ ID3D11Device* Device, _In_ LPCWSTR Str, _In_ LPCWSTR Title, HRESULT hr, _In_opt_z_ HRESULT* ExpectedErrors)
{
	HRESULT TranslatedHr;

	// On an error check if the DX device is lost
	if (Device)
	{
		const HRESULT DeviceRemovedReason = Device->GetDeviceRemovedReason();

		switch (DeviceRemovedReason)
		{
		case DXGI_ERROR_DEVICE_REMOVED:
		case DXGI_ERROR_DEVICE_RESET:
			case static_cast<HRESULT>(E_OUTOFMEMORY) :
			{
				// Our device has been stopped due to an external event on the GPU so map them all to
				// device removed and continue processing the condition
				TranslatedHr = DXGI_ERROR_DEVICE_REMOVED;
				break;
			}

			case S_OK:
			{
				// Device is not removed so use original error
				TranslatedHr = hr;
				break;
			}

			default:
			{
				// Device is removed but not a error we want to remap
				TranslatedHr = DeviceRemovedReason;
			}
		}
	}
	else
	{
		TranslatedHr = hr;
	}

	// Check if this error was expected or not
	if (ExpectedErrors)
	{
		HRESULT* CurrentResult = ExpectedErrors;

		while (*CurrentResult != S_OK)
		{
			if (*(CurrentResult++) == TranslatedHr)
			{
				return DUPL_RETURN_ERROR_EXPECTED;
			}
		}
	}

	// Error was not expected so display the message box
	DisplayMsg(Str, Title, TranslatedHr);

	return DUPL_RETURN_ERROR_UNEXPECTED;
}

//
// Displays a message
//
void DisplayMsg(_In_ LPCWSTR Str, _In_ LPCWSTR Title, HRESULT hr)
{
	if (SUCCEEDED(hr))
	{
		MessageBoxW(nullptr, Str, Title, MB_OK);
		return;
	}

	const UINT StringLen = (UINT)(wcslen(Str) + sizeof(" with HRESULT 0x########."));
	wchar_t* OutStr = new wchar_t[StringLen];
	if (!OutStr)
	{
		return;
	}

	const INT LenWritten = swprintf_s(OutStr, StringLen, L"%s with 0x%X.", Str, hr);
	if (LenWritten != -1)
	{
		MessageBoxW(nullptr, OutStr, Title, MB_OK);
	}

	delete[] OutStr;
}

// ReSharper restore CppInconsistentNaming
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//wprintf_s(L"Usage: %s inputaudio.mp3, %s output.wm*, %Encoding Type: CBR, VBR\n");

#include <iostream> 
#include <mfapi.h>
#include <mfobjects.h>
#include <limits.h>
#include <Mferror.h>
#include <mftransform.h>
#include "OutputManager.h"
#include "DisplayManager.h"
#include "DuplicationManager.h"
#include "ThreadManager.h"
#include <wmcodecdsp.h>        // Windows Media DSP interfaces
#include "Evr.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "evr.lib")

//
// Globals
//
OUTPUTMANAGER OutMgr;
IUnknown *spTransformUnk = NULL;
IMFTransform *pTransform = NULL;
IMFMediaType *pMFTInputMediaType = NULL, *pMFTOutputMediaType = NULL;
DWORD mftStatus = 0;
IMFSample *videoSample = NULL;

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
//HRESULT CreateH264Encoder();
HRESULT FindEncoder(const GUID& subtype, BOOL bAudio, IMFTransform **ppEncoder);
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
    public :
        DYNAMIC_WAIT();
        ~DYNAMIC_WAIT();

        void Wait();

    private :

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
    LARGE_INTEGER CurrentQPC = {0};

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
	HRESULT encoderResult = S_OK;
    INT SingleOutput;

    // Synchronization
    HANDLE UnexpectedErrorEvent = nullptr;
    HANDLE ExpectedErrorEvent = nullptr;
    HANDLE TerminateThreadsEvent = nullptr;

    // Window
    HWND WindowHandle = nullptr;

    bool CmdResult = ProcessCmdline(&SingleOutput);
    if (!CmdResult)
    {
        ShowHelp();
        return 0;
    }

    // Event used by the threads to signal an unexpected error and we want to quit the app
    UnexpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!UnexpectedErrorEvent)
    {
        ProcessFailure(nullptr, L"UnexpectedErrorEvent creation failed", L"Error", E_UNEXPECTED);
        return 0;
    }

    // Event for when a thread encounters an expected error
    ExpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ExpectedErrorEvent)
    {
        ProcessFailure(nullptr, L"ExpectedErrorEvent creation failed", L"Error", E_UNEXPECTED);
        return 0;
    }

    // Event to tell spawned threads to quit
    TerminateThreadsEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
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
    Wc.cbSize           = sizeof(WNDCLASSEXW);
    Wc.style            = CS_HREDRAW | CS_VREDRAW;
    Wc.lpfnWndProc      = WndProc;
    Wc.cbClsExtra       = 0;
    Wc.cbWndExtra       = 0;
    Wc.hInstance        = hInstance;
    Wc.hIcon            = nullptr;
    Wc.hCursor          = Cursor;
    Wc.hbrBackground    = nullptr;
    Wc.lpszMenuName     = nullptr;
    Wc.lpszClassName    = L"ddasample";
    Wc.hIconSm          = nullptr;
    if (!RegisterClassExW(&Wc))
    {
        ProcessFailure(nullptr, L"Window class registration failed", L"Error", E_UNEXPECTED);
        return 0;
    }

    // Create window
    RECT WindowRect = {0, 0, 800, 600};
    AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);
    WindowHandle = CreateWindowW(L"ddasample", L"DXGI desktop duplication sample",
                           WS_OVERLAPPEDWINDOW,
                           0, 0,
                           WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top,
                           nullptr, nullptr, hInstance, nullptr);
    if (!WindowHandle)
    {
        ProcessFailure(nullptr, L"Window creation failed", L"Error", E_FAIL);
        return 0;
    }

    DestroyCursor(Cursor);

    ShowWindow(WindowHandle, nCmdShow);
    UpdateWindow(WindowHandle);

	//CreateH264Encoder();
	encoderResult = FindEncoder(MFVideoFormat_H264, FALSE, &pTransform);
	// enumerate the encoders instead and use them
	if (FAILED(encoderResult)) {
		printLn("FAILED TO FIND ENCODER");
	}
	
	printLn("THIS IS A TEST");
    THREADMANAGER ThreadMgr;
    RECT DeskBounds;
    UINT OutputCount;

    // Message loop (attempts to update screen when no other messages to process)
    MSG msg = {0};
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
                // Present may not be occluded now so try again
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
                // First time through the loop so nothing to clean up
                FirstTime = false;
            }

            // Re-initialize
            Ret = OutMgr.InitOutput(WindowHandle, SingleOutput, &OutputCount, &DeskBounds);
            if (Ret == DUPL_RETURN_SUCCESS)
            {
                HANDLE SharedHandle = OutMgr.GetSharedHandle();
                if (SharedHandle)
                {
                    Ret = ThreadMgr.Initialize(SingleOutput, OutputCount, UnexpectedErrorEvent, ExpectedErrorEvent, TerminateThreadsEvent, SharedHandle, &DeskBounds);
                }
                else
                {
                    DisplayMsg(L"Failed to get handle of shared surface", L"Error", S_OK);
                    Ret = DUPL_RETURN_ERROR_UNEXPECTED;
                }
            }

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

    if (msg.message == WM_QUIT)
    {
        // For a WM_QUIT message we should return the wParam value
        return static_cast<INT>(msg.wParam);
    }

    return 0;
}

HRESULT FindEncoder(const GUID& subtype, BOOL bAudio, IMFTransform **ppEncoder) {
	HRESULT hr = S_OK;
	UINT32 count = 0;

	CLSID *ppCLSIDs = NULL;

	MFT_REGISTER_TYPE_INFO info = { 0 };

	info.guidMajorType = bAudio ? MFMediaType_Audio : MFMediaType_Video;
	info.guidSubtype = subtype;

	hr = MFTEnum(
		bAudio ? MFT_CATEGORY_AUDIO_ENCODER : MFT_CATEGORY_VIDEO_ENCODER,
		0,
		NULL,
		&info,
		NULL,
		&ppCLSIDs,
		&count
	);

	if (SUCCEEDED(hr) && count == 0) {
		hr = MF_E_TOPO_CODEC_NOT_FOUND;
	}

	// create the first encoder in the list

	if (SUCCEEDED(hr)) {
		hr = CoCreateInstance(ppCLSIDs[0], NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(ppEncoder));
		printLn("Encoder found");
	}
	

	CoTaskMemFree(ppCLSIDs);
	return hr;
}

void printLn(const char *outputStr) {
	char msgbuffer[100];
	sprintf(msgbuffer, "%s \n", outputStr);
	OutputDebugStringA(msgbuffer);
}

// Create H264 Encoder
//HRESULT CreateH264Encoder() {
//	HRESULT result = S_OK;
//	
//	result = CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, (void**)&spTransformUnk);
//	if (FAILED(result)) {
//		wprintf(L"Failed to create H264 Encoder");
//		goto Exit; // TODO exit from here
//	}
//	result = spTransformUnk->QueryInterface(IID_PPV_ARGS(&pTransform));
//	if (FAILED(result)) {
//		wprintf(L"Failed to get IMFTransform interface from H264 encoder MFT object");
//		goto Exit; // TODO exit from here
//	}
//
//	// H264 encoder only supports 1 input and 1 output stream
//
//	// Set the output types
//	MFCreateMediaType(&pMFTOutputMediaType);
//	pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
//	pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
//	pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000);
//
//	result = MFSetAttributeSize(pMFTOutputMediaType, MF_MT_FRAME_SIZE, 640, 480);
//	if (FAILED(result)) {
//		wprintf(L"Failed to set frame size");
//		goto Exit; // TODO exit from here
//	}
//
//	result = MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_FRAME_RATE, 30, 1);
//	if (FAILED(result)) {
//		wprintf(L"Failed to set frame rate");
//		goto Exit; // TODO exit from here
//	}
//
//	result = MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
//
//	if (FAILED(result)) {
//		wprintf(L"Failed to set aspect ratio");
//		goto Exit; // TODO exit from here
//	}
//
//	pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);
//	pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
//
//	// Flags can be passed in here as the 3rd arg
//	result = pTransform->SetOutputType(0, pMFTOutputMediaType, 0);
//	if (FAILED(result)) {
//		wprintf(L"Failed to set output type on H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	// Set the input type
//	MFCreateMediaType(&pMFTInputMediaType);
//	pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
//	pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32); // This may need to be tweaked
//
//	result = MFSetAttributeSize(pMFTInputMediaType, MF_MT_FRAME_SIZE, 640, 480); // This is the frame size
//	if (FAILED(result)) {
//		wprintf(L"Failed to set frame size on H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	result = MFSetAttributeRatio(pMFTInputMediaType, MF_MT_FRAME_RATE, 60, 1); // Frame rate - up sample this (was 30)
//	if (FAILED(result)) {
//		wprintf(L"Failed to set frame rate on H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	result = MFSetAttributeRatio(pMFTInputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
//	if (FAILED(result)) {
//		wprintf(L"Failed to set aspect ratio on H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);
//
//	result = pTransform->SetInputType(0, pMFTInputMediaType, 0);
//	if (FAILED(result)) {
//		wprintf(L"Failed to set input media type on H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	result = pTransform->GetInputStatus(0, &mftStatus);
//	if (FAILED(result)) {
//		wprintf(L"Failed to get input status from H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
//		printf("E: ApplyTransform() pTransform->GetInputStatus() not accept data \n");
//		goto Exit; // TODO exit from here
//	}
//
//	result = pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
//	if (FAILED(result)) {
//		wprintf(L"Failed to process FLUSH command H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	result = pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
//	if (FAILED(result)) {
//		wprintf(L"Failed to process BEGIN_STREAMING command H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//	result = pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL);
//	if (FAILED(result)) {
//		wprintf(L"Failed to process START_OF_STREAM command H.264 MFT");
//		goto Exit; // TODO exit from here
//	}
//
//Exit:
//	printf("Failed to set up transform");
//
//	return result;
//}

HRESULT EncodeFrame(_In_ ID3D11Texture2D *Frame_Data, _Outptr_ IMFSample *Sample) {
	DWORD processOutputStatus = 0;
	IMFMediaBuffer *encBuffer = NULL;
	MFT_OUTPUT_DATA_BUFFER encDataBuffer;
	HRESULT mftEncProcessOutput = S_OK;
	MFT_OUTPUT_STREAM_INFO StreamInfo;
	DWORD mftEncFlags;
	IMFSample *mftEncSample = NULL;
	HRESULT result = S_OK;

	memset(&encDataBuffer, 0, sizeof encDataBuffer);

	result = MFCreateVideoSampleFromSurface(Frame_Data, &videoSample);
	if (FAILED(result)) {
		printLn("Failed to create IMFSample from ID311Texture2D object");
		goto Exit;
	}

	// IMFSample created
	// pass the sample to the H.264 Transform
	result = pTransform->ProcessInput(0, videoSample, 0);
	if (FAILED(result)) {
		printLn("The H264 Process Input call failed");
		goto Exit;
	}

	// Query the transform for the output status
	// https://msdn.microsoft.com/en-us/library/windows/desktop/aa965264

	result = pTransform->GetOutputStatus(&mftEncFlags);
	if (FAILED(result)) {
		printLn("H264 MFT GetOutputStatus Failed");
		goto Exit;
	}



	// Sample is ready
	if (mftEncFlags == MFT_OUTPUT_STATUS_SAMPLE_READY) {
		result = pTransform->GetOutputStreamInfo(0, &StreamInfo);
		if (FAILED(result)) {
			printLn("Failed to get output stream info from H@^$ MFT");
			goto Exit;
		}

		result = MFCreateSample(&mftEncSample);
		if (FAILED(result)) {
			printLn("Failed to create MF sample");
			goto Exit;
		}

		// The buffer created with the call below is reference counted and auto released
		// see (https://msdn.microsoft.com/en-us/library/windows/desktop/bb530123(v=vs.85).aspx).
		// TODO check if this buffer is necessary
		result = MFCreateMemoryBuffer(StreamInfo.cbSize, &encBuffer);
		if (FAILED(result)) {
			printLn("Failed to create memory buffer");
			goto Exit;
		}

		result = mftEncSample->AddBuffer(encBuffer);
		if (FAILED(result)) {
			printLn("Failed to add sample to buffer");
			goto Exit;
		}

		encDataBuffer.dwStreamID = 0;
		encDataBuffer.dwStatus = 0;
		encDataBuffer.pEvents = NULL;
		encDataBuffer.pSample = mftEncSample;

		// This processes the frame
		mftEncProcessOutput = pTransform->ProcessOutput(0, 1, &encDataBuffer, &processOutputStatus);

		// EncDataBuffer has encoded the frame

		// TODO
		// Send encDataBuffer to an rtp endpoint or udp endpoint
	}

	return result;
Exit:
	return 0;
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
    THREAD_DATA* TData = reinterpret_cast<THREAD_DATA*>(Param);

    // Get desktop
    DUPL_RETURN Ret;
    HDESK CurrentDesktop = nullptr;
    CurrentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
    if (!CurrentDesktop)
    {
        // We do not have access to the desktop so request a retry
        SetEvent(TData->ExpectedErrorEvent);
        Ret = DUPL_RETURN_ERROR_EXPECTED;
        goto Exit;
    }

    // Attach desktop to this thread
    bool DesktopAttached = SetThreadDesktop(CurrentDesktop) != 0;
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
    if (FAILED (hr))
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
		
		hr = EncodeFrame(CurrentData.Frame, videoSample);
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
        HRESULT DeviceRemovedReason = Device->GetDeviceRemovedReason();

        switch (DeviceRemovedReason)
        {
            case DXGI_ERROR_DEVICE_REMOVED :
            case DXGI_ERROR_DEVICE_RESET :
            case static_cast<HRESULT>(E_OUTOFMEMORY) :
            {
                // Our device has been stopped due to an external event on the GPU so map them all to
                // device removed and continue processing the condition
                TranslatedHr = DXGI_ERROR_DEVICE_REMOVED;
                break;
            }

            case S_OK :
            {
                // Device is not removed so use original error
                TranslatedHr = hr;
                break;
            }

            default :
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

    INT LenWritten = swprintf_s(OutStr, StringLen, L"%s with 0x%X.", Str, hr);
    if (LenWritten != -1)
    {
        MessageBoxW(nullptr, OutStr, Title, MB_OK);
    }

    delete [] OutStr;
}

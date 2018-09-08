#include "AudioLoopbackSource.h"

#include <Windows.h>

#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <cstdio>
#include <avrt.h>
#include <cassert>
#include "VFDebug.h"
#include "MFMSAACEncoder.h"
#include "WAVWriter.h"

#define DEBUG_WAV_OUTPUT 0

HRESULT get_default_device(IMMDevice **ppMMDevice);

IAudioCaptureClient *pAudioCaptureClient;
IAudioClient *pAudioClient;
HANDLE hTask;
bool bDiscontinuityDetected; 
bool bVeryFirstPacket; 
IMMDevice *m_pMMDevice;
UINT32 nBlockAlign;
UINT32 pnFrames;

CRITICAL_SECTION csMyLock;  // shared critical section. Starts not locked...

bool shouldStop = false;

BYTE pBufLocal[1024 * 1024]; // 1MB is quite awhile 
long pBufOriginalSize = 1024 * 1024;

long pBufLocalCurrentEndLocation = 0;

long expectedMaxBufferSize = 1024 * 1024; 

HANDLE m_hThread;

static DWORD WINAPI propagateBufferForever(LPVOID pv);

CRITICAL_SECTION gSharedState;

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { return hres; }
#define REFTIMES_PER_SEC  10000000

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

HRESULT get_default_device(IMMDevice **ppMMDevice)
{
	IMMDeviceEnumerator *pMMDeviceEnumerator;
	// activate a device enumerator
	HRESULT hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void**)&pMMDeviceEnumerator
	);

	if (FAILED(hr)) 
	{
		printf("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08lx\n", hr);
		return hr;
	}

	// get the default render endpoint
	hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, ppMMDevice);
	pMMDeviceEnumerator->Release();
	if (FAILED(hr))
	{
		printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08lx\n", hr);
		return hr;
	}

	return S_OK;
}

void propagateWithRawCurrentFormat(WAVEFORMATEX *toThis) 
{
	WAVEFORMATEX *pwfx;
	IMMDevice *pMMDevice;
	IAudioClient *pAudioClient;
	DWORD nTaskIndex = 0;
	const HANDLE hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);

	HRESULT hr = get_default_device(&pMMDevice);
	if (FAILED(hr))
	{
		assert(false);
	}

	// activate an (the default, for us, since we want loopback) IAudioClient
	hr = pMMDevice->Activate(
		__uuidof(IAudioClient),
		CLSCTX_ALL, nullptr,
		(void**)&pAudioClient
	);

	if (FAILED(hr))
	{
		XTraceE(L"IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
		assert(false);
	}

	hr = pAudioClient->GetMixFormat(&pwfx);
	if (FAILED(hr))
	{
		XTraceE(L"IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
		CoTaskMemFree(pwfx);
		pAudioClient->Release();
		assert(false);
	}

	pAudioClient->Stop();
	AvRevertMmThreadCharacteristics(hTask);
	pAudioClient->Release();
	pMMDevice->Release();
	memcpy(toThis, pwfx, sizeof(WAVEFORMATEX));
	CoTaskMemFree(pwfx);
}

#if DEBUG_WAV_OUTPUT
	WAVWriter* wavWriter = nullptr;
#endif

// we only call this once...per hit of the play button :)
HRESULT LoopbackCaptureSetup(int* channels, int* bps, int* sampleRate, int* bufferSize, int* blockAlign)
{
#if DEBUG_WAV_OUTPUT
	wavWriter = new WAVWriter(L"c:\\vf\\output_test.wav");
#endif

	// Initialize the critical section one time only.
	if (!InitializeCriticalSectionAndSpinCount(&csMyLock, 0x00000400))
		return E_FAIL;

	if (!InitializeCriticalSectionAndSpinCount(&gSharedState, 0x00000400))
		return E_FAIL;

	assert(!shouldStop); // double start would be odd...
	shouldStop = false; // allow graphs to restart, if they so desire...
	pnFrames = 0;
	const bool bInt16 = true; // makes it actually work, for some reason...my guess is it's a more common format

	HRESULT hr = get_default_device(&m_pMMDevice); // so it can re-place our pointer...
	if (FAILED(hr)) {
		return hr;
	}

	// tell it to not overflow one buffer's worth <sigh> not sure if this is right or not, and thus we don't "cache" or "buffer" more than that much currently...
	// but a buffer size is a buffer size...hmm...as long as we keep it small though...
	assert(expectedMaxBufferSize <= pBufOriginalSize);
	// activate an (the default, for us, since we want loopback) IAudioClient
	hr = m_pMMDevice->Activate(
		__uuidof(IAudioClient),
		CLSCTX_ALL, nullptr,
		(void**)&pAudioClient
	);
	if (FAILED(hr)) {
		XTraceE(L"IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
		return hr;
	}

	// get the default device periodicity, why? I don't know...
	REFERENCE_TIME hnsDefaultDevicePeriod;
	hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, nullptr);
	if (FAILED(hr)) {
		XTraceE(L"IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
		pAudioClient->Release();
		return hr;
	}

	// get the default device format (incoming...)
	WAVEFORMATEX *pwfx; // incoming wave...
	// apparently propogated by GetMixFormat...
	hr = pAudioClient->GetMixFormat(&pwfx); // we free pwfx
	if (FAILED(hr)) {
		XTraceE(L"IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
		CoTaskMemFree(pwfx);
		pAudioClient->Release();
		return hr;
	}

	if (bInt16) {
		// coerce int-XX wave format (like int-16 or int-32)
		// can do this in-place since we're not changing the size of the format
		// also, the engine will auto-convert from float to int for us
		switch (pwfx->wFormatTag) {
		case WAVE_FORMAT_IEEE_FLOAT:
			assert(false);// we never get here...I never have anyway...my guess is windows vista+ by default just uses WAVE_FORMAT_EXTENSIBLE
			pwfx->wFormatTag = WAVE_FORMAT_PCM;
			pwfx->wBitsPerSample = 16;
			pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
			pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
			break;

		case WAVE_FORMAT_EXTENSIBLE: // 65534
		{
			// naked scope for case-local variable
			PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
			if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat)) {
				// WE GET HERE!
				pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
				// convert it to PCM, but let it keep as many bits of precision as it has initially...though it always seems to be 32
				// comment this out and set wBitsPerSample to  pwfex->wBitsPerSample = getBitsPerSample(); to get an arguably "better" quality 32 bit pcm
				// unfortunately flash media live encoder basically rejects 32 bit pcm, and it's not a huge gain sound quality-wise, so disabled for now.
				pwfx->wBitsPerSample = 16;
				pEx->Samples.wValidBitsPerSample = pwfx->wBitsPerSample;
				pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
				pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
				// see also setupPwfex method
			}
			else {
				XTraceE(L"Don't know how to coerce mix format to int-16\n");
				CoTaskMemFree(pwfx);
				pAudioClient->Release();
				return E_UNEXPECTED;
			}
		}
		break;

		default:
			XTraceE(L"Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16\n", pwfx->wFormatTag);
			CoTaskMemFree(pwfx);
			pAudioClient->Release();
			return E_UNEXPECTED;
		}
	}

	*channels = pwfx->nChannels;
	*sampleRate = pwfx->nSamplesPerSec;
	*bps = 16;
	*blockAlign = pwfx->nBlockAlign;

#if DEBUG_WAV_OUTPUT
	wavWriter->OpenFile();
	wavWriter->WriteHeader(*sampleRate, *channels);
#endif

	nBlockAlign = pwfx->nBlockAlign;

	IMMDeviceEnumerator *pEnumerator = nullptr;
	IMMDevice *pDevice = nullptr;

	IAudioRenderClient *pRenderClient = nullptr;
	WAVEFORMATEXTENSIBLE *captureDataFormat = nullptr;
	BYTE *captureData;

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;

	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, nullptr,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	EXIT_ON_ERROR(hr)

		hr = get_default_device(&pDevice);
	EXIT_ON_ERROR(hr)

		hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->GetMixFormat((WAVEFORMATEX **)&captureDataFormat);
	EXIT_ON_ERROR(hr)
		
		// Silence: initialise in sharedmode [this is the "silence" bug overwriter, so buffer doesn't matter as much...]
		hr = pAudioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			0,
			REFTIMES_PER_SEC, // buffer size a full 1.0s, though prolly doesn't matter here.
			0,
			pwfx,
			nullptr);
	EXIT_ON_ERROR(hr)

		// get the frame count
		UINT32  bufferFrameCount;
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)

		*bufferSize = bufferFrameCount;

	// create a render client
	hr = pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);
	EXIT_ON_ERROR(hr)

		// get the buffer
		hr = pRenderClient->GetBuffer(bufferFrameCount, &captureData);
	EXIT_ON_ERROR(hr)

		// release it
		hr = pRenderClient->ReleaseBuffer(bufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
	EXIT_ON_ERROR(hr)

		// release the audio client
		pAudioClient->Release();
	EXIT_ON_ERROR(hr)


		// create a new IAudioClient
		hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

		// -============================ now the sniffing code initialization stuff, direct from mauritius... ===================================

		// call IAudioClient::Initialize
		// note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
		// do not work together...
		// the "data ready" event never gets set
		// so we're going to have to do this in a timer-driven loop...

		hr = pAudioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_LOOPBACK,
			REFTIMES_PER_SEC, // buffer size a full 1.0s, seems ok VLC
			0, 
			pwfx, 
			nullptr
		);

	if (FAILED(hr)) {
		XTraceE(L"IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
		pAudioClient->Release();
		return hr;
	}

	CoTaskMemFree(pwfx);

	// activate an IAudioCaptureClient
	hr = pAudioClient->GetService(
		__uuidof(IAudioCaptureClient),
		(void**)&pAudioCaptureClient
	);

	if (FAILED(hr)) {
		XTraceE(L"IAudioClient::GetService(IAudioCaptureClient) failed: hr 0x%08x\n", hr);
		pAudioClient->Release();
		return hr;
	}

	// register with MMCSS
	DWORD nTaskIndex = 0;

	hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);
	if (nullptr == hTask) {
		const DWORD dwErr = GetLastError();
		XTraceE(L"AvSetMmThreadCharacteristics failed: last error = %u\n", dwErr);
		pAudioCaptureClient->Release();
		pAudioClient->Release();
		return HRESULT_FROM_WIN32(dwErr);
	}

	// call IAudioClient::Start
	hr = pAudioClient->Start();
	if (FAILED(hr)) {
		XTraceE(L"IAudioClient::Start failed: hr = 0x%08x\n", hr);
		AvRevertMmThreadCharacteristics(hTask);
		pAudioCaptureClient->Release();
		pAudioClient->Release();
		return hr;
	}

	// init some var's [XXXX using global vars doesn't work if I had 2 of these at the same time in the same process [?]]
	bDiscontinuityDetected = true;
	bVeryFirstPacket = true;

	return hr;	
}

HRESULT LoopbackCaptureStart(MFPipeline* pipeline)
{
	HRESULT hr = S_OK;

	// start the forever grabbing thread...
	DWORD dwThreadID;
	m_hThread = CreateThread(
		nullptr,
		0,
		propagateBufferForever,
		pipeline,
		0,
		&dwThreadID);

	if (!m_hThread)
	{
		const DWORD dwErr = GetLastError();
		return HRESULT_FROM_WIN32(dwErr);
	}
	else {
		// we...shouldn't need this...maybe?
		// seems to make no difference anyway, and probably won't hurt...
		hr = SetThreadPriority(m_hThread, THREAD_PRIORITY_TIME_CRITICAL);
		if (FAILED(hr)) {
			return hr;
		}
	}

	return hr;
}

HRESULT propagateBufferOnce(MFPipeline* pipeline);

int totalSuccessFullyread = 0;
int totalBlips = 0;
int totalOverflows = 0;

HRESULT propagateBufferOnce(MFPipeline* pipeline) 
{
	DWORD start_time = timeGetTime();

	while (!shouldStop) {
		UINT32 nNextPacketSize;
		HRESULT hr = pAudioCaptureClient->GetNextPacketSize(&nNextPacketSize); // get next packet, if one is ready...
		if (FAILED(hr)) {
			XTraceE(L"IAudioCaptureClient::GetNextPacketSize failed after %u frames: hr = 0x%08x\n", pnFrames, hr);
			pAudioClient->Stop();
			AvRevertMmThreadCharacteristics(hTask);
			pAudioCaptureClient->Release();
			pAudioClient->Release();
			return hr;
		}

		if (0 == nNextPacketSize)
		{
			// (CA) - this condition appears on Win8 when there is "silence" in the audio stream 
			// (CA) - we don't appear to hit this condition in Win7 .. so I have eliminated the logic in here for right now...

			/*
			// no data yet, we're waiting, between incoming chunks of audio. [occurs even with silence on the line--it just means no new data yet]
			DWORD millis_to_fill = (DWORD) (1.0/SECOND_FRACTIONS_TO_GRAB*1000); // truncate is ok :) -- 1s
			assert(millis_to_fill > 1); // sanity
			DWORD current_time = timeGetTime();
			if((current_time - start_time > millis_to_fill)) {
				// I don't think we ever get to here anymore...thankfully, since it's mostly broken code probably, anyway
				if(!gotAnyAtAll) {
				  // We get here under high load...
				  // ignore for now, but sleep more
				  ShowOutput("detected high amount of time without receiving a packet from the capturer!");
				  start_time = timeGetTime();
				  Sleep(0);
				}
			} else {
			  Sleep(1); // doesn't seem to hurt cpu--"sleep x ms"
			  continue;
			}
			*/
			Sleep(1);
			continue;
		}
		else
		{
			int gotAnyAtAll = TRUE;
			totalSuccessFullyread++;
		}

		// get the captured data
		BYTE *pData;
		UINT32 nNumFramesToRead;
		DWORD dwFlags;

		// I guess it gives us...as much audio as possible to read...probably

		hr = pAudioCaptureClient->GetBuffer(
			&pData,
			&nNumFramesToRead,
			&dwFlags,
			nullptr,
			nullptr
		); // ACTUALLY GET THE BUFFER which I assume it reads in the format of the fella we passed in
		
		if (FAILED(hr)) 
		{
			XTraceE(L"IAudioCaptureClient::GetBuffer failed after %u frames: hr = 0x%08x\n", pnFrames, hr);
			pAudioClient->Stop();
			AvRevertMmThreadCharacteristics(hTask);
			pAudioCaptureClient->Release();
			pAudioClient->Release();
			return hr;
		}

		{
			EnterCriticalSection(&gSharedState);

			//CAutoLock cAutoLockShared(&);  // for the booleans, we lock csMyLock later :| XXXX weird?

			if (dwFlags == 0) 
			{
				// the good case, got audio packet
				// we'll let fillbuffer set bDiscontinuityDetected = false; since it uses it to know if the next packet should restart, etc.
			}
			else if (bDiscontinuityDetected && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags)
			{
				XTraceE(L"Probably spurious glitch reported on first packet, or two discontinuity errors occurred before it read from the cached buffer\n");

				bDiscontinuityDetected = true; // won't hurt, even if it is a real first packet :)

				// XXXX it should probably clear the buffers if it ever gets discontinuity
				// or "let" it clear the buffers then send the new data on
				// as we have any left-over data that will be assigned a wrong timestamp
				// but it won't be too far wrong, compared to what it would otherwise be with always
				// assigning it the current graph timestamp, like we used to...
			}
			else if (AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags)
			{
				XTraceE(L"IAudioCaptureClient::discontinuity GetBuffer set flags to 0x%08x after %u frames\n", dwFlags, pnFrames);
				// expected your CPU gets behind or what not. I guess.
				/*pAudioClient->Stop();
				AvRevertMmThreadCharacteristics(hTask);
				pAudioCaptureClient->Release();
				pAudioClient->Release();
				return E_UNEXPECTED;*/

				bDiscontinuityDetected = true;
			}
			else if (AUDCLNT_BUFFERFLAGS_SILENT == dwFlags)
			{
				// ShowOutput("IAudioCaptureClient::silence (just) from GetBuffer after %u frames\n", pnFrames);
				// expected if there's silence (i.e. nothing playing), since we now include the "silence generator" work-around...
				// at least in windows 7, we get here...
			}
			else 
			{
				// probably silence + discontinuity
				XTraceE(L"IAudioCaptureClient::unknown discontinuity GetBuffer set flags to 0x%08x after %u frames\n", dwFlags, pnFrames);
				bDiscontinuityDetected = true; // probably is some type of discontinuity :P
			}

			if (bDiscontinuityDetected)
				totalBlips++;

			if (0 == nNumFramesToRead)
			{
				// we should probably never get here, right?
				  // my guess is that we don't, even in win8?
				  // I mean, this is probably really messed up it told us it had some data to grab, we try to grab it, it returns us nothing?
				  /*
				  ShowOutput("death failure: IAudioCaptureClient::GetBuffer said to read 0 frames after %u frames\n", pnFrames);
				  pAudioClient->Stop();
				  AvRevertMmThreadCharacteristics(hTask);
				  pAudioCaptureClient->Release();
				  pAudioClient->Release();
				  return E_UNEXPECTED;
				  */
			}

			pnFrames += nNumFramesToRead; // increment total count...		

			// lBytesToWrite typically 1792 bytes...
			const LONG lBytesToWrite = nNumFramesToRead * nBlockAlign; // nBlockAlign is "audio block size" or frame size, for one audio segment...
			{
				EnterCriticalSection(&csMyLock); // Lock the critical section, releases scope after block is over...

				if (pBufLocalCurrentEndLocation > expectedMaxBufferSize) 
				{
					// this happens during VLC pauses...
					// I have no idea what I'm doing here... this doesn't fix it, but helps a bit... TODO FINISH THIS
					// it seems like if you're just straight recording then you want this big...otherwise you want it like size 0 and non-threaded [pausing with graphedit, for example]... [?]
					// if you were recording skype, you'd want it non realtime...hmm...
					// it seems that if the cpu is loaded, we run into this if it's for the next packet...hmm...
					// so basically we don't accomodate realtime at all currently...hmmm...
					XTraceE(L"overfilled buffer, cancelling/flushing."); //over flow overflow appears VLC just keeps reading though, when paused [?] but not graphedit...or does it?
					pBufLocalCurrentEndLocation = 0;
					totalOverflows++;
					bDiscontinuityDetected = true;
				}

				for (INT i = 0; i < lBytesToWrite && pBufLocalCurrentEndLocation < expectedMaxBufferSize; i++) 
				{
					pBufLocal[pBufLocalCurrentEndLocation++] = pData[i];
				}

#if DEBUG_WAV_OUTPUT
				if (wavWriter)
				{
					wavWriter->WriteChunk((short*)pBufLocal, pBufLocalCurrentEndLocation);
				}
#endif

				if (pipeline->audenc)
				{
					const auto frame = new RAWAudioFrame();
					frame->BufferSize = pBufLocalCurrentEndLocation;
					frame->Buffer = (BYTE*)malloc(frame->BufferSize);

					auto* encoder = (MFMSAACEncoder*)pipeline->audenc;
					frame->Duration = (((int64_t)frame->BufferSize * 10000 * 1000) / ((encoder->AudioFormat.BPS / 8) * encoder->AudioFormat.Channels * encoder->AudioFormat.SampleRate));
					frame->Timestamp = pipeline->lastAudioTS;
					pipeline->lastAudioTS += frame->Duration;

					XTraceE(L"Audio source: sample timestamp is %lld\n", pipeline->lastAudioTS);

					memcpy(frame->Buffer, pBufLocal, frame->BufferSize);

					IMFSample* sample = MFMSAACEncoder::PCMToMFSample(frame);
					pipeline->audioCapBuffer->push(sample);
				}

				pBufLocalCurrentEndLocation = 0;

				LeaveCriticalSection(&csMyLock);
			}

			LeaveCriticalSection(&gSharedState);
		}

		hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
		if (FAILED(hr))
		{
			XTraceE(L"IAudioCaptureClient::ReleaseBuffer failed after %u frames: hr = 0x%08x\n", pnFrames, hr);
			pAudioClient->Stop();
			AvRevertMmThreadCharacteristics(hTask);
			pAudioCaptureClient->Release();
			pAudioClient->Release();
			return hr;
		}

		return hr;
	} // while !got anything && should continue loop

	return S_OK; // stop was called...
}

// iSize is max size of the BYTE buffer...so maybe...we should just drop it if we have past that size? hmm...we're probably
HRESULT LoopbackCaptureTakeFromBuffer(BYTE pBuf[], int iSize, WAVEFORMATEX* ifNotNullThenJustSetTypeOnly, LONG* totalBytesWrote)
{
	while (!shouldStop) { // allow this to exit, too, at shutdown, possibly a few times we kind of got stuck in here, waiting for more data, but it was shutdown so not receiving any more data :|
		{
			EnterCriticalSection(&csMyLock);

			if (pBufLocalCurrentEndLocation > 0) {
				// fails lodo is that ok though? 
				// assert(pBufLocalCurrentEndLocation <= expectedMaxBufferSize);
				const int totalToWrite = MIN(pBufLocalCurrentEndLocation, expectedMaxBufferSize);
				//ASSERT(totalToWrite <= iSize); // just in case...just in case almost...
				memcpy(pBuf, pBufLocal, totalToWrite);
				*totalBytesWrote = totalToWrite;
				pBufLocalCurrentEndLocation = 0;
				return S_OK;
			} // else fall through to sleep outside the lock...

			LeaveCriticalSection(&csMyLock);
		}
		// sleep outside the lock ...
		// using sleep doesn't seem to hurt the cpu
		// and it seems to not get many "discontinuity" messages currently...
		Sleep(1);
	}
	return E_FAIL; // we didn't fill anything...and are shutting down...
}

// clean up
void loopBackRelease()
{
	// tell running collector thread to end...
	shouldStop = true;
	WaitForSingleObject(m_hThread, INFINITE);
	CloseHandle(m_hThread);
	m_hThread = nullptr;
	pAudioClient->Stop();
	AvRevertMmThreadCharacteristics(hTask);
	pAudioCaptureClient->Release();
	pAudioClient->Release();
	m_pMMDevice->Release();
	// thread is done, we are exiting...
	pBufLocalCurrentEndLocation = 0;
}

void LoopbackCaptureClear() 
{
	loopBackRelease();

	EnterCriticalSection(&csMyLock); // Lock the critical section, releases scope after block is done...

	pBufLocalCurrentEndLocation = 0;
	bDiscontinuityDetected = 1; // it uses this for timestamping the next packet

	LeaveCriticalSection(&csMyLock);

	// Release resources used by the critical section object.
	DeleteCriticalSection(&csMyLock);
	DeleteCriticalSection(&gSharedState);

#if DEBUG_WAV_OUTPUT
	if (wavWriter)
	{
		wavWriter->CloseFile();

		delete wavWriter;
		wavWriter = nullptr;
	}
#endif
}

// called via reflection :)
static DWORD WINAPI propagateBufferForever(LPVOID pv) 
{
	auto* pipeline = (MFPipeline*)pv;

	while (!shouldStop)
	{
		const HRESULT hr = propagateBufferOnce(pipeline);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	return S_OK;
}

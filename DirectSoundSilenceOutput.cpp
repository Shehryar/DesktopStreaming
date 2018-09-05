#include "DirectSoundSilenceOutput.h"

#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <mmeapi.h>
#include <cassert>
#include <ctime>
#include <iostream>
#include <dsound.h>

DirectSoundSilenceOutput::DirectSoundSilenceOutput():
	_thread(nullptr),
	StopFlag(false), 
	Started(false)
{
}

DirectSoundSilenceOutput::~DirectSoundSilenceOutput()
{
}

HRESULT DirectSoundSilenceOutput::Start()
{
	HRESULT hr = S_OK;

	// start the forever grabbing thread...
	DWORD dwThreadID;
	_thread = CreateThread(
		nullptr,
		0,
		StartThread,
		this,
		0,
		&dwThreadID);

	if (!_thread)
	{
		const DWORD dwErr = GetLastError();
		return HRESULT_FROM_WIN32(dwErr);
	}
	else 
	{
		hr = SetThreadPriority(_thread, THREAD_PRIORITY_NORMAL);
		if (FAILED(hr)) 
		{
			return hr;
		}
	}

	while (!Started)
	{
		Sleep(10);
	}

	return hr;
}

void DirectSoundSilenceOutput::Stop()
{
	StopFlag = true;
	WaitForSingleObject(_thread, INFINITE);
	CloseHandle(_thread);
	_thread = nullptr;
}

static DWORD WINAPI StartThread(LPVOID pv)
{
	auto* silenceOutput = (DirectSoundSilenceOutput*)pv;

	LPDIRECTSOUND8 dsound;
	WAVEFORMATEX format;
	const int numchunks = 8;
	LPDIRECTSOUNDBUFFER dsbuf;
	DSBUFFERDESC buf_format;
	const int chunksize = 1024;
	uint8_t *data1, *data2;
	uint32_t size1, size2;

	memset(&format, 0, sizeof(WAVEFORMATEX));

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 1;
	format.wBitsPerSample = 16;
	format.nSamplesPerSec = 44100;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;
	format.cbSize = 0;

	assert(DirectSoundCreate8(NULL, &dsound, NULL) == DS_OK);	
	assert(dsound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY) == DS_OK);
	
	memset(&buf_format, 0, sizeof(DSBUFFERDESC));
	buf_format.dwSize = sizeof(buf_format);
	buf_format.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
	buf_format.dwFlags |= DSBCAPS_STICKYFOCUS;
	buf_format.dwBufferBytes = numchunks * chunksize;
	buf_format.dwReserved = 0;
	buf_format.lpwfxFormat = &format;

	const HRESULT res = dsound->CreateSoundBuffer(&buf_format, &dsbuf, nullptr);
	assert(res == DS_OK);

	srand(time(nullptr));

	assert(IDirectSoundBuffer_Lock(dsbuf, 0, buf_format.dwBufferBytes, (LPVOID *)&data1, (LPDWORD)&size1,
		(LPVOID *)&data2, (LPDWORD)&size2, DSBLOCK_ENTIREBUFFER) == DS_OK);

	memset(data1, 0, size1);
	memset(data2, 0, size2);

	IDirectSoundBuffer_Unlock(
		dsbuf,
		(LPVOID)data1, 
		(DWORD)size1,
		(LPVOID)data2,
		(DWORD)size2);

	assert(IDirectSoundBuffer_Play(dsbuf, 0, 0, DSBPLAY_LOOPING) == DS_OK);

	silenceOutput->Started = true;

	while (!silenceOutput->StopFlag)
	{
		Sleep(100);
	}

	return 0;
}
#pragma once

#include <Windows.h>

class DirectSoundSilenceOutput
{
	HANDLE _thread;
public:
	bool StopFlag;
	bool Started;

	DirectSoundSilenceOutput();
	~DirectSoundSilenceOutput();

	HRESULT Start();
	void Stop();
};

static DWORD WINAPI StartThread(LPVOID pv);

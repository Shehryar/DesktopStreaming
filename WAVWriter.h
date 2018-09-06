#pragma once
#include <string>

class WAVWriter
{
	std::wstring* _filename;
	std::ofstream* _stream;
	int64_t _dataSize;
public:
	WAVWriter(const wchar_t* filename);
	~WAVWriter();

	void WriteHeader(int sampleRate, short channels) const;

	void WriteChunk(short* buf, size_t bufSize);

	void OpenFile();
	void CloseFile();
};


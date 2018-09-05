#include "WAVWriter.h"
#include <fstream>
#include <atlbase.h>


WAVWriter::WAVWriter(const wchar_t* filename) : 
	_stream(nullptr),
	_dataSize(0)
{
	_filename = new std::wstring(filename);
}


WAVWriter::~WAVWriter()
{
	if (_stream)
	{
		CloseFile();
	}
}


template <typename T>
void writeWAVPart(std::ofstream* stream, const T& t) {
	stream->write((const char*)&t, sizeof(T));
}

void WAVWriter::OpenFile()
{
	if (PathFileExistsW(_filename->c_str()))
	{
		DeleteFileW(_filename->c_str());
	}

	_stream = new std::ofstream(_filename->c_str(), std::ios::binary);
}

void WAVWriter::WriteHeader(int sampleRate, short channels) const
{
	const int fakeBufSize = 7777;
	
	_stream->write("RIFF", 4);
	writeWAVPart<int>(_stream, 36 + fakeBufSize);
	_stream->write("WAVE", 4);
	_stream->write("fmt ", 4);
	writeWAVPart<int>(_stream, 16);
	writeWAVPart<short>(_stream, 1);                                        // Format (1 = PCM)
	writeWAVPart<short>(_stream, channels);                                 // Channels //mono/stereo
	writeWAVPart<int>(_stream, sampleRate);

	writeWAVPart<int>(_stream, sampleRate * channels * sizeof(short)); // Byterate
	writeWAVPart<short>(_stream, channels * sizeof(short));            // Frame size
	writeWAVPart<short>(_stream, 8 * sizeof(short));                   // Bits per sample
	_stream->write("data", 4);
	uint32_t sz = fakeBufSize;
	_stream->write((const char*)&sz, 4);
}

void WAVWriter::WriteChunk(short* buf, size_t bufSize)
{	
	_stream->write((const char*)buf, bufSize);
	_dataSize += bufSize;
}

void WAVWriter::CloseFile()
{
	_stream->close();
	delete _stream;

	_stream = new std::ofstream(_filename->c_str(), std::ios::binary | std::ios::out | std::ios::in);

	_stream->seekp(4, std::ios::beg);
	writeWAVPart<int>(_stream, 36 + _dataSize);

	_stream->seekp(40, std::ios::beg);
	uint32_t sz = _dataSize;
	_stream->write((const char*)&sz, 4);

	_stream->close();
	delete _stream;

	_stream = nullptr;
}
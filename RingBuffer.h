#pragma once

#include <vector>
#include <mutex>
#include <thread>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#include "VFDebug.h"
#include "MFUtils.h"


class MFRingBuffer
{
	std::vector<IMFSample *>data_;
	std::mutex mutex_;
	int size_, start_, end_;

public:
	MFRingBuffer(int size = 256)
	{
		size_ = size;
		start_ = 0;
		end_ = 0;

		data_ = std::vector<IMFSample *>(size_);
	}

	IMFSample *pop()
	{
		mutex_.lock();
		while (empty()) 
		{
			mutex_.unlock();
			return NULL;
		}

		IMFSample *sample = data_[start_];
		data_[start_] = NULL;
		start_ = (start_ + 1) % size_;
		mutex_.unlock();
		return sample;
	}

	IMFSample *refSample()
	{
		mutex_.lock();
		while (empty()) {
			mutex_.unlock();
			return NULL;
		}
		IMFSample *sample = data_[start_];
		mutex_.unlock();
		return sample;
	}

	void dropNext()
	{
		mutex_.lock();
		while (empty()) {
			mutex_.unlock();
			return;
		}
		// try to release.
		IMFSample *sample = data_[start_];
		IMFMediaBuffer *buffer = NULL;
		sample->GetBufferByIndex(0, &buffer);
		if (buffer != NULL) {
			buffer->Release();
			buffer = NULL;
		}
		if (sample != NULL) {
			sample->Release();
			sample = NULL;
		}
		data_[start_] = NULL;
		start_ = (start_ + 1) % size_;
		mutex_.unlock();
	}

	void push(IMFSample *sample)
	{
		mutex_.lock();
		if (isFull())
		{
			start_ = (start_ + 1) & size_;
			//Pipeline->TraceE(L"Dropping frame from buffer.\n");
		}

		int newEnd = (end_ + 1) % size_;
		data_[end_] = sample;
		end_ = newEnd;
		mutex_.unlock();
	}

	__inline bool isFull()
	{
		return ((end_ + 1) % size_) == start_;
	}

	__inline bool empty()
	{
		return end_ == start_;
	}

	void clear()
	{
		while (!empty())
		{
			IMFSample* sample = pop();
			if (sample != nullptr)
			{
				SafeReleaseSample(&sample);
			}
		}
	}
};



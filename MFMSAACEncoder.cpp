#include "MFMSAACEncoder.h"
#include "MFUtils.h"

#include <codecapi.h>
#include <Shlwapi.h>
#include <atlbase.h>
#include "VFDebug.h"
#include <wmcodecdsp.h>
#include <comdef.h>

MFMSAACEncoder::MFMSAACEncoder(MFPipeline* pipeline, VFAudioMediaType audioFormat, int bitrate)
	: MFFilter(pipeline)
{
	this->encodeThread = NULL;
	this->pEncoder = NULL;
	this->pInType = NULL;
	this->OutputMediaType = NULL;
	this->pEvGenerator = NULL;
	
	this->AudioFormat = audioFormat;
	this->bitrate = bitrate;
	
	_firstSample = TRUE;
	_baseTime = 0;

	this->Init();
}

MFMSAACEncoder::~MFMSAACEncoder()
{
	SafeRelease(&pEncoder);
	SafeRelease(&pInType);
	SafeRelease(&OutputMediaType);
	SafeRelease(&pEvGenerator);
}

int MFMSAACEncoder::Init()
{	
	CComPtr<IUnknown> spXferUnk;
	HRESULT hr = CoCreateInstance(CLSID_AACMFTEncoder, NULL, CLSCTX_INPROC_SERVER, IID_IUnknown, (void**)&spXferUnk);

	if (SUCCEEDED(hr))
	{
		hr = spXferUnk->QueryInterface(IID_PPV_ARGS(&pEncoder));
	}

	if (FAILED(hr))
	{
		pEncoder = NULL;

		TraceE(L"AAC Encoder: Unable to create.");
		return false;
	}

	MFT_REGISTER_TYPE_INFO out_type = { 0 };

	out_type.guidMajorType = MFMediaType_Audio;
	out_type.guidSubtype = MFAudioFormat_AAC;
	
	DWORD inMin = 0, inMax = 0, outMin = 0, outMax = 0;
	pEncoder->GetStreamLimits(&inMin, &inMax, &outMin, &outMax);

	DWORD inStreamsCount = 0, outStreamsCount = 0;
	pEncoder->GetStreamCount(&inStreamsCount, &outStreamsCount);

	DWORD * inStreams = new DWORD[inStreamsCount];
	DWORD *outStreams = new DWORD[outStreamsCount];

	hr = pEncoder->GetStreamIDs(inStreamsCount, inStreams, outStreamsCount, outStreams);

	if (hr != S_OK)
	{
		if (hr == E_NOTIMPL)
		{
			inStreams[0] = 0;
			outStreams[0] = 0;
		}
		else
		{
			TraceE(L"Unable to get MFT encoder stream IDs");
			return false;
		}
	}

	MFCreateMediaType(&OutputMediaType);
	OutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, out_type.guidMajorType);
	OutputMediaType->SetGUID(MF_MT_SUBTYPE, out_type.guidSubtype);
	OutputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, AudioFormat.BPS);
	OutputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, AudioFormat.SampleRate);
	OutputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, AudioFormat.Channels);
	OutputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (bitrate * 1000) / 8);

	//pOutType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, nAvgBytesPerSec);
	//OutputMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
	//OutputMediaType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
	//pOutType->SetUINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, TRUE);
	OutputMediaType->SetUINT32(MF_MT_COMPRESSED, TRUE);
	
	//if (out_type.guidSubtype == MFAudioFormat_AAC)
	//	pOutType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1);

	hr = pEncoder->SetOutputType(outStreams[0], OutputMediaType, 0);

	if (FAILED(hr))
	{
		TraceE(L"Audio Encoder: Failed to set encoder output type");
		return false;
	}

	GUID format;
	for (int i = 0;; i++)
	{
		hr = pEncoder->GetInputAvailableType(inStreams[0], i, &pInType);
		if (hr != S_OK) break;

		pInType->GetGUID(MF_MT_SUBTYPE, &format);
		if (format == MFAudioFormat_PCM)
		{
			break;
		}

		SafeRelease(&pInType);
	}

	if (pInType == NULL)
	{
		TraceE(L"Audio Encoder: Failed to get input type");
		return false;
	}

	//UINT32 ch = 0, sr = 0, bps = 0, ba = 0, avg = 0;
	//if (Pipeline->audcap->SourceType != nullptr)
	//{		
	//	Pipeline->audcap->SourceType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bps);
	//	Pipeline->audcap->SourceType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sr);
	//	Pipeline->audcap->SourceType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &ch);
	//	Pipeline->audcap->SourceType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &ba);
	//	Pipeline->audcap->SourceType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &avg);
	//}

	// TODO?
	//LONGLONG hnsSampleDuration = 		(nAudioSamplesPerChannel * (LONGLONG)10000000) / nSamplesPerSec;

	UINT32 blockAlignIn = AudioFormat.Channels * (AudioFormat.BPS / 8);
	UINT32 bytesPerSecondIn = blockAlignIn * AudioFormat.SampleRate;
	pInType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, AudioFormat.BPS);
	pInType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, AudioFormat.SampleRate);
	pInType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, AudioFormat.Channels);
	pInType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlignIn);
	pInType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytesPerSecondIn);
	//pInType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);

	hr = pEncoder->SetInputType(inStreams[0], pInType, 0);
	if (FAILED(hr))
	{
		TraceE(L"Audio Encoder: Failed to set input type");
		return false;
	}

	if (hr == S_OK)
	{
		Initiated = TRUE;
	}

	return SUCCEEDED(hr);
}

HRESULT MFMSAACEncoder::Start()
{
	if (!Initiated)
	{
		Finished = TRUE;
		return E_FAIL;
	}

	StopFlag = FALSE;
	encodeThread = new std::thread(&MFMSAACEncoder::ProcessData, this);
	
	return 0;
}

void MFMSAACEncoder::Join() const
{
	if (encodeThread)
	{
		encodeThread->join();
	}
}

void MFMSAACEncoder::ProcessData()
{
	HRESULT hr;

	Finished = FALSE;

	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
	pEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);

	while (!(StopFlag && Pipeline->audioCapBuffer->empty()))
	{	
		
		IMFSample* pSample = NULL;
		pSample = Pipeline->audioCapBuffer->pop();
		if (pSample == NULL)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
			continue;
		}

		LONGLONG llTimeStamp = 0;
		hr = pSample->GetSampleTime(&llTimeStamp);
		if (_firstSample)
		{
			_baseTime = llTimeStamp;
			_firstSample = FALSE;
		}

		// rebase the time stamp
		llTimeStamp -= _baseTime;

		hr = pSample->SetSampleTime(llTimeStamp);

		//TraceD(L"Push audio frame to encoder\n");

		DWORD buffersCount = 0;
		pSample->GetBufferCount(&buffersCount);

		if (buffersCount == 0)
		{
			SafeReleaseSample(&pSample);
			continue;
		}

		hr = pEncoder->ProcessInput(0, pSample, 0);
		if (FAILED(hr))
		{
			TraceE(L"Audio Encoder: Input Failed");
		}

		SafeReleaseSample(&pSample);

		while (true)
		{
			//if (*STOP_FLAG && in_buffer->empty())
			//{
			//	return;
			//}

			DWORD flags = 0;
			hr = pEncoder->GetOutputStatus(&flags);
			if (flags != MFT_OUTPUT_STATUS_SAMPLE_READY && hr != E_NOTIMPL)
			{
				break;
			}

			MFT_OUTPUT_DATA_BUFFER outDataBuffer;
			MFT_OUTPUT_STREAM_INFO info;
			IMFMediaBuffer *b;
			DWORD status;

			outDataBuffer.dwStatus = 0;
			outDataBuffer.dwStreamID = 0;
			outDataBuffer.pEvents = NULL;
			outDataBuffer.pSample = NULL;

			pEncoder->GetOutputStreamInfo(0, &info);

			MFCreateSample(&outDataBuffer.pSample);
			MFCreateMemoryBuffer(info.cbSize, &b);
			outDataBuffer.pSample->AddBuffer(b);

			hr = pEncoder->ProcessOutput(0, 1, &outDataBuffer, &status);
			if (SUCCEEDED(hr))
			{
				DWORD totalLength = 0;
				outDataBuffer.pSample->GetTotalLength(&totalLength);

				//TraceD(L"Audio Encoder: Output Processed: %d: \n" , totalLength);

				//m_Mux->WriteAudioSample(outDataBuffer.pSample, 0);

				Pipeline->audioEncBuffer->push(outDataBuffer.pSample);
				outDataBuffer.pSample->AddRef();
				//b->AddRef();
			}
			else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
			{
				SafeRelease(&outDataBuffer.pSample);
				SafeRelease(&outDataBuffer.pEvents);
				SafeRelease(&b);

				continue;
			}
			else if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
			{
				//XTrace("Audio Encoder: TRANSFORM STREAM CHANGE");
				hr = pEncoder->SetOutputType(0, OutputMediaType, 0);
				if (FAILED(hr))
				{
					TraceE(L"Audio Encoder: Failed to set output type");
				}
			}
			else
			{
				TraceE(L"Audio Encoder: Process output failed");
			}

			SafeRelease(&outDataBuffer.pSample);
			SafeRelease(&outDataBuffer.pEvents);
			SafeRelease(&b);
		}	

		if (StopFlag)
		{
			Sleep(1000);
			continue;
		}
	}

	if (!Pipeline->audioCapBuffer->empty())
	{
		ProcessData();
	}

	Finished = TRUE;
}

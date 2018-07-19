#pragma once

#include <new>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Wmcodecdsp.h>
#include <assert.h>
#include <Dbt.h>
#include <shlwapi.h>

class MFCodecList
{
	UINT32      m_cDevices;
	IMFActivate **m_ppDevices;
public:
	MFCodecList();
	~MFCodecList();

	UINT32  Count() const { return m_cDevices; }

	void    Clear();
	HRESULT Enumerate(GUID mediaType, GUID subtype, BOOL hw);

	HRESULT GetCodec(UINT32 index, IMFActivate **ppActivate) const;
	HRESULT GetCodecName(UINT32 index, WCHAR **ppszName) const;
	HRESULT PrintNames() const;
	
	HRESULT GetQSVH264Encoder(IMFActivate **ppActivate) const;

	HRESULT GetNVENCH264Encoder(IMFActivate **ppActivate) const;
	HRESULT GetNVENCH265Encoder(IMFActivate **ppActivate) const;

	HRESULT GetAMDH264Encoder(IMFActivate **ppActivate) const;
	HRESULT GetAMDH265Encoder(IMFActivate **ppActivate) const;

	BOOL IsQSVH264EncoderAvailable() const;

	BOOL IsNVENCH264EncoderAvailable() const;
	BOOL IsNVENCH265EncoderAvailable() const;
	
	BOOL IsAMDH264EncoderAvailable() const;
	BOOL IsAMDH265EncoderAvailable() const;
};


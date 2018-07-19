#include "MFCodecList.h"

#include "MFUtils.h"
#include <stdio.h>

MFCodecList::MFCodecList() :
	m_ppDevices(NULL),
	m_cDevices(0)
{
}

MFCodecList::~MFCodecList()
{
	Clear();
}

void MFCodecList::Clear()
{
	for (UINT32 i = 0; i < m_cDevices; i++)
	{
		SafeRelease(&m_ppDevices[i]);
	}

	CoTaskMemFree(m_ppDevices);
	m_ppDevices = NULL;

	m_cDevices = 0;
}

HRESULT MFCodecList::Enumerate(GUID mediaType, GUID subtype, BOOL hw)
{
	Clear();

	HRESULT hr = S_OK;
	
	MFT_REGISTER_TYPE_INFO out_type = { 0 };
	out_type.guidMajorType = mediaType;
	out_type.guidSubtype = subtype;
		
	UINT32 flags = 0;
	if (hw)
	{
		flags |= MFT_ENUM_FLAG_HARDWARE;
	}

	hr = MFTEnumEx(
		MFT_CATEGORY_VIDEO_ENCODER,
		flags,
		NULL, 
		&out_type,
		&m_ppDevices,
		&m_cDevices
	);
	
	return hr;
}

HRESULT MFCodecList::GetCodec(UINT32 index, IMFActivate **ppActivate) const
{
	if (index >= Count())
	{
		return E_INVALIDARG;
	}

	*ppActivate = m_ppDevices[index];
	(*ppActivate)->AddRef();

	return S_OK;
}

HRESULT MFCodecList::GetCodecName(UINT32 index, WCHAR **ppszName) const
{
	if (index >= Count())
	{
		return E_INVALIDARG;
	}

	HRESULT hr = S_OK;

	hr = m_ppDevices[index]->GetAllocatedString(
		MFT_FRIENDLY_NAME_Attribute,
		ppszName,
		NULL
	);

	return hr;
}

HRESULT MFCodecList::PrintNames() const
{
	HRESULT hr = S_OK;
	
	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		hr = GetCodecName(i, &name);
		wprintf(L"%s\n", name);
	}

	return hr;
}

HRESULT MFCodecList::GetQSVH264Encoder(IMFActivate **ppActivate) const
{
	*ppActivate = NULL;

	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);
		
		if (hr == S_OK && wcsstr(name, L"Intel") != nullptr && wcsstr(name, L"H.264 Encoder") != nullptr)
		{
			*ppActivate = m_ppDevices[i];
			(*ppActivate)->AddRef();

			return S_OK;
		}
	}

	return E_FAIL;
}

BOOL MFCodecList::IsQSVH264EncoderAvailable() const
{
	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"Intel") != nullptr && wcsstr(name, L"H.264 Encoder") != nullptr)
		{		
			return TRUE;
		}
	}

	return FALSE;
}

HRESULT MFCodecList::GetNVENCH264Encoder(IMFActivate **ppActivate) const
{
	*ppActivate = NULL;

	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"NVIDIA") != nullptr && wcsstr(name, L"H.264 Encoder") != nullptr)
		{
			*ppActivate = m_ppDevices[i];
			(*ppActivate)->AddRef();

			return S_OK;
		}
	}

	return E_FAIL;
}

HRESULT MFCodecList::GetNVENCH265Encoder(IMFActivate **ppActivate) const
{
	*ppActivate = NULL;

	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"NVIDIA") != nullptr && wcsstr(name, L"HEVC Encoder") != nullptr)
		{
			*ppActivate = m_ppDevices[i];
			(*ppActivate)->AddRef();

			return S_OK;
		}
	}

	return E_FAIL;
}


HRESULT  MFCodecList::GetAMDH264Encoder(IMFActivate **ppActivate) const
{
	*ppActivate = NULL;

	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"AMDh264Encoder") != nullptr)
		{
			*ppActivate = m_ppDevices[i];
			(*ppActivate)->AddRef();

			return S_OK;
		}
	}

	return E_FAIL;
}

HRESULT  MFCodecList::GetAMDH265Encoder(IMFActivate **ppActivate) const
{
	*ppActivate = NULL;

	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"AMDh265Encoder") != nullptr)
		{
			*ppActivate = m_ppDevices[i];
			(*ppActivate)->AddRef();

			return S_OK;
		}
	}

	return E_FAIL;
}

BOOL MFCodecList::IsNVENCH264EncoderAvailable() const
{
	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"NVIDIA") != nullptr && wcsstr(name, L"H.264 Encoder") != nullptr)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL MFCodecList::IsNVENCH265EncoderAvailable() const
{
	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"NVIDIA") != nullptr && wcsstr(name, L"HEVC Encoder") != nullptr)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL MFCodecList::IsAMDH264EncoderAvailable() const
{
	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"AMDh264Encoder") != nullptr)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL MFCodecList::IsAMDH265EncoderAvailable() const
{
	WCHAR* name = new WCHAR[256];
	memset(name, 0, 256);
	for (int i = 0; i < Count(); i++)
	{
		HRESULT hr = GetCodecName(i, &name);

		if (hr == S_OK && wcsstr(name, L"AMDh265Encoder") != nullptr)
		{
			return TRUE;
		}
	}

	return FALSE;
}

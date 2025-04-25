// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#include <string>

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include "Windows/AllowWindowsPlatformTypes.h"

namespace WyvrnSDK
{
	class VerifyLibrarySignature
	{
	public:
		static bool VerifyModule(const std::wstring& filename);
		static bool IsFileVersionSameOrNewer(const std::wstring& filename, const int minMajor, const int minMinor, const int minRevision, const int minBuild);
	private:
		static bool IsFileSignedByRazer(const wchar_t* szFileName);
		static bool IsFileSigned(const wchar_t* szFileName);
	};
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif

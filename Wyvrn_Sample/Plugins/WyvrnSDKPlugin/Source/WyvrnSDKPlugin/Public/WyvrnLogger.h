// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once


#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include <stdio.h>
#include "Windows/AllowWindowsPlatformTypes.h" 

namespace WyvrnSDK
{
	class WyvrnLogger
	{
	public:
		static void printf(const char* format, ...);
		static void fprintf(FILE* stream, const char* format, ...);

		static void wprintf(const wchar_t* format, ...);
		static void fwprintf(FILE* stream, const wchar_t* format, ...);
	};
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif

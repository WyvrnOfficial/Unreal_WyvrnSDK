// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "WyvrnLogger.h"

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include <stdarg.h>
#include "Windows/AllowWindowsPlatformTypes.h" 

using namespace WyvrnSDK;


void WyvrnLogger::printf(const char* format, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, format);
	::vprintf(format, args);
	va_end(args);
#endif
}

void WyvrnLogger::fprintf(FILE* stream, const char* format, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, format);
	::vfprintf(stream, format, args);
	va_end(args);
#endif
}

void WyvrnLogger::wprintf(const wchar_t* format, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, format);
	::vwprintf(format, args);
	va_end(args);
#endif
}

void WyvrnLogger::fwprintf(FILE* stream, const wchar_t* format, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, format);
	::vfwprintf(stream, format, args);
	va_end(args);
#endif
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif

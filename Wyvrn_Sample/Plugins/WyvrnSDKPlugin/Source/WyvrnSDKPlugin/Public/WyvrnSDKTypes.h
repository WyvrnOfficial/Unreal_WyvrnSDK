// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#ifndef _WYVRNSDKTYPES_H_
#define _WYVRNSDKTYPES_H_

#pragma once

typedef long RZRESULT;

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include "Windows/AllowWindowsPlatformTypes.h" 

namespace WyvrnSDK
{
     typedef struct APPINFOTYPE
     {
         wchar_t Title[256];           
         wchar_t Description[1024];
         struct Author
         {
             wchar_t Name[256];
             wchar_t Contact[256];
         } Author;
         const DWORD SupportedDevice = 63;
         DWORD Category;             
     } APPINFOTYPE;
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif

#endif

// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include "Logging/LogMacros.h"
DECLARE_LOG_CATEGORY_EXTERN(LogWyvrnAnimationAPI, Log, All);

#include "WyvrnSDKTypes.h"

#include "Windows/AllowWindowsPlatformTypes.h" 

namespace WyvrnSDK
{
	/* Setup log mechanism */
	typedef void (*DebugLogPtr)(const wchar_t*);
	void LogDebug(const wchar_t *text, ...);
	void LogError(const wchar_t *text, ...);
	/* End of setup log mechanism */

	class WyvrnAPI
	{
	public:

                
#pragma region API declare prototypes
		/*
			Direct access to low level API.
		*/
		static RZRESULT CoreInitSDK(WyvrnSDK::APPINFOTYPE* AppInfo);
		/*
			Direct access to low level API.
		*/
		static RZRESULT CoreSetEventName(const wchar_t* Name);
		/*
			Direct access to low level API.
		*/
		static RZRESULT CoreUnInit();
#pragma endregion


		static int InitAPI();
		static int UninitAPI();
		static bool GetIsInitializedAPI();
		static bool _sIsInitializedAPI;
		static bool _sInitialized;
	};
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif

// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#if PLATFORM_WINDOWS || (defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)

#include "WyvrnAPI.h"
#include "WyvrnLogger.h"
#if !(defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
#include "VerifyLibrarySignature.h"
#endif
#include "WyvrnErrors.h"
#include <iostream>
#include <tchar.h>
#include "Interfaces/IPluginManager.h"
#include <Misc/Paths.h>


DEFINE_LOG_CATEGORY(LogWyvrnAPI);


#if defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE
#define WYVRN_EDITOR_DLL	L"WyvrnSDK64.dll"
#else

#ifdef _WIN64
#define WYVRN_EDITOR_DLL	L"WyvrnSDK64.dll"
#else
#define WYVRN_EDITOR_DLL	L"WyvrnSDK.dll"
#endif


#endif


using namespace WyvrnSDK;
using namespace std;

HMODULE WyvrnAPI::_sLibrary = nullptr;
bool WyvrnAPI::_sInvalidSignature = false;
bool WyvrnAPI::_sIsInitializedAPI = false;

#define WYVRNSDK_DECLARE_METHOD_IMPL(Signature, FieldName) Signature WyvrnAPI::FieldName = nullptr;

#pragma region API declare assignments
WYVRNSDK_DECLARE_METHOD_IMPL(PLUGIN_CORE_INIT_SDK, CoreInitSDK);
WYVRNSDK_DECLARE_METHOD_IMPL(PLUGIN_CORE_SET_EVENT_NAME, CoreSetEventName);
WYVRNSDK_DECLARE_METHOD_IMPL(PLUGIN_CORE_UNINIT, CoreUnInit);
#pragma endregion

#define WYVRNSDK_VALIDATE_METHOD(Signature, FieldName) FieldName = reinterpret_cast<Signature>(reinterpret_cast<void*>(GetProcAddress(library, "Plugin" #FieldName))); \
if (FieldName == nullptr) \
{ \
	cerr << "Failed to find method: " << ("Plugin" #FieldName) << endl; \
    return -1; \
}

int WyvrnAPI::InitAPI()
{
	// abort load if an invalid signature was detected
	if (_sInvalidSignature)
	{
		return RZRESULT_DLL_INVALID_SIGNATURE;
	}

	if (_sIsInitializedAPI)
	{
		return 0;
	}

	std::wstring path;

#if defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE
	path = WYVRN_EDITOR_DLL;
#else

	
#ifdef _WIN64
	FString PluginDirectory = IPluginManager::Get().FindPlugin(TEXT("WyvrnSDKPlugin"))->GetBaseDir();
	PluginDirectory = PluginDirectory.Replace(TEXT("/"), TEXT("\\"));
	path = TCHAR_TO_WCHAR(*PluginDirectory);
	path += L"\\Binaries\\Win64\\";
	path += WYVRN_EDITOR_DLL;
#else
	FString PluginDirectory = IPluginManager::Get().FindPlugin(TEXT("WyvrnSDKPlugin"))->GetBaseDir();
	PluginDirectory = PluginDirectory.Replace(TEXT("/"), TEXT("\\"));
	path = TCHAR_TO_WCHAR(*PluginDirectory);
	path += L"\\Binaries\\Win32\\";
	path += WYVRN_EDITOR_DLL;
#endif

	// check the library file version
	if (!VerifyLibrarySignature::IsFileVersionSameOrNewer(path.c_str(), 2, 0, 1, 6))
	{
		WyvrnLogger::fprintf(stderr, "Detected old version of Wyvrn SDK!\r\n");
		return RZRESULT_DLL_NOT_FOUND;
	}

#ifdef CHECK_WYVRN_LIBRARY_SIGNATURE
	// verify the library has a valid signature
	//_sInvalidSignature = !VerifyLibrarySignature::VerifyModule(path);
#endif

	if (_sInvalidSignature)
	{
		//Expected scenario: Debug builds might not be signed
		//WyvrnLogger::fprintf(stderr, "Wyvrn Editor Library has an invalid signature!\r\n");
		return RZRESULT_DLL_INVALID_SIGNATURE;
	}

#endif

#if defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE
	UE_LOG(LogWyvrnAPI, Log, TEXT("Load WyvrnSDK64 at: %s"), *FString(path.c_str()));
#endif

	HMODULE library = LoadLibrary(path.c_str());
	if (library == NULL)
	{ 
		//Expected scenario: When Wyvrn SDK is not installed or out of date
		//UE_LOG(LogWyvrnAPI, Error, TEXT("Failed to load Wyvrn SDK!"));
		//WyvrnLogger::fprintf(stderr, "Failed to load Wyvrn SDK!\r\n");
        return RZRESULT_DLL_NOT_FOUND;
	}

	_sLibrary = library;
	
	//WyvrnLogger::fprintf(stderr, "Loaded Wyvrn SDK DLL!\r\n");
	//UE_LOG(LogWyvrnAPI, Log, TEXT("Loaded Wyvrn SDK DLL!"));	

#pragma region API validation
WYVRNSDK_VALIDATE_METHOD(PLUGIN_CORE_INIT_SDK, CoreInitSDK);
WYVRNSDK_VALIDATE_METHOD(PLUGIN_CORE_SET_EVENT_NAME, CoreSetEventName);
WYVRNSDK_VALIDATE_METHOD(PLUGIN_CORE_UNINIT, CoreUnInit);
#pragma endregion

	//WyvrnLogger::printf(stdout, "Validated all DLL methods [success]\r\n");
	//UE_LOG(LogWyvrnAPI, Log, TEXT("Validated all DLL methods [success]"));
	_sIsInitializedAPI = true;
	return 0;
}

bool WyvrnAPI::GetIsInitializedAPI()
{
	return _sIsInitializedAPI;
}

#undef WYVRNSDK_DECLARE_METHOD_CLEAR
#define WYVRNSDK_DECLARE_METHOD_CLEAR(FieldName) WyvrnAPI::FieldName = nullptr;

int WyvrnAPI::UninitAPI()
{
	if (nullptr != _sLibrary)
	{
		FreeLibrary(_sLibrary);
		_sLibrary = nullptr;
	}

#pragma region Free API Methods

	WYVRNSDK_DECLARE_METHOD_CLEAR(CoreInitSDK);
	WYVRNSDK_DECLARE_METHOD_CLEAR(CoreSetEventName);
	WYVRNSDK_DECLARE_METHOD_CLEAR(CoreUnInit);

#pragma endregion

	_sIsInitializedAPI = false;
	
	return 0;
}

#endif

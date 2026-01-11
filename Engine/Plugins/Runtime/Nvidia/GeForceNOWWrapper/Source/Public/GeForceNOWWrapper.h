// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if NV_GEFORCENOW

#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

THIRD_PARTY_INCLUDES_START
#include "GfnRuntimeSdk_CAPI.h"
THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(LogGFNWrapper, Log, All);

class GeForceNOWActionZoneProcessor;

/**
 * Singleton wrapper to manage the GeForceNow SDK
 */
class GeForceNOWWrapper
{
public:
	/** Load and Initialize the GeforceNOW SDK dll. */
	static GEFORCENOWWRAPPER_API GfnRuntimeError Initialize();
	
	/** Unload the GeforceNOW SDK dlls. */
	static GEFORCENOWWRAPPER_API GfnRuntimeError Shutdown();

	/*
	* Use to request that the client application open a URL link in their local web browser
	* Returns true if the client successfully opened the URL
	* If the client fails to open the URL an error code is provided
	*/
	static GEFORCENOWWRAPPER_API bool OpenURLOnClient(const TCHAR* URL, int32& OutErrorCode);
	
	static GEFORCENOWWRAPPER_API GeForceNOWWrapper& Get();	

	static GEFORCENOWWRAPPER_API const FString GetGfnOsTypeString(GfnOsType OsType);

	/** Returns true if the GeForceNow SDK is initialized and running in cloud. */
	static GEFORCENOWWRAPPER_API bool IsRunningInGFN();

	/** Returns true for mock, but this can be used to differentiate between real and mock. */
	static GEFORCENOWWRAPPER_API bool IsRunningMockGFN();

	/** Initializes the Action Zone Processor.Returns true if the initialization was a success. */
	GEFORCENOWWRAPPER_API bool InitializeActionZoneProcessor();

	/** Determines if application is running in GeforceNOW environment  and without requiring process elevation. */
	static GEFORCENOWWRAPPER_API bool IsRunningInCloud();

	/** Notify GeforceNOW that an application should be readied for launch. */
	GEFORCENOWWRAPPER_API GfnRuntimeError SetupTitle(const FString& InPlatformAppId) const;

	/** Notify GeForceNOW that an application is ready to be displayed. */
	GEFORCENOWWRAPPER_API GfnRuntimeError NotifyAppReady(bool bSuccess, const FString& InStatus) const;
	/** Notify GeforceNOW that an application has exited. */
	GEFORCENOWWRAPPER_API GfnRuntimeError NotifyTitleExited(const FString& InPlatformId, const FString& InPlatformAppId) const;

	/** Request GeforceNOW client to start a streaming session of an application in a synchronous (blocking) fashion. */
	GEFORCENOWWRAPPER_API GfnRuntimeError StartStream(StartStreamInput& InStartStreamInput, StartStreamResponse& OutResponse) const;
	/** Request GeforceNOW client to start a streaming session of an application in an asynchronous fashion. */
	GEFORCENOWWRAPPER_API GfnRuntimeError StartStreamAsync(const StartStreamInput& InStartStreamInput, StartStreamCallbackSig StartStreamCallback, void* Context, uint32 TimeoutMs) const;

	/** Request GeforceNOW client to stop a streaming session of an application in a synchronous (blocking) fashion. */
	GEFORCENOWWRAPPER_API GfnRuntimeError StopStream() const;
	/** Request GeforceNOW client to stop a streaming session of an application in an asynchronous fashion. */
	GEFORCENOWWRAPPER_API GfnRuntimeError StopStreamAsync(StopStreamCallbackSig StopStreamCallback, void* Context, unsigned int TimeoutMs) const;

	/** Use to invoke special events on the client from the GFN cloud environment */
	GEFORCENOWWRAPPER_API GfnRuntimeError SetActionZone(GfnActionType ActionType, unsigned int Id, GfnRect* Zone);

	/** Registers a callback that gets called on the user's PC when the streaming session state changes. */
	GEFORCENOWWRAPPER_API GfnRuntimeError RegisterStreamStatusCallback(StreamStatusCallbackSig StreamStatusCallback, void* Context) const;
	/** Registers an application function to call when GeforceNOW needs to exit the game. */
	GEFORCENOWWRAPPER_API GfnRuntimeError RegisterExitCallback(ExitCallbackSig ExitCallback, void* Context) const;
	/** Registers an application callback with GeforceNOW to be called when GeforceNOW needs to pause the game on the user's behalf. */
	GEFORCENOWWRAPPER_API GfnRuntimeError RegisterPauseCallback(PauseCallbackSig PauseCallback, void* Context) const;
	/** Registers an application callback with GeforceNOW to be called after a successful call to SetupTitle. */
	GEFORCENOWWRAPPER_API GfnRuntimeError RegisterInstallCallback(InstallCallbackSig InstallCallback, void* Context) const;
	/** Registers an application callback with GeforceNOW to be called when GeforceNOW needs the application to save user progress. */
	GEFORCENOWWRAPPER_API GfnRuntimeError RegisterSaveCallback(SaveCallbackSig SaveCallback, void* Context) const;
	/** Registers an application callback to be called when a GeforceNOW user has connected to the game seat. */
	GEFORCENOWWRAPPER_API GfnRuntimeError RegisterSessionInitCallback(SessionInitCallbackSig SessionInitCallback, void* Context) const;
	/** Registers an application callback with GFN to be called when client info changes. */
	GEFORCENOWWRAPPER_API GfnRuntimeError RegisterClientInfoCallback(ClientInfoCallbackSig ClientInfoCallback, void* Context) const;

	/** Gets user client's IP address. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetClientIpV4(FString& OutIpv4) const;
	/** Gets user's client language code in the form "<lang>-<country>" using a standard ISO 639-1 language code and ISO 3166-1 Alpha-2 country code. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetClientLanguageCode(FString& OutLanguageCode) const;
	/** Gets user’s client country code using ISO 3166-1 Alpha-2 country code. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetClientCountryCode(FString& OutCountryCode) const;
	/** Gets user's client data. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetClientInfo(GfnClientInfo& OutClientInfo) const;
	/** Gets user's session data. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetSessionInfo(GfnSessionInfo& OutSessionInfo) const;
	/** Retrieves secure partner data that is either a) passed by the client in the gfnStartStream call or b) sent in response to Deep Link nonce validation. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetPartnerData(FString& OutPartnerData) const;
	/** Use during cloud session to retrieve secure partner data. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetPartnerSecureData(FString& OutPartnerSecureData) const;
	/** Retrieves all titles that can be launched in the current game streaming session. */
	GEFORCENOWWRAPPER_API GfnRuntimeError GetTitlesAvailable(FString& OutAvailableTitles) const;

	/** Determines if calling application is running in GeforceNOW environment, and what level of security assurance that the result is valid. */
	GEFORCENOWWRAPPER_API GfnRuntimeError IsRunningInCloudSecure(GfnIsRunningInCloudAssurance& OutAssurance) const;
	/** Determines if a specific title is available to launch in current streaming session. */
	GEFORCENOWWRAPPER_API GfnRuntimeError IsTitleAvailable(const FString& InTitleID, bool& OutbIsAvailable) const;
	
	/** Returns true is the GeforceNOW SDK dll was loaded and initialized. */
	static bool IsSdkInitialized() { return bIsSdkInitialized; }

private:

	/** Singleton access only. */
	GeForceNOWWrapper() = default;
	~GeForceNOWWrapper() = default;

	GEFORCENOWWRAPPER_API void HandleLaunchURL(const TCHAR* URL);

	/** Free memory allocated by gfnGetTitlesAvailable and the likes. */
	GEFORCENOWWRAPPER_API GfnRuntimeError Free(const char** data) const;

	/** Is the DLL loaded and GfnInitializeSdk was called and succeeded. */
	static GEFORCENOWWRAPPER_API bool bIsSdkInitialized;

	/** Is the DLL running in the GeForce Now environment. */
	static GEFORCENOWWRAPPER_API TOptional<bool> bIsRunningInCloud;

	/** Keeps track of actions zones for GeForce NOW. Action Zones are used for things like keyboard invocation within the GeForce NOW app.*/
	TSharedPtr<GeForceNOWActionZoneProcessor> ActionZoneProcessor;

	static GEFORCENOWWRAPPER_API GeForceNOWWrapper* Singleton;
};

#endif // NV_GEFORCENOW

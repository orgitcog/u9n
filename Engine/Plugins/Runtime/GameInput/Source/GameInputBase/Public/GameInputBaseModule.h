// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"		// For TMulticastDelegate
#include "HAL/CriticalSection.h"
#include "Modules/ModuleInterface.h"

#define UE_API GAMEINPUTBASE_API

#if GAME_INPUT_SUPPORT
struct IGameInput;
#endif

class FGameInputBaseModule : public IModuleInterface
{
public:

	static UE_API FGameInputBaseModule& Get();

	/** Returns true if this module is loaded (aka available) by the FModuleManager */
	static UE_API bool IsAvailable();

	//~ Begin IModuleInterface interface
	UE_API virtual void StartupModule() override;
	UE_API virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

#if GAME_INPUT_SUPPORT
	/** 
	* Pointer to the static IGameInput that is created upon module startup.
	*/
	static UE_API IGameInput* GetGameInput();

	/**
	 * Delegate which is called after the creation of the IGameInput object from the game input library.
	 */
	TMulticastDelegate<void(IGameInput*)> OnGameInputCreation;
#endif

protected:

	UE_API void InitializeGameInputKeys();

#if PLATFORM_WINDOWS && GAME_INPUT_SUPPORT
	// Handle to the game input dll which is set on StartupModule.
	// If we can't find the DLL then we will early exit and not attempt to initalize GameInput.
	void* GameInputDLLHandle = nullptr;

	static UE_API FCriticalSection GameInputCreationLock;
	
#endif // endif PLATFORM_WINDOWS && GAME_INPUT_SUPPORT
};

#undef UE_API

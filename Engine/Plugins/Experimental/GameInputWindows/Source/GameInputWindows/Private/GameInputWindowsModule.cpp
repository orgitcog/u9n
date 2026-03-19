// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameInputWindowsModule.h"

#include "CoreGlobals.h"
#include "GameInputWindowsDevice.h"
#include "GameInputBaseModule.h"
#include "GameInputLogging.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"

#if GAME_INPUT_SUPPORT
THIRD_PARTY_INCLUDES_START
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <GameInput.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#endif	// GAME_INPUT_SUPPORT

namespace UE::GameInput
{
	/** The name of this modular feature plugin. */
	static const FName GameInputWindowsFeatureName = TEXT("GameInputWindows");
}

FGameInputWindowsModule& FGameInputWindowsModule::Get()
{
	return FModuleManager::LoadModuleChecked<FGameInputWindowsModule>(UE::GameInput::GameInputWindowsFeatureName);
}

bool FGameInputWindowsModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(UE::GameInput::GameInputWindowsFeatureName);
}

TSharedPtr<IInputDevice> FGameInputWindowsModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
#if GAME_INPUT_SUPPORT
	IGameInput* BaseGameInputPtr = FGameInputBaseModule::GetGameInput();
	TSharedPtr<FGameInputWindowsInputDevice> DevicePtr = MakeShared<FGameInputWindowsInputDevice>(InMessageHandler, BaseGameInputPtr);
	
	if (BaseGameInputPtr)
	{
		DevicePtr->Initialize();
	}
	else
	{
		FGameInputBaseModule& BaseModule = FGameInputBaseModule::Get();
		BaseModule.OnGameInputCreation.AddLambda([&DevicePtr](IGameInput* GameInputPtr)
		{
			if (ensure(DevicePtr.IsValid()))
			{
				DevicePtr->SetGameInputAndReinitialize(GameInputPtr);
			}
		});	
	}
	return DevicePtr;
	
#else
	UE_LOG(LogGameInput, Error, TEXT("[FGameInputWindowsModule] Failed to create a GameInput device! GAME_INPUT_SUPPORT is false! See GameInputBase.build.cs and the build log for more info."));
	return nullptr;
#endif	// GAME_INPUT_SUPPORT
}

IMPLEMENT_MODULE(FGameInputWindowsModule, GameInputWindows)
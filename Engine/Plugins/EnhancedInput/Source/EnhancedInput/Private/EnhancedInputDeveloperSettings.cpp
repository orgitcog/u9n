// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputDeveloperSettings.h"
#include "EnhancedInputPlatformSettings.h"
#include "EnhancedPlayerInput.h"
#include "HAL/IConsoleManager.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputDeveloperSettings)

UEnhancedInputDeveloperSettings::UEnhancedInputDeveloperSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, UserSettingsClass(UEnhancedInputUserSettings::StaticClass())
	, DefaultPlayerMappableKeyProfileClass(UEnhancedPlayerMappableKeyProfile::StaticClass())
	, DefaultWorldInputClass(UEnhancedPlayerInput::StaticClass())
	, bSendTriggeredEventsWhenInputIsFlushed(true)
	, bEnableUserSettings(false)
	, bEnableDefaultMappingContexts(true)
	, bShouldOnlyTriggerLastActionInChord(true)
	, bEnableInputModeFiltering(true)
	, bEnableWorldSubsystem(false)
{
	PlatformSettings.Initialize(UEnhancedInputPlatformSettings::StaticClass());
}
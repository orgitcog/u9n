// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "AnimatorKitSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta = (Experimental, DisplayName = "Animator Kit Settings"))
class ANIMATORKITSETTINGS_API UAnimatorKitSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(config, EditAnywhere, Category = "Animation Settings|Focus", meta=(ConsoleVariable="AnimMode.PendingFocusMode"))
	bool bEnableFocusMode = true;
    
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdateSettings,  const UAnimatorKitSettings*);
	static FOnUpdateSettings OnSettingsChange;

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "AnimationAuthoringSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta = (DisplayName = "Animation Authoring Settings"))
class CONTROLRIGEDITOR_API UAnimationAuthoringSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

	// Whether controls' keyframes should be added on release only.
	UPROPERTY(config, EditAnywhere, Category = Interaction)
	bool bAutoKeyOnRelease = false;
    
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdateSettings,  const UAnimationAuthoringSettings*);
	static FOnUpdateSettings OnSettingsChange;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
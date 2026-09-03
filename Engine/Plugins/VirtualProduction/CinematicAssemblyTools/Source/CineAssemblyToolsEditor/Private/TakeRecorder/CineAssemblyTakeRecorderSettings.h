// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "CineAssemblySchema.h"

#include "CineAssemblyTakeRecorderSettings.generated.h"

UCLASS(Config=EditorPerProjectUserSettings, DisplayName = "Take Recorder")
class UCineAssemblyTakeRecorderSettings : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

public:
	/** The Cine Assembly Schema to use as the base for the recorded Assembly */
	UPROPERTY(Config, EditAnywhere, Category = "Cine Assembly")
	TSoftObjectPtr<UCineAssemblySchema> AssemblySchema;
};

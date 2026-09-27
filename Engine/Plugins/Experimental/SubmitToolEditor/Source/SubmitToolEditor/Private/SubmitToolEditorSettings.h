// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "SubmitToolEditorSettings.generated.h"

struct FPropertyChangedEvent;

/** Settings for the submit tool in the editor */

UCLASS(config=Editor, meta = (DisplayName = "Submit Tool Settings"))
class USubmitToolEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USubmitToolEditorSettings();
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FName GetCategoryName() const override;

	UPROPERTY(config, Category = "Submit Tool", EditDefaultsOnly)
	FString SubmitToolPath;

	UPROPERTY(config, Category = "Submit Tool", EditDefaultsOnly)
	FString SubmitToolArguments;

	UPROPERTY(config, Category = "Submit Tool", EditDefaultsOnly)
	bool bSubmitToolEnabled;

	UPROPERTY(config, Category = "Submit Tool", EditDefaultsOnly)
	bool bEnforceDataValidation;
};


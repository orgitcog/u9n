// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimDetailsSettings.generated.h"

/** Settings for Anim Details */
UCLASS(config = EditorPerProjectUserSettings)
class UAnimDetailsSettings
	: public UObject
{
	GENERATED_BODY()

public:	
	/** The number of fractional digits displayed in anim details */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	uint8 NumFractionalDigits = 2;

	/** If true, selects a range when the left mouse button is down */
	UPROPERTY(config, EditAnywhere, Category = "Animation Settings")
	bool bLMBSelectsRange = true;
};

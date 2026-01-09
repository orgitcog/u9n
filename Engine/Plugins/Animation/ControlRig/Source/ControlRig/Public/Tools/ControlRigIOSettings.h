// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigIOSettings.generated.h"

/** Struct defining the settings to override when driving a control rig */
USTRUCT()
struct FControlRigIOSettings
{
	GENERATED_BODY()

	FControlRigIOSettings()
		: bUpdatePose(true)
		, bUpdateCurves(true)
	{}

	static FControlRigIOSettings MakeEnabled()
	{
		return FControlRigIOSettings();
	}

	static FControlRigIOSettings MakeDisabled()
	{
		FControlRigIOSettings Settings;
		Settings.bUpdatePose = Settings.bUpdateCurves = false;
		return Settings;
	}

	UPROPERTY()
	bool bUpdatePose;

	UPROPERTY()
	bool bUpdateCurves;
};

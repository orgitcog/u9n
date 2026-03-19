// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/ScaleSourceOp.h"

const UClass* FIKRetargetScaleSourceOpSettings::GetControllerType() const
{
	return UIKRetargetScaleSourceController::StaticClass();
}

void FIKRetargetScaleSourceOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	// copy ALL properties
	static TArray<FName> PropertiesToIgnore;
	FIKRetargetOpBase::CopyStructProperties(
		FIKRetargetScaleSourceOpSettings::StaticStruct(),
		InSettingsToCopyFrom,
		this,
		PropertiesToIgnore);
}

bool FIKRetargetScaleSourceOp::Initialize(
	const FIKRetargetProcessor& InProcessor,
	const FRetargetSkeleton& InSourceSkeleton,
	const FTargetSkeleton& InTargetSkeleton,
	const FIKRetargetOpBase* InParentOp,
	FIKRigLogger& Log)
{
	bIsInitialized = true;
	return true;
}

FIKRetargetOpSettingsBase* FIKRetargetScaleSourceOp::GetSettings()
{
	return &Settings;
}

const UScriptStruct* FIKRetargetScaleSourceOp::GetSettingsType() const
{
	return FIKRetargetScaleSourceOpSettings::StaticStruct();
}

const UScriptStruct* FIKRetargetScaleSourceOp::GetType() const
{
	return FIKRetargetScaleSourceOp::StaticStruct();
}

FIKRetargetScaleSourceOpSettings UIKRetargetScaleSourceController::GetSettings()
{
	return *reinterpret_cast<FIKRetargetScaleSourceOpSettings*>(OpSettingsToControl);
}

void UIKRetargetScaleSourceController::SetSettings(FIKRetargetScaleSourceOpSettings InSettings)
{
	OpSettingsToControl->CopySettingsAtRuntime(&InSettings);
}

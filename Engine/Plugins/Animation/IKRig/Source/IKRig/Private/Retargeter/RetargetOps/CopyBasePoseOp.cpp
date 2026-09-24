// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/CopyBasePoseOp.h"

const UClass* FIKRetargetCopyBasePoseOpSettings::GetControllerType() const
{
	return UIKRetargetCopyBasePoseController::StaticClass();
}

void FIKRetargetCopyBasePoseOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	// copies all properties
	const TArray<FName> PropertiesToIgnore = {};
	FIKRetargetOpBase::CopyStructProperties(
		FIKRetargetCopyBasePoseOpSettings::StaticStruct(),
		InSettingsToCopyFrom,
		this,
		PropertiesToIgnore);
}

bool FIKRetargetCopyBasePoseOp::Initialize(
	const FIKRetargetProcessor& InProcessor,
	const FRetargetSkeleton& InSourceSkeleton,
	const FTargetSkeleton& InTargetSkeleton,
	const FIKRetargetOpBase* InParentOp,
	FIKRigLogger& Log)
{
	bIsInitialized = true;
	return true;
}

FIKRetargetOpSettingsBase* FIKRetargetCopyBasePoseOp::GetSettings()
{
	return &Settings;
}

const UScriptStruct* FIKRetargetCopyBasePoseOp::GetSettingsType() const
{
	return FIKRetargetCopyBasePoseOpSettings::StaticStruct();
}

const UScriptStruct* FIKRetargetCopyBasePoseOp::GetType() const
{
	return FIKRetargetCopyBasePoseOp::StaticStruct();
}

FIKRetargetCopyBasePoseOpSettings UIKRetargetCopyBasePoseController::GetSettings()
{
	return *reinterpret_cast<FIKRetargetCopyBasePoseOpSettings*>(OpSettingsToControl);
}

void UIKRetargetCopyBasePoseController::SetSettings(FIKRetargetCopyBasePoseOpSettings InSettings)
{
	OpSettingsToControl->CopySettingsAtRuntime(&InSettings);
}

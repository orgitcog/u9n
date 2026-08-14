// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/CurveRemapOp.h"

#include "Animation/AnimCurveUtils.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimNodeBase.h"

const UClass* FIKRetargetCurveRemapOpSettings::GetControllerType() const
{
	return UIKRetargetCurveRemapController::StaticClass();
}

void FIKRetargetCurveRemapOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	// copies all properties
	const TArray<FName> PropertiesToIgnore = {};
	FIKRetargetOpBase::CopyStructProperties(
		FIKRetargetCurveRemapOpSettings::StaticStruct(),
		InSettingsToCopyFrom,
		this,
		PropertiesToIgnore);
}

void FIKRetargetCurveRemapOp::AnimGraphPreUpdateMainThread(
	USkeletalMeshComponent& SourceMeshComponent,
    USkeletalMeshComponent& TargetMeshComponent)
{
	if (!IsEnabled())
	{
		return;
	}
	
	SourceCurves.Empty();
	
	// get the source curves out of the source anim instance
	const UAnimInstance* SourceAnimInstance = SourceMeshComponent.GetAnimInstance();
	if (!SourceAnimInstance)
	{
		return;
	}
	
	// Potential optimization/tradeoff: If we stored the curve results on the mesh component in non-editor scenarios, this would be
	// much faster (but take more memory). As it is, we need to translate the map stored on the anim instance.
	const TMap<FName, float>& AnimCurveList = SourceAnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve);
	UE::Anim::FCurveUtils::BuildUnsorted(SourceCurves, AnimCurveList);
}

void FIKRetargetCurveRemapOp::AnimGraphEvaluateAnyThread(FPoseContext& Output)
{
	if (!IsEnabled())
	{
		return;
	}
	
	FBlendedCurve& OutputCurves = Output.Curve;

	// copy curves over with same name (if it exists)
	if (Settings.bCopyAllSourceCurves)
	{
		OutputCurves.CopyFrom(SourceCurves);
	}

	// copy curves over with different names (remap)
	if (Settings.bRemapCurves)
	{
		FBlendedCurve RemapedCurves;
		for (const FCurveRemapPair& CurveToRemap : Settings.CurvesToRemap)
		{
			bool bOutIsValid = false;
			const float SourceValue = SourceCurves.Get(CurveToRemap.SourceCurve, bOutIsValid);
			if (bOutIsValid)
			{
				RemapedCurves.Add(CurveToRemap.TargetCurve, SourceValue);
			}
		}
		
		OutputCurves.Combine(RemapedCurves);
	}
}

FIKRetargetCurveRemapOpSettings UIKRetargetCurveRemapController::GetSettings()
{
	return *reinterpret_cast<FIKRetargetCurveRemapOpSettings*>(OpSettingsToControl);
}

void UIKRetargetCurveRemapController::SetSettings(FIKRetargetCurveRemapOpSettings InSettings)
{
	OpSettingsToControl->CopySettingsAtRuntime(&InSettings);
}

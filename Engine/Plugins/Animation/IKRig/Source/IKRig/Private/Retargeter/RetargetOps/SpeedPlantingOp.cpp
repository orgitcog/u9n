// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/SpeedPlantingOp.h"

#include "Retargeter/IKRetargetProcessor.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimNodeBase.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"

#define LOCTEXT_NAMESPACE "SpeedPlantingOp"

const UClass* FIKRetargetSpeedPlantingOpSettings::GetControllerType() const
{
	return UIKRetargetSpeedPlantingController::StaticClass();
}

void FIKRetargetSpeedPlantingOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	// copies everything except "ChainsToSpeedPlant"
	static TArray<FName> PropertiesToIgnore = {"ChainsToSpeedPlant"};
	FIKRetargetOpBase::CopyStructProperties(
		FIKRetargetSpeedPlantingOpSettings::StaticStruct(),
		InSettingsToCopyFrom,
		this,
		PropertiesToIgnore);
	
	// copy settings only for chains that the op has initialized
	const FIKRetargetSpeedPlantingOpSettings* NewSettings = reinterpret_cast<const FIKRetargetSpeedPlantingOpSettings*>(InSettingsToCopyFrom);
	for (const FRetargetSpeedPlantingSettings& NewChainSettings : NewSettings->ChainsToSpeedPlant)
	{
		for (FRetargetSpeedPlantingSettings& ChainSettings : ChainsToSpeedPlant)
		{
			if (ChainSettings.TargetChainName == NewChainSettings.TargetChainName)
			{
				ChainSettings = NewChainSettings;
			}
		}
	}
}

bool FIKRetargetSpeedPlantingOp::Initialize(
	const FIKRetargetProcessor& InProcessor,
	const FRetargetSkeleton& InSourceSkeleton,
	const FTargetSkeleton& InTargetSkeleton,
	const FIKRetargetOpBase* InParentOp,
	FIKRigLogger& InLog)
{
	bIsInitialized = false;
	
	// this op requires a parent to supply an IK Rig
	if (!ensure(InParentOp))
	{
		return false;
	}

	// validate that an IK rig has been assigned
	const FIKRetargetRunIKRigOp* ParentOp = reinterpret_cast<const FIKRetargetRunIKRigOp*>(InParentOp);
	if (ParentOp->Settings.IKRigAsset == nullptr)
	{
		InLog.LogWarning( FText::Format(
		LOCTEXT("MissingIKRig", "{0}, is missing an IK rig. No chains can be retargeted. "), FText::FromName(GetName())));
		return false;
	}

	GoalsToPlant.Reset();

	const FRetargeterBoneChains& BoneChains = InProcessor.GetBoneChains();
	const FIKRigGoalContainer& GoalContainer = InProcessor.GetIKRigGoalContainer();
	for (const FRetargetSpeedPlantingSettings& SettingsForChain : Settings.ChainsToSpeedPlant)
	{
		const FName TargetChainName = SettingsForChain.TargetChainName;
		const FResolvedBoneChain* TargetBoneChain = BoneChains.GetResolvedBoneChainByName(
			TargetChainName,
			ERetargetSourceOrTarget::Target,
			ParentOp->Settings.IKRigAsset);
		if (TargetBoneChain == nullptr)
		{
			InLog.LogWarning( FText::Format(
			LOCTEXT("SpeedPlantingMissingChain", "Speed Planting Op: chain data is out of sync with IK Rig. Missing target chain, '{0}."),
			FText::FromName(TargetChainName)));
			continue;
		}

		if (TargetBoneChain->IKGoalName == NAME_None)
		{
			InLog.LogWarning( FText::Format(
			LOCTEXT("SpeedPlantingChainWithoutGoal", "Speed Planting Op: specified chain does not have an IK goal. Cannot speed plant, '{0}."),
			FText::FromName(TargetChainName)));
			continue;
		}

		const FIKRigGoal* Goal = GoalContainer.FindGoalByName(TargetBoneChain->IKGoalName);
		if (Goal == nullptr)
		{
			InLog.LogWarning( FText::Format(
			LOCTEXT("SpeedPlantingRigWithoutGoal", "Speed Planting Op: target chain references a goal that is not present in the IK Rig, '{0}."),
			FText::FromName(TargetBoneChain->IKGoalName)));
			continue;
		}
		
		GoalsToPlant.Emplace(TargetBoneChain->IKGoalName, &SettingsForChain, Goal->Position);
	}

	bIsInitialized = !GoalsToPlant.IsEmpty();
	return bIsInitialized;
}

void FIKRetargetSpeedPlantingOp::Run(
	FIKRetargetProcessor& InProcessor,
	const double InDeltaTime,
	const TArray<FTransform>& InSourceGlobalPose,
	TArray<FTransform>& OutTargetGlobalPose)
{
	if (InProcessor.IsIKForcedOff())
	{
		return; // skip this op when IK is off
	}
	
	FIKRigGoalContainer& GoalContainer = InProcessor.GetIKRigGoalContainer();
	
	for (FSpeedPlantingGoalState& GoalToPlant : GoalsToPlant)
	{
		FIKRigGoal* IKRigGoal = GoalContainer.FindGoalByName(GoalToPlant.GoalName);
		if (!IKRigGoal)
		{
			continue; // goal excluded, just ignore it
		}
		
		if (GoalToPlant.CurrentSpeedValue < 0.0f || GoalToPlant.CurrentSpeedValue > Settings.SpeedThreshold)
		{
			GoalToPlant.PrevGoalPosition = IKRigGoal->Position;
			GoalToPlant.PositionSpring.Reset();
			continue;
		}

		// run the goal position through a spring damper to un-plant it
		IKRigGoal->Position = UKismetMathLibrary::VectorSpringInterp(
				GoalToPlant.PrevGoalPosition,
				IKRigGoal->Position,
				GoalToPlant.PositionSpring,
				static_cast<float>(Settings.Stiffness),
				static_cast<float>(Settings.CriticalDamping),
				static_cast<float>(InDeltaTime),
				1.0f /*mass*/,
				0.0f /*target velocity amount*/);
	}
}

void FIKRetargetSpeedPlantingOp::OnAddedToStack(const UIKRetargeter* InRetargetAsset, const FIKRetargetOpBase* InParentOp)
{
	RegenerateChainSettings(InParentOp);
}

FIKRetargetOpSettingsBase* FIKRetargetSpeedPlantingOp::GetSettings()
{
	return &Settings;
}

const UScriptStruct* FIKRetargetSpeedPlantingOp::GetSettingsType() const
{
	return FIKRetargetSpeedPlantingOpSettings::StaticStruct();
}

const UScriptStruct* FIKRetargetSpeedPlantingOp::GetType() const
{
	return FIKRetargetSpeedPlantingOp::StaticStruct();
}

const UScriptStruct* FIKRetargetSpeedPlantingOp::GetParentOpType() const
{
	return FIKRetargetRunIKRigOp::StaticStruct();
}

void FIKRetargetSpeedPlantingOp::OnTargetChainRenamed(const FName InOldChainName, const FName InNewChainName)
{
	for (FRetargetSpeedPlantingSettings& ChainSettings : Settings.ChainsToSpeedPlant)
	{
		if (ChainSettings.TargetChainName == InOldChainName)
		{
			ChainSettings.TargetChainName = InNewChainName;
		}
	}
}

void FIKRetargetSpeedPlantingOp::OnParentReinitPropertyEdited(
	const FIKRetargetOpBase& InParentOp,
	const FPropertyChangedEvent* InPropertyChangedEvent)
{
	RegenerateChainSettings(&InParentOp);
}

void FIKRetargetSpeedPlantingOp::OnPlaybackReset()
{
	for (FSpeedPlantingGoalState& GoalToPlant : GoalsToPlant)
	{
		GoalToPlant.PositionSpring.Reset();
	}
	
	ResetThisTick = true;
}

void FIKRetargetSpeedPlantingOp::AnimGraphPreUpdateMainThread(
	USkeletalMeshComponent& SourceMeshComponent,
	USkeletalMeshComponent& TargetMeshComponent)
{
	if (!bIsInitialized)
	{
		return;
	}

	const UAnimInstance* SourceAnimInstance = SourceMeshComponent.GetAnimInstance();
	if (!SourceAnimInstance)
	{
		return;
	}

	// update speed values for each planted chain
	// NOTE: these are values from curves running on the SOURCE skeletal mesh
	// they will be overriden by any values coming from the target in AnimGraphEvaluateAnyThread
	const TMap<FName, float>& AnimCurveList = SourceAnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve);
	for (FSpeedPlantingGoalState& GoalToPlant : GoalsToPlant)
	{
		const float* SourceValue = AnimCurveList.Find(GoalToPlant.Settings->SpeedCurveName);
		GoalToPlant.CurrentSpeedValue = SourceValue ? *SourceValue : GoalToPlant.CurrentSpeedValue;
	}
}

void FIKRetargetSpeedPlantingOp::AnimGraphEvaluateAnyThread(FPoseContext& Output)
{
	if (!bIsInitialized)
	{
		return;
	}

	// update speed values for each planted chain
	// NOTE: these are values from curves running on the TARGET skeletal mesh
	// they will override any values coming from the source via AnimGraphPreUpdateMainThread()
	for (FSpeedPlantingGoalState& GoalToPlant : GoalsToPlant)
	{
		bool bFoundCurve = false;
		GoalToPlant.CurrentSpeedValue = Output.Curve.Get(GoalToPlant.Settings->SpeedCurveName, bFoundCurve, static_cast<float>(GoalToPlant.CurrentSpeedValue));
	}
}

TArray<FName> FIKRetargetSpeedPlantingOp::GetRequiredSpeedCurves() const
{
	TArray<FName> OutSpeedCurveNames;
	for (const FRetargetSpeedPlantingSettings& ChainToPlant : Settings.ChainsToSpeedPlant)
	{
		const FName SpeedCurveName = ChainToPlant.SpeedCurveName;
		if (SpeedCurveName != NAME_None)
		{
			OutSpeedCurveNames.Add(SpeedCurveName);
		}
	}
	return MoveTemp(OutSpeedCurveNames);
}

#if WITH_EDITOR
FText FIKRetargetSpeedPlantingOp::GetWarningMessage() const
{
	// warn about missing curves
	if (bIsInitialized)
	{
		int32 NumMissingCurves = 0;
		for (const FSpeedPlantingGoalState& GoalToPlant : GoalsToPlant)
		{
			if (!GoalToPlant.bFoundCurveInSourceComponent && !GoalToPlant.bFoundCurveInTargetComponent)
			{
				NumMissingCurves++;
			}
		}

		if (NumMissingCurves > 0)
		{
			return FText::Format(LOCTEXT("MissingSpeedCurves", "Running, but missing {0} speed curves."), FText::AsNumber(NumMissingCurves));
		}
	}

	return FIKRetargetOpBase::GetWarningMessage();
}
#endif

void FIKRetargetSpeedPlantingOp::RegenerateChainSettings(const FIKRetargetOpBase* InParentOp)
{
	const FIKRetargetRunIKRigOp* ParentOp = reinterpret_cast<const FIKRetargetRunIKRigOp*>(InParentOp);
	if (!ensure(ParentOp))
	{
		return;
	}
	
	// find the target chains that require goal retargeting
	const TArray<FName> RequiredTargetChains = ParentOp->GetRequiredTargetChains();
	if (RequiredTargetChains.IsEmpty())
	{
		// NOTE: if there's no chains, we don't clear the settings
		// this allows users to clear and reassign a different rig and potentially retain/restore compatible settings
		return;
	}

	// remove chains that are not required
	Settings.ChainsToSpeedPlant.RemoveAll([&RequiredTargetChains](const FRetargetSpeedPlantingSettings& InChainSettings)
	{
		return !RequiredTargetChains.Contains(InChainSettings.TargetChainName);
	});
	
	// add any required chains not already present
	for (FName RequiredTargetChain : RequiredTargetChains)
	{
		bool bFound = false;
		for (const FRetargetSpeedPlantingSettings& ChainToSpeedPlant : Settings.ChainsToSpeedPlant)
		{
			if (ChainToSpeedPlant.TargetChainName == RequiredTargetChain)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Settings.ChainsToSpeedPlant.Emplace(RequiredTargetChain);
		}
	}
}

FIKRetargetSpeedPlantingOpSettings UIKRetargetSpeedPlantingController::GetSettings()
{
	return *reinterpret_cast<FIKRetargetSpeedPlantingOpSettings*>(OpSettingsToControl);
}

void UIKRetargetSpeedPlantingController::SetSettings(FIKRetargetSpeedPlantingOpSettings InSettings)
{
	OpSettingsToControl->CopySettingsAtRuntime(&InSettings);
}

#undef LOCTEXT_NAMESPACE
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/RunIKRigOp.h"

#include "IKRigDebugRendering.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "RunIKRigSolversOp"

const UClass* FIKRetargetRunIKRigOpSettings::GetControllerType() const
{
	return UIKRetargetRunIKRigController::StaticClass();
}

void FIKRetargetRunIKRigOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	// copies everything except "IKRigAsset"
	const TArray<FName> PropertiesToIgnore = {"IKRigAsset"};
	FIKRetargetOpBase::CopyStructProperties(
		FIKRetargetRunIKRigOpSettings::StaticStruct(),
		InSettingsToCopyFrom,
		this,
		PropertiesToIgnore);
}

bool FIKRetargetRunIKRigOp::Initialize(
	const FIKRetargetProcessor& InProcessor,
	const FRetargetSkeleton& InSourceSkeleton,
	const FTargetSkeleton& InTargetSkeleton,
	const FIKRetargetOpBase* InParentOp,
	FIKRigLogger& InLog)
{
	bIsInitialized = false;

	if (!Settings.IKRigAsset)
	{
		InLog.LogWarning( LOCTEXT("NoIKRigFound", "Run IK Rig Op: No IK Rig asset was specified."));
		return false;
	}

	// initialize the IK Rig
	IKRigProcessor.Initialize(Settings.IKRigAsset, InTargetSkeleton.SkeletalMesh, InProcessor.GetIKRigGoalContainer());

	// warn if the IK Rig couldn't initialize
	if (!IKRigProcessor.IsInitialized())
	{
		// couldn't initialize the IK Rig, we don't disable the retargeter in this case, just warn the user
		InLog.LogWarning( FText::Format(
			LOCTEXT("CouldNotInitializeIKRig", "Run IK Rig Op: unable to initialize the IK Rig, {0} for the Skeletal Mesh {1}. See previous warnings."),
			FText::FromString(Settings.IKRigAsset->GetName()), FText::FromString(InTargetSkeleton.SkeletalMesh->GetName())));
	}
	
	bIsInitialized = IKRigProcessor.IsInitialized();
	return bIsInitialized;
}

void FIKRetargetRunIKRigOp::Run(
	FIKRetargetProcessor& InProcessor,
	const double InDeltaTime,
	const TArray<FTransform>& InSourceGlobalPose,
	TArray<FTransform>& OutTargetGlobalPose)
{
	if (InProcessor.IsIKForcedOff())
	{
		return; // skip this op when IK is off
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// live preview source asset settings in the retarget editor
		// NOTE: this copies solver settings and goal.PositionAlpha and goal.RotationAlpha
		IKRigProcessor.CopyAllSettingsFromAsset(Settings.IKRigAsset);
	}
#endif

	// set the goals
	IKRigProcessor.ApplyGoalsFromOtherContainer(InProcessor.GetIKRigGoalContainer());
	// trigger reinitialization if the goal container was modified such that it needs it
	if (IKRigProcessor.GetGoalContainer().NeedsInitialized())
	{
		const USkeletalMesh* TargetSkeletalMesh = InProcessor.GetSkeleton(ERetargetSourceOrTarget::Target).SkeletalMesh;
		IKRigProcessor.Initialize(Settings.IKRigAsset, TargetSkeletalMesh, InProcessor.GetIKRigGoalContainer());
	}
	
#if WITH_EDITOR
	// store initial goals transforms
	// (must be before the IK solve changes the bone transforms)
	SaveInitialGoalTransformsIntoDebugData(InProcessor, OutTargetGlobalPose);
#endif
	
	// copy input pose to start IK solve from
	IKRigProcessor.SetInputPoseGlobal(OutTargetGlobalPose);
	// run IK solve
	IKRigProcessor.Solve();
	// copy results of solve
	IKRigProcessor.GetOutputPoseGlobal(OutTargetGlobalPose);

#if WITH_EDITOR
	// store current goals transforms after the IK solve
	// (must be AFTER the IK solve because the IK Rig processor resolves the final goal transforms)
	SaveCurrentGoalTransformsIntoDebugData();
#endif
}

void FIKRetargetRunIKRigOp::OnAddedToStack(const UIKRetargeter* InRetargetAsset, const FIKRetargetOpBase* InParentOp)
{
	// load the target IK Rig asset to execute
	Settings.IKRigAsset = InRetargetAsset->GetIKRig(ERetargetSourceOrTarget::Target);

	// initialize the chain mapping
	ChainMapping.ReinitializeWithIKRigs(InRetargetAsset->GetIKRig(ERetargetSourceOrTarget::Source), Settings.IKRigAsset);
	static bool bForceRemap = true;
	ChainMapping.AutoMapChains(EAutoMapChainType::Fuzzy, bForceRemap);
}

FIKRetargetOpSettingsBase* FIKRetargetRunIKRigOp::GetSettings()
{
	return &Settings;
}

const UScriptStruct* FIKRetargetRunIKRigOp::GetSettingsType() const
{
	return FIKRetargetRunIKRigOpSettings::StaticStruct();
}

const UScriptStruct* FIKRetargetRunIKRigOp::GetType() const
{
	return FIKRetargetRunIKRigOp::StaticStruct();
}

const UIKRigDefinition* FIKRetargetRunIKRigOp::GetCustomTargetIKRig() const
{
	return Settings.IKRigAsset;
}

FRetargetChainMapping* FIKRetargetRunIKRigOp::GetChainMapping()
{
	return &ChainMapping;
}

void FIKRetargetRunIKRigOp::OnReinitPropertyEdited(const FPropertyChangedEvent* InPropertyChangedEvent)
{
	ChainMapping.ReinitializeWithIKRigs(ChainMapping.GetIKRig(ERetargetSourceOrTarget::Source), Settings.IKRigAsset);
}

#if WITH_EDITOR

FCriticalSection FIKRetargetRunIKRigOp::DebugDataMutex;

void FIKRetargetRunIKRigOp::DebugDraw(
	FPrimitiveDrawInterface* InPDI,
	const FTransform& InComponentTransform,
	const double InComponentScale,
	const FIKRetargetDebugDrawState& InEditorState) const
{
	// lock because main thread is doing the drawing while data is modified on worker
	FScopeLock ScopeLock(&DebugDataMutex);
	
	// draw IK goals on each IK chain
	if (!(Settings.bDrawGoals || Settings.bDrawGoalBoneLocations))
	{
		return;
	}

	// spin through all the IK goals debug data
	for (const FRunIKRigOpGoalDebugData& GoalData : GoalDebugData)
	{
		bool bIsSelected = InEditorState.SelectedGoals.Contains(GoalData.GoalName);

		FTransform Initial = GoalData.InitialGoal * InComponentTransform;
		FTransform Current = GoalData.CurrentGoal * InComponentTransform;

		if (Settings.bDrawGoals)
		{
			FLinearColor GoalColor = bIsSelected ? InEditorState.GoalColor : InEditorState.GoalColor * InEditorState.NonSelected;
			
			IKRigDebugRendering::DrawWireCube(
			InPDI,
			Current,
			GoalColor,
			static_cast<float>(Settings.GoalDrawSize * InComponentScale),
			static_cast<float>(Settings.GoalDrawThickness * InComponentScale));
		}
		
		if (Settings.bDrawGoalBoneLocations)
		{
			IKRigDebugRendering::DrawWireCube(
			InPDI,
			Initial,
			InEditorState.Muted,
			static_cast<float>(Settings.GoalDrawSize * InComponentScale * 0.5),
			static_cast<float>(Settings.GoalDrawThickness * InComponentScale));

			if (Settings.bDrawGoals)
			{
				DrawDashedLine(
					InPDI,
					Initial.GetLocation(),
					Current.GetLocation(),
					InEditorState.Muted,
					1.0f,
					SDPG_Foreground);
			}
		}
	}
}

void FIKRetargetRunIKRigOp::SaveInitialGoalTransformsIntoDebugData(
	const FIKRetargetProcessor& InProcessor,
	const TArray<FTransform>& InTargetGlobalPose)
{
	// lock because main thread is doing the drawing while this is on worker
	FScopeLock ScopeLock(&DebugDataMutex);
	
	GoalDebugData.Reset();
	const FRetargetSkeleton& TargetSkeleton = InProcessor.GetSkeleton(ERetargetSourceOrTarget::Target);
	const TArray<FIKRigGoal>& GoalArray = IKRigProcessor.GetGoalContainer().GetGoalArray();
	for (int32 GoalIndex=0; GoalIndex<GoalArray.Num(); ++GoalIndex)
	{
		const FIKRigGoal& Goal = GoalArray[GoalIndex];

		FRunIKRigOpGoalDebugData NewGoalData;
		NewGoalData.GoalName = Goal.Name;
		
		int32 BoneIndex = TargetSkeleton.FindBoneIndexByName(Goal.BoneName);
		if (!ensure(InTargetGlobalPose.IsValidIndex(BoneIndex)))
		{
			NewGoalData.InitialGoal = FTransform(Goal.Rotation, Goal.Position);
		}
		else
		{
			NewGoalData.InitialGoal = InTargetGlobalPose[BoneIndex];
		}
		
		GoalDebugData.Add(NewGoalData);
	}
}

void FIKRetargetRunIKRigOp::SaveCurrentGoalTransformsIntoDebugData()
{
	// lock because main thread is doing the drawing while this is on worker
	FScopeLock ScopeLock(&DebugDataMutex);
	
	const TArray<FIKRigGoal>& GoalArray = IKRigProcessor.GetGoalContainer().GetGoalArray();
	for (int32 GoalIndex=0; GoalIndex<GoalArray.Num(); ++GoalIndex)
	{
		const FIKRigGoal& Goal = GoalArray[GoalIndex];
		GoalDebugData[GoalIndex].CurrentGoal = FTransform(Goal.FinalBlendedRotation, Goal.FinalBlendedPosition);
	}
}
#endif

TArray<FName> FIKRetargetRunIKRigOp::GetRequiredTargetChains() const
{
	if (!Settings.IKRigAsset)
	{
		return TArray<FName>{};
	}
	
	// find the target chains that children ops should deal with
	TArray<FName> RequiredTargetChains;
	const TArray<FBoneChain>& TargetChains = Settings.IKRigAsset->GetRetargetChains();
	for (const FBoneChain& TargetChain : TargetChains)
	{
		if (TargetChain.IKGoalName == NAME_None)
		{
			continue; // skip non-IK chains
		}

		const FName SourceChain = ChainMapping.GetChainMappedTo(TargetChain.ChainName, ERetargetSourceOrTarget::Target);
		if (SourceChain == NAME_None)
		{
			continue; // skip unmapped chains
		}
		
		RequiredTargetChains.Add(TargetChain.ChainName);
	}
	
	return MoveTemp(RequiredTargetChains);
}

FIKRetargetRunIKRigOpSettings UIKRetargetRunIKRigController::GetSettings()
{
	return *reinterpret_cast<FIKRetargetRunIKRigOpSettings*>(OpSettingsToControl);
}

void UIKRetargetRunIKRigController::SetSettings(FIKRetargetRunIKRigOpSettings InSettings)
{
	OpSettingsToControl->CopySettingsAtRuntime(&InSettings);
}

#undef LOCTEXT_NAMESPACE

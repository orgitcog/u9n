// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/IKChainsOp.h"

#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "Retargeter/RetargetOps/StrideWarpingOp.h"

#if WITH_EDITOR
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#include "Engine/EngineTypes.h"
#include "IKRigDebugRendering.h"
#endif

#define LOCTEXT_NAMESPACE "IKChainsOp"


bool FSourceChainIK::Initialize(const FResolvedBoneChain& InSourceBoneChain, FIKRigLogger& InLog)
{
	if (InSourceBoneChain.BoneIndices.Num() < 2)
	{
		InLog.LogWarning(LOCTEXT("SourceChainLessThanThree", "IK Chains Op: trying to retarget source bone chain with IK but it has less than 2 joints."));
		return false;
	}

	SourceBoneChain = &InSourceBoneChain;
	StartBoneIndex = InSourceBoneChain.BoneIndices[0];
	EndBoneIndex = InSourceBoneChain.BoneIndices.Last();
	
	const FTransform& End = InSourceBoneChain.RefPoseGlobalTransforms.Last();
	PreviousEndPosition = End.GetTranslation();
	InitialEndPosition = End.GetTranslation();
	InitialEndRotation = End.GetRotation();

	const FTransform& Start = InSourceBoneChain.RefPoseGlobalTransforms[0];
	const float Length = static_cast<float>((Start.GetTranslation() - InitialEndPosition).Size());

	if (Length <= UE_KINDA_SMALL_NUMBER)
	{
		InLog.LogWarning(LOCTEXT("SourceZeroLengthIK", "IK Chains Op: found zero-length source bone chain."));
		return false;
	}

	InvInitialLength = 1.0f / Length;

	return true;
}

bool FTargetChainIK::Initialize(const FResolvedBoneChain& InTargetBoneChain, FIKRigLogger& InLog)
{
	if (InTargetBoneChain.BoneIndices.Num() < 3)
	{
		InLog.LogWarning(LOCTEXT("TargetChainLessThanThree", "IK Chains Op: trying to retarget target bone chain with IK but it has less than 3 joints."));
		return false;
	}

	TargetBoneChain = &InTargetBoneChain;
	BoneIndexA = InTargetBoneChain.BoneIndices[0];
	BoneIndexC = InTargetBoneChain.BoneIndices.Last();
	const FTransform& Last = InTargetBoneChain.RefPoseGlobalTransforms.Last();
	PrevEndPosition = Last.GetLocation();
	InitialEndPosition = Last.GetTranslation();
	InitialEndRotation = Last.GetRotation();
	InitialLength = static_cast<float>((InTargetBoneChain.RefPoseGlobalTransforms[0].GetTranslation() - Last.GetTranslation()).Size());

	if (InitialLength <= UE_KINDA_SMALL_NUMBER)
	{
		InLog.LogWarning(LOCTEXT("TargetZeroLengthIK", "IK Retargeter trying to retarget target bone chain with IK, but it is zero length!"));
		return false;
	}
	
	return true;
}

bool FIKChainRetargeter::Initialize(
	const FResolvedBoneChain& InSourceBoneChain,
	const FResolvedBoneChain& InTargetBoneChain,
	const FRetargetIKChainSettings& InSettings,
	FIKRigLogger& InLog)
{
	bool bIsInitialized = true;
	Settings = &InSettings;
	bIsInitialized &= Source.Initialize(InSourceBoneChain, InLog);
	bIsInitialized &= Target.Initialize(InTargetBoneChain, InLog);
	return bIsInitialized;
}

void FIKChainRetargeter::EncodePose(const TArray<FTransform>& InSourceGlobalPose)
{
	const FVector Start = InSourceGlobalPose[Source.StartBoneIndex].GetTranslation();
	const FVector End = InSourceGlobalPose[Source.EndBoneIndex].GetTranslation();

    // get the normalized direction / length of the IK limb (how extended it is as percentage of original length)
    const FVector ChainVector = End - Start;
	double ChainLength;
	FVector ChainDirection;
	ChainVector.ToDirectionAndLength(ChainDirection, ChainLength);
	const double NormalizedLimbLength = ChainLength * Source.InvInitialLength;

	Source.PreviousEndPosition = Source.CurrentEndPosition;
	Source.CurrentEndPosition = End;
	Source.CurrentEndDirectionNormalized = ChainDirection * NormalizedLimbLength;
	Source.CurrentEndRotation = InSourceGlobalPose[Source.EndBoneIndex].GetRotation();
	Source.CurrentHeightFromGroundNormalized = static_cast<float>(End.Z - Source.InitialEndPosition.Z)  * Source.InvInitialLength;
}
	
void FIKChainRetargeter::DecodePose(
	const FIKRetargetPelvisMotionOp* PelvisMotionOp,
    const TArray<FTransform>& InGlobalPose)
{
	//
	// calculate ROTATION of IK goal ...
	//
	
	// apply delta rotation from input
	const FQuat DeltaRotation = Source.CurrentEndRotation * Source.InitialEndRotation.Inverse();
	FQuat GoalRotation = DeltaRotation * Target.InitialEndRotation;

	// blend to source rotation
	const double BlendToSourceRotation = Settings->BlendToSource * Settings->BlendToSourceRotation;
	if (BlendToSourceRotation > UE_KINDA_SMALL_NUMBER)
	{
		GoalRotation = FQuat::FastLerp(GoalRotation, Source.CurrentEndRotation, BlendToSourceRotation);
		GoalRotation.Normalize();
	}

	// apply static rotation offset in the local space of the foot
	GoalRotation = GoalRotation * Settings->StaticRotationOffset.Quaternion();

	//
	// calculate POSITION of IK goal ...
	//
	
	// set position to length-scaled direction from source limb
	const FVector PelvisTranslationDelta = PelvisMotionOp ? PelvisMotionOp->GetPelvisTranslationOffset() : FVector::ZeroVector;
	const FVector AffectIKWeights = PelvisMotionOp ? PelvisMotionOp->GetAffectIKWeightAsVector() : FVector::ZeroVector;
	const FVector InvAffectIKWeights = FVector::OneVector - AffectIKWeights;
	const FVector InvRootModification = PelvisTranslationDelta * InvAffectIKWeights;
	const FVector Start = InGlobalPose[Target.BoneIndexA].GetTranslation() - InvRootModification;
	FVector GoalPosition = Start + (Source.CurrentEndDirectionNormalized * Target.InitialLength);

	// blend to source location
	const double BlendToSourceTranslation = Settings->BlendToSource * Settings->BlendToSourceTranslation;
	if (BlendToSourceTranslation > UE_KINDA_SMALL_NUMBER)
	{
		const FVector RootModification = PelvisTranslationDelta * AffectIKWeights;
		const FVector Weight = BlendToSourceTranslation * Settings->BlendToSourceWeights;
		const FVector SourceLocation = Source.CurrentEndPosition + RootModification;
		GoalPosition.X = FMath::Lerp(GoalPosition.X, SourceLocation.X, Weight.X);
		GoalPosition.Y = FMath::Lerp(GoalPosition.Y, SourceLocation.Y, Weight.Y);
		GoalPosition.Z = FMath::Lerp(GoalPosition.Z, SourceLocation.Z, Weight.Z);
	}

	// apply global static offset
	GoalPosition += Settings->StaticOffset;

	// apply local static offset
	GoalPosition += GoalRotation.RotateVector(Settings->StaticLocalOffset);

	// apply vertical scale
	GoalPosition.Z *= Settings->ScaleVertical;
	
	// apply extension
	if (!FMath::IsNearlyEqual(Settings->Extension, 1.0f))
	{
		GoalPosition = Start + (GoalPosition - Start) * Settings->Extension;	
	}
	
	// output transform
	Results.EndEffectorPosition = GoalPosition;
	Results.EndEffectorRotation = GoalRotation;
	Target.PrevEndPosition = GoalPosition;
}

bool FRetargetIKChainSettings::operator==(const FRetargetIKChainSettings& Other) const
{
	return EnableIK == Other.EnableIK
		&& FMath::IsNearlyEqualByULP(BlendToSource, Other.BlendToSource)
		&& BlendToSourceWeights.Equals(Other.BlendToSourceWeights)
		&& StaticOffset.Equals(Other.StaticOffset)
		&& StaticLocalOffset.Equals(Other.StaticLocalOffset)
		&& StaticRotationOffset.Equals(Other.StaticRotationOffset)
		&& FMath::IsNearlyEqualByULP(ScaleVertical, Other.ScaleVertical)
		&& FMath::IsNearlyEqualByULP(Extension, Other.Extension);
}

const UClass* FIKRetargetIKChainsOpSettings::GetControllerType() const
{
	return UIKRetargetIKChainsController::StaticClass();
}

void FIKRetargetIKChainsOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	// copies everything except the ChainsToRetarget array (those are copied below, only for already existing chains)
	const TArray<FName> PropertiesToIgnore = {"ChainsToRetarget"};
	FIKRetargetOpBase::CopyStructProperties(
		FIKRetargetIKChainsOpSettings::StaticStruct(),
		InSettingsToCopyFrom,
		this,
		PropertiesToIgnore);
	
	// copy ChainsToRetarget only for chains are in-common
	const FIKRetargetIKChainsOpSettings* NewSettings = reinterpret_cast<const FIKRetargetIKChainsOpSettings*>(InSettingsToCopyFrom);
	for (const FRetargetIKChainSettings& NewChainSettings : NewSettings->ChainsToRetarget)
	{
		for (FRetargetIKChainSettings& ChainSettings : ChainsToRetarget)
		{
			if (ChainSettings.TargetChainName == NewChainSettings.TargetChainName)
			{
				ChainSettings = NewChainSettings;
			}
		}
	}
}

bool FIKRetargetIKChainsOp::Initialize(
	const FIKRetargetProcessor& InProcessor,
	const FRetargetSkeleton& InSourceSkeleton,
	const FTargetSkeleton& InTargetSkeleton,
	const FIKRetargetOpBase* InParentOp,
	FIKRigLogger& InLog)
{
	bIsInitialized = false;
	IKChainRetargeters.Reset();

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
		LOCTEXT("MissingIKRig", "{0}, is missing an IK rig. No chains can be retargeted."), FText::FromName(GetName())));
		return false;
	}

	// go through all chains to retarget and load them
	const FRetargeterBoneChains& BoneChains = InProcessor.GetBoneChains();
	for (FRetargetIKChainSettings& ChainSettings : Settings.ChainsToRetarget)
	{
		FName TargetChainName = ChainSettings.TargetChainName;
		const FResolvedBoneChain* TargetBoneChain = BoneChains.GetResolvedBoneChainByName(
			TargetChainName,
			ERetargetSourceOrTarget::Target,
			ParentOp->Settings.IKRigAsset);

		// validate that the chain even exists
		if (TargetBoneChain == nullptr)
		{
			InLog.LogWarning( FText::Format(
			LOCTEXT("IKChainOpMissingChain", "IK Chain Op: chain data is out of sync with IK Rig. Missing target chain, '{0}."),
			FText::FromName(TargetChainName)));
			continue;
		}

		// validate that the chain has IK applied to it
		if (TargetBoneChain->IKGoalName == NAME_None)
		{
			InLog.LogWarning( FText::Format(
			LOCTEXT("IKChainOpChainHasNoIK", "IK Chain Op: an IK chain was found with no IK goal assigned to it, '{0}."),
			FText::FromName(TargetChainName)));
			continue;
		}
		
		// which source chain was this target chain mapped to?
		const FName SourceChainName = ParentOp->ChainMapping.GetChainMappedTo(TargetChainName, ERetargetSourceOrTarget::Target);
		const FResolvedBoneChain* SourceBoneChain = BoneChains.GetResolvedBoneChainByName(SourceChainName, ERetargetSourceOrTarget::Source);
		if (SourceBoneChain == nullptr)
		{
			InLog.LogWarning( FText::Format(
			LOCTEXT("IKChainOpChainNotMapped", "IK Chain Op: found IK chain that was not mapped to a source chain, '{0}."),
			FText::FromName(TargetChainName)));
			continue;
		}
		
		// initialize the mapped pair of source/target bone chains
		FIKChainRetargeter IKChainRetargeter;
		const bool bChainInitialized = IKChainRetargeter.Initialize(*SourceBoneChain, *TargetBoneChain, ChainSettings, InLog);
		if (!bChainInitialized)
		{
			InLog.LogWarning( FText::Format(
			LOCTEXT("IKChainOpBadChain", "IK Chain Op: could not initialize a mapped retarget chain for IK, '{0}."), FText::FromName(TargetChainName)));
			continue;
		}

		// warn user if IK goal is not on the END bone of the target chain. It will still work, but may produce bad results.
		const TArray<UIKRigEffectorGoal*>& AllGoals = ParentOp->Settings.IKRigAsset->GetGoalArray();
		for (const UIKRigEffectorGoal* Goal : AllGoals)
		{
			if (Goal->GoalName != TargetBoneChain->IKGoalName)
			{
				continue;
			}
			
			if (Goal->BoneName != TargetBoneChain->EndBone)
			{
				InLog.LogWarning( FText::Format(
			LOCTEXT("TargetIKNotOnEndBone", "IK Chain Op: Retarget chain, '{0}' has an IK goal that is not on the End Bone of the chain."),
				FText::FromString(TargetBoneChain->ChainName.ToString())));
			}
			break;
		}

		// store valid chain pair to be retargeted
		IKChainRetargeters.Add(IKChainRetargeter);
	}
	
	// consider initialized if at least 1 IK chain was initialized
	bIsInitialized = !IKChainRetargeters.IsEmpty();
	return bIsInitialized;
}

void FIKRetargetIKChainsOp::Run(
	FIKRetargetProcessor& InProcessor,
	const double InDeltaTime,
	const TArray<FTransform>& InSourceGlobalPose,
	TArray<FTransform>& OutTargetGlobalPose)
{
	if (InProcessor.IsIKForcedOff())
	{
		return; // skip this op when IK is off
	}
	
	const FIKRetargetPelvisMotionOp* PelvisMotionOp = InProcessor.GetFirstRetargetOpOfType<FIKRetargetPelvisMotionOp>();
	
	// retarget the IK goals to their new locations based on input pose
	for (FIKChainRetargeter& IKChainRetargeter : IKChainRetargeters)
	{
		// encode them all using the input pose
		IKChainRetargeter.EncodePose(InSourceGlobalPose);
		// decode the IK goal and apply to IKRig
		IKChainRetargeter.DecodePose(PelvisMotionOp, OutTargetGlobalPose);
	}

	// set the goal transform on the IK Rig
	FIKRigGoalContainer& GoalContainer = InProcessor.GetIKRigGoalContainer();
	for (const FIKChainRetargeter& IKChain : IKChainRetargeters)
	{
		constexpr double PositionAlpha = 1.0;
		constexpr double RotationAlpha = 1.0;
		
		FIKRigGoal Goal = FIKRigGoal(
			IKChain.GetTargetChain().IKGoalName,
			IKChain.GetTargetChain().EndBone,
			IKChain.GetResults().EndEffectorPosition,
			IKChain.GetResults().EndEffectorRotation,
			PositionAlpha,
			RotationAlpha,
			EIKRigGoalSpace::Component,
			EIKRigGoalSpace::Component,
			IKChain.GetSettings()->EnableIK);
		
		GoalContainer.SetIKGoal(Goal);
	}

#if WITH_EDITOR
	SaveDebugData(InProcessor, InSourceGlobalPose, OutTargetGlobalPose);
#endif
}

void FIKRetargetIKChainsOp::OnAddedToStack(const UIKRetargeter* InRetargetAsset, const FIKRetargetOpBase* InParentOp)
{
	RegenerateChainSettings(InParentOp);
}

FIKRetargetOpSettingsBase* FIKRetargetIKChainsOp::GetSettings()
{
	return &Settings;
}

void FIKRetargetIKChainsOp::SetSettings(const FIKRetargetOpSettingsBase* InSettings)
{
	const FIKRetargetIKChainsOpSettings* NewSettings = reinterpret_cast<const FIKRetargetIKChainsOpSettings*>(InSettings);
	
	// copies everything except the ChainsToRetarget array (those are copied below, only for already existing chains)
	const TArray<FName> PropertiesToIgnore = {"ChainsToRetarget"};
	CopySettingsRaw(InSettings, PropertiesToIgnore);
	
	// copy ChainsToRetarget only for chains are in-common
	for (const FRetargetIKChainSettings& NewChainSettings : NewSettings->ChainsToRetarget)
	{
		for (FRetargetIKChainSettings& ChainSettings : Settings.ChainsToRetarget)
		{
			if (ChainSettings.TargetChainName == NewChainSettings.TargetChainName)
			{
				ChainSettings = NewChainSettings;
			}
		}
	}
}

const UScriptStruct* FIKRetargetIKChainsOp::GetSettingsType() const
{
	return FIKRetargetIKChainsOpSettings::StaticStruct();
}

const UScriptStruct* FIKRetargetIKChainsOp::GetType() const
{
	return FIKRetargetIKChainsOp::StaticStruct();
}

const UScriptStruct* FIKRetargetIKChainsOp::GetParentOpType() const
{
	return FIKRetargetRunIKRigOp::StaticStruct();
}

void FIKRetargetIKChainsOp::OnTargetChainRenamed(const FName InOldChainName, const FName InNewChainName)
{
	for (FRetargetIKChainSettings& ChainSettings : Settings.ChainsToRetarget)
	{
		if (ChainSettings.TargetChainName == InOldChainName)
		{
			ChainSettings.TargetChainName = InNewChainName;
		}
	}
}

void FIKRetargetIKChainsOp::OnParentReinitPropertyEdited(
	const FIKRetargetOpBase& InParentOp,
	const FPropertyChangedEvent* InPropertyChangedEvent)
{
	RegenerateChainSettings(&InParentOp);
}

#if WITH_EDITOR

FCriticalSection FIKRetargetIKChainsOp::DebugDataMutex;

void FIKRetargetIKChainsOp::DebugDraw(
	FPrimitiveDrawInterface* InPDI,
	const FTransform& InComponentTransform,
	const double InComponentScale,
	const FIKRetargetDebugDrawState& InEditorState) const
{
	// draw IK goals on each IK chain
	if (!(Settings.bDrawFinalGoals || Settings.bDrawSourceLocations))
	{
		return;
	}

	// locked because this is called from the main thread and debug data is modified on worker
	FScopeLock ScopeLock(&DebugDataMutex);
	
	// spin through all IK chains
	for (const FChainDebugData& ChainDebugData : AllChainsDebugData)
	{
		FTransform FinalTransform = ChainDebugData.OutputTransformEnd * InComponentTransform;
		
		bool bIsSelected = InEditorState.SelectedChains.Contains(ChainDebugData.TargetChainName);

		InPDI->SetHitProxy(new HIKRetargetEditorChainProxy(ChainDebugData.TargetChainName));

		if (Settings.bDrawFinalGoals)
		{
			FLinearColor GoalColor = bIsSelected ? InEditorState.GoalColor : InEditorState.GoalColor * InEditorState.NonSelected;
			
			IKRigDebugRendering::DrawWireCube(
			InPDI,
			FinalTransform,
			GoalColor,
			static_cast<float>(Settings.GoalDrawSize),
			static_cast<float>(Settings.GoalDrawThickness * InComponentScale));
		}
	
		if (Settings.bDrawSourceLocations)
		{
			FTransform SourceGoalTransform;
			SourceGoalTransform.SetTranslation(ChainDebugData.SourceTransformEnd.GetLocation() + DebugRootModification);
			SourceGoalTransform.SetRotation(ChainDebugData.SourceTransformEnd.GetRotation());
			SourceGoalTransform *= InComponentTransform;

			FLinearColor Color = bIsSelected ? InEditorState.SourceColor : InEditorState.SourceColor * InEditorState.NonSelected;

			DrawWireSphere(
				InPDI,
				SourceGoalTransform,
				Color,
				Settings.GoalDrawThickness,
				12,
				SDPG_World,
				0.0f,
				0.001f,
				false);

			if (Settings.bDrawFinalGoals)
			{
				DrawDashedLine(
					InPDI,
					SourceGoalTransform.GetLocation(),
					FinalTransform.GetLocation(),
					Color,
					1.0f,
					SDPG_Foreground);
			}
		}

		// done drawing chain proxies
		InPDI->SetHitProxy(nullptr);
	}
}

void FIKRetargetIKChainsOp::SaveDebugData(
	const FIKRetargetProcessor& InProcessor,
	const TArray<FTransform>& InSourceGlobalPose,
	const TArray<FTransform>& OutTargetGlobalPose)
{
	FScopeLock ScopeLock(&DebugDataMutex);
	
	AllChainsDebugData.Reset();
	for (const FIKChainRetargeter& IKChainPair : IKChainRetargeters)
	{
		FChainDebugData NewChainData;
		const FSourceChainIK& Source = IKChainPair.GetSource();
		const FTargetChainIK& Target = IKChainPair.GetTarget();
		const FDecodedIKChain& Results = IKChainPair.GetResults();
		NewChainData.TargetChainName = IKChainPair.GetSettings()->TargetChainName;
		NewChainData.InputTransformStart = OutTargetGlobalPose[Target.BoneIndexA];
		NewChainData.InputTransformEnd = OutTargetGlobalPose[Target.BoneIndexC];
		NewChainData.OutputTransformEnd = FTransform(Results.EndEffectorRotation, Results.EndEffectorPosition);
		NewChainData.SourceTransformEnd = InSourceGlobalPose[Source.EndBoneIndex];
		AllChainsDebugData.Add(NewChainData);
	}

	// get the root modification
	DebugRootModification = FVector::ZeroVector;
	if (const FIKRetargetPelvisMotionOp* PelvisMotionOp = InProcessor.GetFirstRetargetOpOfType<FIKRetargetPelvisMotionOp>())
	{
		DebugRootModification = PelvisMotionOp->GetPelvisTranslationOffset() * PelvisMotionOp->GetAffectIKWeightAsVector();
	}
}

void FIKRetargetIKChainsOp::ResetChainSettingsToDefault(const FName& InChainName)
{
	for (FRetargetIKChainSettings& ChainToRetarget : Settings.ChainsToRetarget)
	{
		if (ChainToRetarget.TargetChainName == InChainName)
		{
			ChainToRetarget = FRetargetIKChainSettings(InChainName);
			return;
		}
	}
}

bool FIKRetargetIKChainsOp::AreChainSettingsAtDefault(const FName& InChainName)
{
	for (FRetargetIKChainSettings& ChainToRetarget : Settings.ChainsToRetarget)
	{
		if (ChainToRetarget.TargetChainName == InChainName)
		{
			FRetargetIKChainSettings DefaultSettings = FRetargetIKChainSettings();
			return ChainToRetarget == DefaultSettings;
		}
	}

	return true;
}

#endif

void FIKRetargetIKChainsOp::RegenerateChainSettings(const FIKRetargetOpBase* InParentOp)
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
	Settings.ChainsToRetarget.RemoveAll([&RequiredTargetChains](const FRetargetIKChainSettings& InChainSettings)
	{
		return !RequiredTargetChains.Contains(InChainSettings.TargetChainName);
	});
	
	// add any required chains not already present
	for (FName RequiredTargetChain : RequiredTargetChains)
	{
		bool bFound = false;
		for (const FRetargetIKChainSettings& ChainToRetarget : Settings.ChainsToRetarget)
		{
			if (ChainToRetarget.TargetChainName == RequiredTargetChain)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Settings.ChainsToRetarget.Emplace(RequiredTargetChain);
		}
	}
}

FIKRetargetIKChainsOpSettings UIKRetargetIKChainsController::GetSettings()
{
	return *reinterpret_cast<FIKRetargetIKChainsOpSettings*>(OpSettingsToControl);
}

void UIKRetargetIKChainsController::SetSettings(FIKRetargetIKChainsOpSettings InSettings)
{
	OpSettingsToControl->CopySettingsAtRuntime(&InSettings);
}

#undef LOCTEXT_NAMESPACE

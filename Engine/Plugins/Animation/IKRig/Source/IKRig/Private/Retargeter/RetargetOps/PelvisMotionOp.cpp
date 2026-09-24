// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/RetargetOps/PelvisMotionOp.h"

#include "Retargeter/IKRetargetProcessor.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#endif

#define LOCTEXT_NAMESPACE "PelvisMotionOp"


const UClass* FIKRetargetPelvisMotionOpSettings::GetControllerType() const
{
	return UIKRetargetPelvisMotionController::StaticClass();
}

void FIKRetargetPelvisMotionOpSettings::CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom)
{
	// copies everything except the bones we are operating on (those require reinit)
	const TArray<FName> PropertiesToIgnore = {"SourcePelvisBone", "TargetPelvisBone"};
	FIKRetargetOpBase::CopyStructProperties(
		FIKRetargetPelvisMotionOpSettings::StaticStruct(),
		InSettingsToCopyFrom,
		this,
		PropertiesToIgnore);
}

#if WITH_EDITOR
USkeleton* FIKRetargetPelvisMotionOpSettings::GetSkeleton(const FName InPropertyName)
{
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FIKRetargetPelvisMotionOpSettings, SourcePelvisBone))
	{
		return const_cast<USkeleton*>(SourceSkeletonAsset);
	}

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FIKRetargetPelvisMotionOpSettings, TargetPelvisBone))
	{
		return const_cast<USkeleton*>(TargetSkeletonAsset);
	}
	
	ensureMsgf(false, TEXT("Pelvis motion op unable to get skeleton for UI widget."));
	return nullptr;
}
#endif

bool FIKRetargetPelvisMotionOp::Initialize(
	const FIKRetargetProcessor& Processor,
	const FRetargetSkeleton& SourceSkeleton,
	const FTargetSkeleton& TargetSkeleton,
	const FIKRetargetOpBase* InParentOp,
	FIKRigLogger& Log)
{
	bIsInitialized = false;
	
	// reset root data
	Reset();
	
	// initialize root encoder
	const FName SourcePelvisBoneName = Settings.SourcePelvisBone.BoneName;
	const bool bPelvisEncoderInit = InitializeSource(SourcePelvisBoneName, SourceSkeleton, Log);
	if (!bPelvisEncoderInit)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("NoSourceRoot", "IK Retargeter unable to initialize source root, '{0}' on skeletal mesh: '{1}'"),
			FText::FromName(SourcePelvisBoneName), FText::FromString(SourceSkeleton.SkeletalMesh->GetName())));
	}

	// initialize root decoder
	const FName TargetPelvisBoneName = Settings.TargetPelvisBone.BoneName;
	const bool bPelvisDecoderInit = InitializeTarget(TargetPelvisBoneName, TargetSkeleton, Log);
	if (!bPelvisDecoderInit)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("NoTargetRoot", "IK Retargeter unable to initialize target root, '{0}' on skeletal mesh: '{1}'"),
			FText::FromName(TargetPelvisBoneName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
	}

#if WITH_EDITOR
	// record skeletons for UI bone selector widgets
	Settings.SourceSkeletonAsset = SourceSkeleton.SkeletalMesh->GetSkeleton();
	Settings.TargetSkeletonAsset = TargetSkeleton.SkeletalMesh->GetSkeleton();
#endif

	bIsInitialized = bPelvisEncoderInit && bPelvisDecoderInit;
	return bIsInitialized;
}

void FIKRetargetPelvisMotionOp::Run(
	FIKRetargetProcessor& Processor,
	const double InDeltaTime,
	const TArray<FTransform>& InSourceGlobalPose,
	TArray<FTransform>& OutTargetGlobalPose)
{
	FTransform NewPelvisGlobalTransform;
	
	EncodePose(InSourceGlobalPose);
	DecodePose(NewPelvisGlobalTransform);
	
	// update global transforms below root
	FTargetSkeleton& TargetSkeleton = Processor.GetTargetSkeleton();
	TargetSkeleton.SetGlobalTransformAndUpdateChildren(Target.BoneIndex, NewPelvisGlobalTransform, TargetSkeleton.OutputGlobalPose);

#if WITH_EDITOR
	FScopeLock ScopeLock(&DebugDataMutex);
	DebugPelvisTransform = OutTargetGlobalPose[Target.BoneIndex];
#endif
}

void FIKRetargetPelvisMotionOp::OnAddedToStack(const UIKRetargeter* InRetargetAsset, const FIKRetargetOpBase* InParentOp)
{
	// copy the source/target pelvis from the default IK Rig
	if (const UIKRigDefinition* SourceIKRig = InRetargetAsset->GetIKRig(ERetargetSourceOrTarget::Source))
	{
		Settings.SourcePelvisBone.BoneName = SourceIKRig->GetPelvis();
	}
	
	if (const UIKRigDefinition* TargetIKRig = InRetargetAsset->GetIKRig(ERetargetSourceOrTarget::Target))
	{
		Settings.TargetPelvisBone.BoneName = TargetIKRig->GetPelvis();
	}
}

bool FIKRetargetPelvisMotionOp::InitializeSource(
	const FName SourcePelvisBoneName,
	const FRetargetSkeleton& SourceSkeleton,
	FIKRigLogger& Log)
{
	// validate target root bone exists
	Source.BoneName = SourcePelvisBoneName;
	Source.BoneIndex = SourceSkeleton.FindBoneIndexByName(SourcePelvisBoneName);
	if (Source.BoneIndex == INDEX_NONE)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("MissingSourceRoot", "IK Retargeter could not find source root bone, {0} in mesh {1}"),
			FText::FromName(SourcePelvisBoneName), FText::FromString(SourceSkeleton.SkeletalMesh->GetName())));
		return false;
	}
	
	// record initial root data
	const FTransform InitialTransform = SourceSkeleton.RetargetPoses.GetGlobalRetargetPose()[Source.BoneIndex]; 
	float InitialHeight = static_cast<float>(InitialTransform.GetTranslation().Z);
	Source.InitialRotation = InitialTransform.GetRotation();

	// ensure root height is not at origin, this happens if user sets root to ACTUAL skeleton root and not pelvis
	if (InitialHeight < UE_KINDA_SMALL_NUMBER)
	{
		// warn user and push it up slightly to avoid divide by zero
		Log.LogError(LOCTEXT("BadPelvisHeight", "The source pelvis bone is very near the ground plane. This will cause the target to be moved very far. To resolve this, please create a retarget pose with the pelvis at the correct height off the ground."));
		InitialHeight = 1.0f;
	}

	// invert height
	Source.InitialHeightInverse = 1.0f / InitialHeight;

	return true;
}

bool FIKRetargetPelvisMotionOp::InitializeTarget(
	const FName TargetPelvisBoneName,
	const FTargetSkeleton& TargetSkeleton,
	FIKRigLogger& Log)
{
	// validate target root bone exists
	Target.BoneName = TargetPelvisBoneName;
	Target.BoneIndex = TargetSkeleton.FindBoneIndexByName(TargetPelvisBoneName);
	if (Target.BoneIndex == INDEX_NONE)
	{
		Log.LogWarning( FText::Format(
			LOCTEXT("CountNotFindRootBone", "IK Retargeter could not find target root bone, {0} in mesh {1}"),
			FText::FromName(TargetPelvisBoneName), FText::FromString(TargetSkeleton.SkeletalMesh->GetName())));
		return false;
	}

	const FTransform TargetInitialTransform = TargetSkeleton.RetargetPoses.GetGlobalRetargetPose()[Target.BoneIndex];
	Target.InitialHeight = static_cast<float>(TargetInitialTransform.GetTranslation().Z);
	Target.InitialRotation = TargetInitialTransform.GetRotation();
	Target.InitialPosition = TargetInitialTransform.GetTranslation();

	// initialize the global scale factor
	const float ScaleFactor = Source.InitialHeightInverse * Target.InitialHeight;
	GlobalScaleFactor.Set(ScaleFactor, ScaleFactor, ScaleFactor);
	
	return true;
}

FIKRetargetOpSettingsBase* FIKRetargetPelvisMotionOp::GetSettings()
{
	return &Settings;
}

const UScriptStruct* FIKRetargetPelvisMotionOp::GetSettingsType() const
{
	return FIKRetargetPelvisMotionOpSettings::StaticStruct();
}

const UScriptStruct* FIKRetargetPelvisMotionOp::GetType() const
{
	return FIKRetargetPelvisMotionOp::StaticStruct();
}

void FIKRetargetPelvisMotionOp::CollectRetargetedBones(TSet<int32>& OutRetargetedBones) const
{
	// the pelvis bone is retargeted
	if (Target.BoneIndex != INDEX_NONE)
	{
		OutRetargetedBones.Add(Target.BoneIndex);
	}
}

FName FIKRetargetPelvisMotionOp::GetPelvisBoneName(ERetargetSourceOrTarget SourceOrTarget) const
{
	if (!bIsInitialized)
	{
		return NAME_None;
	}

	return SourceOrTarget == ERetargetSourceOrTarget::Source ? Source.BoneName : Target.BoneName;
}

FVector FIKRetargetPelvisMotionOp::GetGlobalScaleVector() const
{
	return GlobalScaleFactor * FVector(Settings.ScaleHorizontal, Settings.ScaleHorizontal, Settings.ScaleVertical);
}

FVector FIKRetargetPelvisMotionOp::GetAffectIKWeightAsVector() const
{
	return FVector(Settings.AffectIKHorizontal, Settings.AffectIKHorizontal, Settings.AffectIKVertical);
}

FVector FIKRetargetPelvisMotionOp::GetPelvisTranslationOffset() const
{
	return Target.PelvisTranslationDelta;
}

void FIKRetargetPelvisMotionOp::Reset()
{
	Source = FPelvisSource();
	Target = FPelvisTarget();
}

void FIKRetargetPelvisMotionOp::EncodePose(const TArray<FTransform>& SourceGlobalPose)
{
	const FTransform& SourceTransform = SourceGlobalPose[Source.BoneIndex];
	Source.CurrentPosition = SourceTransform.GetTranslation();
	Source.CurrentPositionNormalized = Source.CurrentPosition * Source.InitialHeightInverse;
	Source.CurrentRotation = SourceTransform.GetRotation();	
}

void FIKRetargetPelvisMotionOp::DecodePose(FTransform& OutPelvisGlobalPose)
{
	// retarget position
	FVector Position;
	{
		// generate basic pelvis position by scaling the normalized position by root height
		const FVector RetargetedPosition = Source.CurrentPositionNormalized * Target.InitialHeight;
		
		// blend the pelvis position towards the source pelvis position
		const FVector PerAxisAlpha = Settings.BlendToSourceTranslation * Settings.BlendToSourceTranslationWeights;
		Position = FMath::Lerp(RetargetedPosition, Source.CurrentPosition, PerAxisAlpha);

		// apply vertical / horizontal scaling of motion
		FVector ScaledRetargetedPosition = Position;
		ScaledRetargetedPosition.Z *= Settings.ScaleVertical;
		const FVector HorizontalOffset = (ScaledRetargetedPosition - Target.InitialPosition) * FVector(Settings.ScaleHorizontal, Settings.ScaleHorizontal, 1.0f);
		Position = Target.InitialPosition + HorizontalOffset;
		
		// apply a static offset
		Position += Settings.TranslationOffset;

		// blend with alpha
		Position = FMath::Lerp(Target.InitialPosition, Position, Settings.TranslationAlpha);

		// record the delta created by all the modifications made to the root translation
		Target.PelvisTranslationDelta = Position - RetargetedPosition;
	}

	// retarget rotation
	FQuat Rotation;
	{
		// calc offset between initial source/target root rotations
		const FQuat RotationDelta = Source.CurrentRotation * Source.InitialRotation.Inverse();
		// add retarget pose delta to the current source rotation
		const FQuat RetargetedRotation = RotationDelta * Target.InitialRotation;

		// add static rotation offset
		Rotation = RetargetedRotation * Settings.RotationOffset.Quaternion();

		// blend with alpha
		Rotation = FQuat::FastLerp(Target.InitialRotation, Rotation, Settings.RotationAlpha);
		Rotation.Normalize();

		// record the delta created by all the modifications made to the root rotation
		Target.PelvisRotationDelta = RetargetedRotation * Target.InitialRotation.Inverse();
	}

	// apply to target
	OutPelvisGlobalPose.SetTranslation(Position);
	OutPelvisGlobalPose.SetRotation(Rotation);
}

#if WITH_EDITOR

FText FIKRetargetPelvisMotionOp::GetWarningMessage() const
{
	if (bIsInitialized)
	{
		return FText::Format(LOCTEXT("PelvisOpSuccess", "Running on {0}."), FText::FromName(Target.BoneName));
	}
	
	return FIKRetargetOpBase::GetWarningMessage();
}

FCriticalSection FIKRetargetPelvisMotionOp::DebugDataMutex;

void FIKRetargetPelvisMotionOp::DebugDraw(
	FPrimitiveDrawInterface* InPDI,
	const FTransform& InComponentTransform,
	const double InComponentScale,
	const FIKRetargetDebugDrawState& InEditorState) const
{
	if (!Settings.bEnableDebugDraw)
	{
		return;
	}

	FScopeLock ScopeLock(&DebugDataMutex);
	
	const FTransform PelvisTransform = DebugPelvisTransform * InComponentTransform;
	const FVector PelvisCircleLocation = PelvisTransform.GetLocation() * FVector(1,1,0);
	const FLinearColor CircleColor = InEditorState.bIsRootSelected ? InEditorState.MainColor : InEditorState.MainColor * InEditorState.NonSelected;

	InPDI->SetHitProxy(new HIKRetargetEditorRootProxy());
	DrawCircle(
		InPDI,
		PelvisCircleLocation,
		FVector(1, 0, 0),
		FVector(0, 1, 0),
		CircleColor,
		Settings.DebugDrawSize * InComponentTransform.GetScale3D().GetMax(),
		30,
		SDPG_World,
		static_cast<float>(Settings.DebugDrawThickness * InComponentScale));
	InPDI->SetHitProxy(nullptr);
}
#endif

FIKRetargetPelvisMotionOpSettings UIKRetargetPelvisMotionController::GetSettings()
{
	return GetPelvisOpSettings();
}

void UIKRetargetPelvisMotionController::SetSettings(FIKRetargetPelvisMotionOpSettings InSettings)
{
	OpSettingsToControl->CopySettingsAtRuntime(&InSettings);
}

void UIKRetargetPelvisMotionController::SetSourcePelvisBone(const FName InSourcePelvisBone)
{
	GetPelvisOpSettings().SourcePelvisBone.BoneName = InSourcePelvisBone;
}

FName UIKRetargetPelvisMotionController::GetSourcePelvisBone()
{
	return GetPelvisOpSettings().SourcePelvisBone.BoneName;
}

void UIKRetargetPelvisMotionController::SetTargetPelvisBone(const FName InTargetPelvisBone)
{
	GetPelvisOpSettings().TargetPelvisBone.BoneName = InTargetPelvisBone;
}

FName UIKRetargetPelvisMotionController::GetTargetPelvisBone()
{
	return GetPelvisOpSettings().TargetPelvisBone.BoneName;
}

FIKRetargetPelvisMotionOpSettings& UIKRetargetPelvisMotionController::GetPelvisOpSettings() const
{
	return *reinterpret_cast<FIKRetargetPelvisMotionOpSettings*>(OpSettingsToControl);
}

#undef LOCTEXT_NAMESPACE

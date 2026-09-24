// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_MotionMatchingInteraction.h"
#include "Animation/AnimInertializationSyncScope.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimRootMotionProvider.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "HAL/IConsoleManager.h"
#include "PoseSearch/PoseHistoryProvider.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchAssetSamplerLibrary.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchInteractionAsset.h"
#include "PoseSearch/PoseSearchInteractionIsland.h"
#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "PoseSearch/PoseSearchInteractionUtils.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchMirrorDataCache.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "VisualLogger/VisualLogger.h"

void FAnimNode_MotionMatchingInteraction::Reset()
{
	Super::Reset();
	CurrentResult = FPoseSearchBlueprintResult();
	MeshWithOffset = FTransform::Identity;
	MeshWithoutOffset = FTransform::Identity;
	CachedDeltaTime = 0.f;
}

bool FAnimNode_MotionMatchingInteraction::NeedsReset(const FAnimationUpdateContext& Context) const
{
	const bool bNeedsReset =
		bResetOnBecomingRelevant &&
		UpdateCounter.HasEverBeenUpdated() &&
		!UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter());
	return bNeedsReset;
}

void FAnimNode_MotionMatchingInteraction::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	using namespace UE::PoseSearch;

	check(Context.AnimInstanceProxy);
	CheckInteractionThreadSafety(Context.AnimInstanceProxy->GetAnimInstanceObject());

	if (NeedsReset(Context))
	{
		Reset();
	}

	const UE::AnimationWarping::FRootOffsetProvider* RootOffsetProvider = Context.GetMessage<UE::AnimationWarping::FRootOffsetProvider>();
	MeshWithoutOffset = Context.AnimInstanceProxy->GetComponentTransform();
	MeshWithOffset = RootOffsetProvider ? RootOffsetProvider->GetRootTransform() : MeshWithoutOffset;

	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());
	CachedDeltaTime = Context.GetDeltaTime();

	GetEvaluateGraphExposedInputs().Execute(Context);

	bool bBlendToExecuted = false;
	const float DeltaTime = Context.GetDeltaTime();
	if (FPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<FPoseHistoryProvider>())
	{
		UPoseSearchInteractionLibrary::MotionMatchInteraction(CurrentResult, Availabilities, Context.AnimInstanceProxy->GetAnimInstanceObject(), FName(),  & PoseHistoryProvider->GetPoseHistory(), bValidateResultAgainstAvailabilities);
		check(CurrentResult.ActorRootTransforms.Num() == CurrentResult.ActorRootBoneTransforms.Num());

		if (const UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(CurrentResult.SelectedAnim))
		{
			if (const UAnimationAsset* RoledAnimAsset = MultiAnimAsset->GetAnimationAsset(CurrentResult.Role))
			{
				if (AnimPlayers.IsEmpty() || !CurrentResult.bIsContinuingPoseSearch)
				{
					const FPoseSearchRoledSkeleton* RoledSkeleton = CurrentResult.SelectedDatabase->Schema->GetRoledSkeleton(CurrentResult.Role);
					check(RoledSkeleton);

					BlendTo(Context, const_cast<UAnimationAsset*>(RoledAnimAsset), CurrentResult.SelectedTime, CurrentResult.bLoop, CurrentResult.bIsMirrored, RoledSkeleton->MirrorDataTable.Get(),
						BlendTime, BlendProfile, BlendOption, bUseInertialBlend, NAME_None, CurrentResult.BlendParameters, CurrentResult.WantedPlayRate);

					bBlendToExecuted = true;
				}
			}
		}
	}
	else
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FAnimNode_MotionMatchingInteraction::Update_AnyThread couldn't find the FPoseHistoryProvider"));
	}

	const bool bDidBlendToRequestAnInertialBlend = bBlendToExecuted && bUseInertialBlend;
	UE::Anim::TOptionalScopedGraphMessage<UE::Anim::FAnimInertializationSyncScope> InertializationSync(bDidBlendToRequestAnInertialBlend, Context);
	
	if (CurrentResult.SelectedAnim)
	{
		UpdatePlayRate(CurrentResult.WantedPlayRate);
		UpdateBlendspaceParameters(BlendspaceUpdateMode, CurrentResult.BlendParameters);
	}

#if ENABLE_ANIM_DEBUG
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimationChannel))
	{
		TRACE_ANIM_NODE_VALUE(Context, *FString("WarpingRotationRatio"), WarpingRotationRatio);
		TRACE_ANIM_NODE_VALUE(Context, *FString("WarpingTranslationRatio"), WarpingTranslationRatio);
	}
#endif // ENABLE_ANIM_DEBUG

	Super::UpdateAssetPlayer(Context);
}

void FAnimNode_MotionMatchingInteraction::Evaluate_AnyThread(FPoseContext& Output)
{
	using namespace UE::PoseSearch;

	check(Output.AnimInstanceProxy);
	CheckInteractionThreadSafety(Output.AnimInstanceProxy->GetAnimInstanceObject());

	Super::Evaluate_AnyThread(Output);

	// calculating the animation time we want the full alignment transforms from
	UAnimInstance* AnimInstance = Cast<UAnimInstance>(Output.AnimInstanceProxy->GetAnimInstanceObject());
	if (!AnimInstance)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FAnimNode_MotionMatchingInteraction::Evaluate_AnyThread couldn't find the AnimInstance!?"));
		return;
	}

	const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();

	if (!RootMotionProvider)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FAnimNode_MotionMatchingInteraction::Evaluate_AnyThread couldn't find the IAnimRootMotionProvider"));
		return;
	}

	const UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(CurrentResult.SelectedAnim);
	if (!MultiAnimAsset)
	{
		return;
	}

	const int32 NumRoles = MultiAnimAsset->GetNumRoles();
	if (CurrentResult.ActorRootTransforms.Num() != NumRoles)
	{
		// warping is supported only for UMultiAnimAsset(s)
		return;
	}

	const int32 CurrentResultRoleIndex = GetRoleIndex(MultiAnimAsset, CurrentResult.Role);
	if (CurrentResultRoleIndex == INDEX_NONE)
	{
		return;
	}

	const UAnimationAsset* AnimationAsset = MultiAnimAsset->GetAnimationAsset(CurrentResult.Role);
	if (!AnimationAsset)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FAnimNode_MotionMatchingInteraction::Evaluate_AnyThread MultiAnimAsset %s for Role %s is invalid!"), *GetNameSafe(MultiAnimAsset), *CurrentResult.Role.ToString());
		return;
	}

	TArray<FTransform, TInlineAllocator<PreallocatedRolesNum>> FullAlignedTransforms;
	FullAlignedTransforms.SetNum(NumRoles);
	CalculateFullAlignedTransforms(CurrentResult, bWarpUsingRootBone, FullAlignedTransforms);

	// NoTe: keep in mind DeltaAlignment is relative to the previous execution frame so we still need to extract and and apply the current animation root motion transform to get to the current frame full aligned transform.
	const FTransform DeltaAlignment = CalculateDeltaAlignment(MeshWithoutOffset, MeshWithOffset, FullAlignedTransforms[CurrentResultRoleIndex], WarpingRotationRatio, WarpingTranslationRatio);
	
	FTransform RootMotionDelta;
	RootMotionProvider->ExtractRootMotion(Output.CustomAttributes, RootMotionDelta);

	const FTransform DeltaAlignmentWithRootMotion = DeltaAlignment * RootMotionDelta;
	RootMotionProvider->OverrideRootMotion(DeltaAlignmentWithRootMotion, Output.CustomAttributes);

#if ENABLE_VISUAL_LOG
	if (FVisualLogger::IsRecording())
	{
		static const TCHAR* LogName = TEXT("MotionMatchingInteraction");

		for (int32 Index = 0; Index < NumRoles; ++Index)
		{
			const FTransform& ActorRootTransform = CurrentResult.ActorRootTransforms[Index];
			const FTransform& FullAlignedTransform = FullAlignedTransforms[Index];

			UE_VLOG_SEGMENT_THICK(AnimInstance, LogName, Display, FullAlignedTransform.GetLocation(), ActorRootTransform.GetLocation(), FColorList::Orange, 1.f, TEXT(""));
			UE_VLOG_SEGMENT_THICK(AnimInstance, LogName, Display, ActorRootTransform.GetLocation(), ActorRootTransform.GetLocation() + ActorRootTransform.GetRotation().GetForwardVector() * 35, FColorList::LightGrey, 3.f, TEXT(""));
			UE_VLOG_SEGMENT_THICK(AnimInstance, LogName, Display, FullAlignedTransform.GetLocation(), FullAlignedTransform.GetLocation() + FullAlignedTransform.GetRotation().GetForwardVector() * 30, FColorList::Orange, 2.f, TEXT(""));
		}

		UE_VLOG_SEGMENT_THICK(AnimInstance, LogName, Display, MeshWithOffset.GetLocation(), MeshWithOffset.GetLocation() + MeshWithOffset.GetRotation().GetForwardVector() * 35, FColorList::Blue, 3.f, TEXT(""));
		UE_VLOG_SEGMENT_THICK(AnimInstance, LogName, Display, MeshWithoutOffset.GetLocation(), MeshWithoutOffset.GetLocation() + MeshWithoutOffset.GetRotation().GetForwardVector() * 40, FColorList::Cyan, 4.f, TEXT(""));
	}
#endif
}

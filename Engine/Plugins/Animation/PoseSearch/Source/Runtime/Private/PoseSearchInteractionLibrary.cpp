// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Pawn.h"
#include "PoseSearch/PoseSearchInteractionSubsystem.h"
#include "Engine/World.h"

FPoseSearchBlueprintResult UPoseSearchInteractionLibrary::MotionMatchInteraction_Pure(TArray<FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities)
{
	FPoseSearchBlueprintResult Result;
	MotionMatchInteraction(Result, Availabilities, AnimContext, PoseHistoryName, nullptr, bValidateResultAgainstAvailabilities);
	return Result;
}

FPoseSearchBlueprintResult UPoseSearchInteractionLibrary::MotionMatchInteraction(TArray<FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, bool bValidateResultAgainstAvailabilities)
{
	FPoseSearchBlueprintResult Result;
	MotionMatchInteraction(Result, Availabilities, AnimContext, PoseHistoryName, nullptr, bValidateResultAgainstAvailabilities);
	return Result;
}

void UPoseSearchInteractionLibrary::MotionMatchInteraction(FPoseSearchBlueprintResult& Result, const TArrayView<const FPoseSearchInteractionAvailability> Availabilities, const UObject* AnimContext, FName PoseHistoryName, const UE::PoseSearch::IPoseHistory* PoseHistory, bool bValidateResultAgainstAvailabilities)
{
	if (UPoseSearchInteractionSubsystem* InteractionSubsystem = UPoseSearchInteractionSubsystem::GetSubsystem_AnyThread(AnimContext))
	{
		InteractionSubsystem->Query_AnyThread(Availabilities, AnimContext, Result, PoseHistoryName, PoseHistory, bValidateResultAgainstAvailabilities);
	}
	else
	{
		Result = FPoseSearchBlueprintResult();
	}
}

FPoseSearchBlueprintResult UPoseSearchInteractionLibrary::GetCachedInteraction(const UObject* AnimContext, bool bCompareOwningActors)
{
	FPoseSearchBlueprintResult Result;
	if (UPoseSearchInteractionSubsystem* InteractionSubsystem = UPoseSearchInteractionSubsystem::GetSubsystem_AnyThread(AnimContext))
	{
		InteractionSubsystem->GetResult_AnyThread(AnimContext, Result, bCompareOwningActors);
	}
	return Result;
}

FPoseSearchContinuingProperties UPoseSearchInteractionLibrary::GetMontageContinuingProperties(UAnimInstance* AnimInstance)
{
	FPoseSearchContinuingProperties ContinuingProperties;
	if (const FAnimMontageInstance* AnimMontageInstance = AnimInstance->GetActiveMontageInstance())
	{
		ContinuingProperties.PlayingAsset = AnimMontageInstance->Montage;
		ContinuingProperties.PlayingAssetAccumulatedTime = AnimMontageInstance->DeltaTimeRecord.GetPrevious();
	}
	return ContinuingProperties;
}

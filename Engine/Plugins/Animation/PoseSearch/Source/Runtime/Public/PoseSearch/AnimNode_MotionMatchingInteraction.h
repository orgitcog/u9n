// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlendStack/AnimNode_BlendStack.h"
#include "PoseSearch/PoseSearchInteractionLibrary.h"
#include "AnimNode_MotionMatchingInteraction.generated.h"

#define UE_API POSESEARCH_API

struct FNodeDebugData;

USTRUCT(Experimental, BlueprintInternalUseOnly)
struct FAnimNode_MotionMatchingInteraction : public FAnimNode_BlendStack_Standalone
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category = Search, meta = (PinHiddenByDefault))
	TArray<FPoseSearchInteractionAvailability> Availabilities;

	UPROPERTY(EditAnywhere, Category = Search, meta = (PinHiddenByDefault))
	bool bValidateResultAgainstAvailabilities = true;

	// amount or translation warping to apply
	UPROPERTY(EditAnywhere, Category = Warping, meta = (PinHiddenByDefault, ClampMin = "0", ClampMax = "1"))
	float WarpingTranslationRatio = 1.f;

	// amount or rotation warping to apply
	UPROPERTY(EditAnywhere, Category = Warping, meta = (PinHiddenByDefault, ClampMin = "0", ClampMax = "1"))
	float WarpingRotationRatio = 1.f;

	// if bWarpUsingRootBone is true, warping will be calculated using the interacting actors previous frame root bone transforms (effective for setups with OffsetRootBone node allowing root bone drifting from capsule)
	// if bWarpUsingRootBone is true, warping will be calculated using the previous frame root transforms (effective root motion driven for setups)
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bWarpUsingRootBone = true;

	// Reset the blend stack if it has become relevant to the graph after not being updated on previous frames.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bResetOnBecomingRelevant = true;

	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault, ClampMin = "0"))
	float BlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault, UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> BlendProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;

	// How we should update individual blend space parameters. See dropdown options tooltips.
	UPROPERTY(EditAnywhere, Category = Settings)
	EBlendStack_BlendspaceUpdateMode BlendspaceUpdateMode = EBlendStack_BlendspaceUpdateMode::InitialOnly;

	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bUseInertialBlend = false;

	// FAnimNode_Base interface
	UE_API virtual void Evaluate_AnyThread(FPoseContext& Output) override;

	UE_API virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface

	UE_API virtual void Reset() override;

	bool IsInteracting() const { return CurrentResult.bIsInteraction; }

private:
	UE_API bool NeedsReset(const FAnimationUpdateContext& Context) const;

	UPROPERTY(Transient)
	FPoseSearchBlueprintResult CurrentResult;
	FTransform MeshWithOffset = FTransform::Identity;
	FTransform MeshWithoutOffset = FTransform::Identity;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;
	float CachedDeltaTime = 0.f;
};

#undef UE_API

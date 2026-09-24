// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchCost.h"
#include "PoseSearch/PoseSearchRole.h"
#include "PoseSearchResult.generated.h"

#define UE_API POSESEARCH_API

class UPoseSearchDatabase;
class UPoseSearchSchema;
struct FGameplayTag;
struct FPoseSearchBlueprintResult;

namespace UE::PoseSearch
{
struct FSearchIndexAsset;

struct FSearchResult
{
	// best cost of the currently selected PoseIdx (it could be equal to ContinuingPoseCost)
	FPoseSearchCost PoseCost;
	
	// pose index from Database associated to this search result
	int32 PoseIdx = INDEX_NONE;

	// Experimental, this feature might be removed without warning, not for production use
	// if this search result is from an event search, EventPoseIdx is the pose index at event time from Database associated to this search result
	// if it's not an event search result EventPoseIdx is INDEX_NONE
	int32 EventPoseIdx = INDEX_NONE;

	// database associated to this search result. Poses are derived data stored in Database->GetSearchIndex()
	TWeakObjectPtr<const UPoseSearchDatabase> Database;

	// time of the associated GetSearchIndexAsset from this search. During continuing pose searches,
	// AssetTime is resynchronized via UpdateWithNormalizedTime or UpdateWithRealTime, so it's not precisely the quantized time associated to PoseIdx
	float AssetTime = 0.f;

	// true if the search result is from a continuing pose search
	bool bIsContinuingPoseSearch = false;

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	// debug information related to the brute force search, to be able to compare it versus a kdtree/vptree search
	FPoseSearchCost BruteForcePoseCost;
	int32 BestPosePos = 0;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED

	// Experimental, this feature might be removed without warning, not for production use
	// Attempts to set the internal state to match the provided asset time including updating the internal DbPoseIdx. 
	// If the provided asset time is out of bounds for the currently playing asset then this function will reset the 
	// state back to the default state.
	void UpdateWithNormalizedTime(float NormalizedTime);
	
	// Experimental, this feature might be removed without warning, not for production use
	void UpdateWithRealTime(float RealTime);

	// Experimental, this feature might be removed without warning, not for production use
	bool DebugValidate() const;

	bool IsValid() const
	{
		return
			PoseIdx != INDEX_NONE &&
			Database != nullptr;
	}

	// Experimental, this feature might be removed without warning, not for production use
	bool IsEventSearchResult() const
	{
		return EventPoseIdx != INDEX_NONE;
	}

	// Experimental, this feature might be removed without warning, not for production use
	// returns true if EventPoseIdx is from the input EventTag
	bool IsEventSearchFromTag(const FGameplayTag& EventTag) const;
	
	// Experimental, this feature might be removed without warning, not for production use
	float CalculateTimeToEvent() const;

	void Reset()
	{
		PoseCost = FPoseSearchCost();
		PoseIdx = INDEX_NONE;
		EventPoseIdx = INDEX_NONE;
		Database = nullptr;
		AssetTime = 0.0f;
		bIsContinuingPoseSearch = false;

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
		BruteForcePoseCost = FPoseSearchCost();
		BestPosePos = 0;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	}

	POSESEARCH_API const FSearchIndexAsset* GetSearchIndexAsset(bool bMandatory = false) const;
	// Experimental, this feature might be removed without warning, not for production use
	POSESEARCH_API const UAnimationAsset* GetCurrentResultAnimationAsset() const;
	// Experimental, this feature might be removed without warning, not for production use
	POSESEARCH_API const UAnimationAsset* GetCurrentResultAnimationAsset(const FRole& Role) const;
	
	bool operator==(const FSearchResult& Other) const
	{
		// best cost of the currently selected PoseIdx (it could be equal to ContinuingPoseCost)
		return 
			PoseCost == Other.PoseCost &&
			PoseIdx == Other.PoseIdx &&
			EventPoseIdx == Other.EventPoseIdx &&
			Database == Other.Database &&
			AssetTime == Other.AssetTime &&
			bIsContinuingPoseSearch == Other.bIsContinuingPoseSearch;
	}

	// Experimental, this feature might be removed without warning, not for production use
	void InitFrom(const FPoseSearchBlueprintResult& BlueprintResult);
};

} // namespace UE::PoseSearch

USTRUCT(BlueprintType, Category="Animation|Pose Search")
struct FPoseSearchBlueprintResult
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	// Deprecated in 5.6. Use "SelectedAnim" property instead
	UPROPERTY(Transient, meta = (DeprecatedProperty, DeprecationMessage = "Use SelectedAnim instead"))
	TObjectPtr<const UObject> SelectedAnimation_DEPRECATED = nullptr;
#endif // WITH_EDITORONLY_DATA

	// selected animation selected by the motion matching alghoritm that matches the query (trajectory + historical pose) the best
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	TObjectPtr<UObject> SelectedAnim = nullptr;
	
	// SelectedAnim associated time (to have the best result out of the motion matchign selection, play the SelectedAnim at the SelectedTime)
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	float SelectedTime = 0.f;
	
	// SelectedAnim at SelectedTime is from the motion matching continuing pose search
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	bool bIsContinuingPoseSearch = false;
	
	// SelectedAnim suggested play rate (it's usually calculated by estimating the ratio between the trajectory speed in the query against the speed in the result)
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	float WantedPlayRate = 1.f;

	// SelectedAnim associated looping state
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	bool bLoop = false;
	
	// SelectedAnim associated mirror state
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	bool bIsMirrored = false;
	
	// SelectedAnim associated BlendParameters (if SelectedAnim is a UBlendSpace)
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	FVector BlendParameters = FVector::ZeroVector;

	// database containig the SelectedAnim
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	TObjectPtr<const UPoseSearchDatabase> SelectedDatabase = nullptr;

	// associated motion matching search cost for this result (how bad the SelectedAnim at SelectedTime etc looks in comparison to the query. The bigger the number, the more different the two are)
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
	float SearchCost = MAX_flt;

	// bIsInteraction is true if this is a result from a motion matching interaction search
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Experimental|Interaction", meta = (Experimental))
	bool bIsInteraction = false;

	// assigned role to this character. If this result is from a motion matched interaction, every interacting Actors will be assigned a different Role
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Experimental|Interaction", meta = (Experimental))
	FName Role = UE::PoseSearch::DefaultRole;

	// cached actors root transforms for all the roles in SelectedAnim (as UMultiAnimAsset). For all the optional missing roles (where the associated AnimContexts[i] is null), the ActorRootTransform will be identity
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Experimental|Interaction", meta = (Experimental))
	TArray<FTransform> ActorRootTransforms;

	// cached actors root bone transforms for all the roles in SelectedAnim (as UMultiAnimAsset). For all the optional missing roles (where the associated AnimContexts[i] is null), the ActorRootBoneTransform will be identity
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Experimental|Interaction", meta = (Experimental))
	TArray<FTransform> ActorRootBoneTransforms;

	// Experimental, this feature might be removed without warning, not for production use
	// AnimContexts involved in this interaction for all the roles in SelectedAnim (as UMultiAnimAsset). If the role is missing because optional the AnimContext will be null
	// BE CAREFUL HOW YOU USE THEM! since accessing data from other actors without proper tick dependencies leads to threading problems!
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UObject>> AnimContexts;

	// Experimental, this feature might be removed without warning, not for production use
	UE_API bool InitFrom(const UE::PoseSearch::FSearchResult& SearchResult, float InWantedPlayRate);

	// Experimental, this feature might be removed without warning, not for production use
	UE_API UAnimationAsset* GetAnimationAssetForRole();
};

#undef UE_API

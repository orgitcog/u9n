// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchResult.h"
#include "Animation/BlendSpace.h"
#include "PoseSearch/MultiAnimAsset.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "StructUtils/InstancedStruct.h"

namespace UE::PoseSearch
{

bool FSearchResult::DebugValidate() const
{
	bool bIsValidated = true;

#if WITH_EDITOR && ENABLE_ANIM_DEBUG

	if (IsValid())
	{
		const FSearchIndex& SearchIndex = Database->GetSearchIndex();
		const FSearchIndexAsset& SearchIndexAsset = SearchIndex.GetAssetForPose(PoseIdx);
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(SearchIndexAsset))
		{
			const float RealAssetTime = AssetTime * SearchIndexAsset.GetToRealTimeFactor();
			const int32 RecalculatedPoseIdx = SearchIndexAsset.GetPoseIndexFromTime(RealAssetTime, Database->Schema->SampleRate);
			if (RecalculatedPoseIdx != PoseIdx)
			{
				bIsValidated = false;
			}
		}
		else
		{
			bIsValidated = false;
		}
	}

#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

	return bIsValidated;
}

void FSearchResult::UpdateWithNormalizedTime(float NormalizedTime)
{
	check(DebugValidate());

	if (IsValid())
	{
		// for non blend spaces the real time corrisponds to the normalized time!
		const FSearchIndexAsset& SearchIndexAsset = Database->GetSearchIndex().GetAssetForPose(PoseIdx);
		const float RealTime = NormalizedTime * SearchIndexAsset.GetToRealTimeFactor();
		
#if WITH_EDITOR && DO_CHECK
		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(SearchIndexAsset);
		check(DatabaseAnimationAssetBase);
		if (Cast<UBlendSpace>(DatabaseAnimationAssetBase->GetAnimationAssetForRole(Database->Schema->GetDefaultRole())))
		{
			const float PlayLength = DatabaseAnimationAssetBase->GetPlayLength(SearchIndexAsset.GetBlendParameters());
			if (PlayLength > UE_KINDA_SMALL_NUMBER)
			{
				// Asset player time for blendspaces is normalized [0, 1] so we need to convert 
				// to a real time before we advance it
				check(NormalizedTime >= 0.f && NormalizedTime <= 1.f);
				check(FMath::IsNearlyEqual(RealTime, NormalizedTime * PlayLength));
			}
			else
			{
				check(FMath::IsNearlyEqual(SearchIndexAsset.GetToRealTimeFactor(), 1.f));
			}
		}
		else
		{
			check(FMath::IsNearlyEqual(SearchIndexAsset.GetToRealTimeFactor(), 1.f));
		}
#endif // WITH_EDITOR && DO_CHECK
		
		PoseIdx = SearchIndexAsset.GetPoseIndexFromTime(RealTime, Database->Schema->SampleRate);
		AssetTime = NormalizedTime;
	}
	else
	{
		Reset();
	}
	
	check(DebugValidate());
}

void FSearchResult::UpdateWithRealTime(float RealTime)
{
	check(DebugValidate());

	if (IsValid())
	{
		const FSearchIndexAsset& SearchIndexAsset = Database->GetSearchIndex().GetAssetForPose(PoseIdx);
		check(SearchIndexAsset.GetToRealTimeFactor() > UE_KINDA_SMALL_NUMBER);
		const float NormalizedTime = RealTime / SearchIndexAsset.GetToRealTimeFactor();

#if WITH_EDITOR && DO_CHECK
		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(SearchIndexAsset);
		check(DatabaseAnimationAssetBase);
		if (Cast<UBlendSpace>(DatabaseAnimationAssetBase->GetAnimationAssetForRole(Database->Schema->GetDefaultRole())))
		{
			const float PlayLength = DatabaseAnimationAssetBase->GetPlayLength(SearchIndexAsset.GetBlendParameters());
			if (PlayLength > UE_KINDA_SMALL_NUMBER)
			{
				// Asset player time for blendspaces is normalized [0, 1] so we need to convert 
				// to a real time before we advance it
				check(RealTime >= 0.f && RealTime <= PlayLength);
				check(FMath::IsNearlyEqual(NormalizedTime, PlayLength > UE_KINDA_SMALL_NUMBER ? RealTime / PlayLength : 0.f));
			}
			else
			{
				check(FMath::IsNearlyEqual(SearchIndexAsset.GetToRealTimeFactor(), 1.f));
			}
		}
		else
		{
			check(FMath::IsNearlyEqual(SearchIndexAsset.GetToRealTimeFactor(), 1.f));
		}
#endif // WITH_EDITOR && DO_CHECK

		PoseIdx = SearchIndexAsset.GetPoseIndexFromTime(RealTime, Database->Schema->SampleRate);
		AssetTime = NormalizedTime;
	}
	else
	{
		Reset();
	}
	
	check(DebugValidate());
}

bool FSearchResult::IsEventSearchFromTag(const FGameplayTag& EventTag) const
{
	check(IsValid());
	return Database->GetSearchIndex().EventData.IsPoseFromEventTag(EventPoseIdx, EventTag);
}

float FSearchResult::CalculateTimeToEvent() const
{
	check(IsValid() && IsEventSearchResult());
	const FSearchIndex& SearchIndex = Database->GetSearchIndex();

	check(SearchIndex.PoseMetadata[PoseIdx].GetAssetIndex() == SearchIndex.PoseMetadata[EventPoseIdx].GetAssetIndex());
	const FSearchIndexAsset& SearchIndexAsset = SearchIndex.GetAssetForPose(PoseIdx);

	// @todo: the mathc here can be simplified between GetDeltaTimeBetweenPoseIndexes and GetTimeFromPoseIndex methods
	// DeltaTimeBetweenPoseAndEvent is the time in seconds between the event pose (EventPoseIdx) and the current pose (PoseIdx) taking into consideration looping.
	const float DeltaTimeBetweenPoseAndEvent = SearchIndexAsset.GetDeltaTimeBetweenPoseIndexes(PoseIdx, EventPoseIdx, Database->Schema->SampleRate);
	// PoseQuantizedTime is the quantized time associated to PoseIdx
	const float PoseQuantizedTime = SearchIndexAsset.GetTimeFromPoseIndex(PoseIdx, Database->Schema->SampleRate);
	// AssetTime is the current search result time, that differs from PoseQuantizedTime in case this search result is from a continuing pose search
	// we calculate the QuantizationError as differnece between AssetTime and PoseQuantizedTime, time in seconds that this search result drifted away from the quantized time of the associated PoseIdx
	const float QuantizationError = AssetTime - PoseQuantizedTime;

	// time to event can be negative if PoseIdx already passed EventPoseIdx and the asset is not looping
	const float TimeToEvent = DeltaTimeBetweenPoseAndEvent - QuantizationError;
	return TimeToEvent;
}

const FSearchIndexAsset* FSearchResult::GetSearchIndexAsset(bool bMandatory) const
{
	if (bMandatory)
	{
		check(IsValid());
	}
	else if (!IsValid())
	{
		return nullptr;
	}

	return &Database->GetSearchIndex().GetAssetForPose(PoseIdx);
}

const UAnimationAsset* FSearchResult::GetCurrentResultAnimationAsset() const
{
	if (const FSearchIndexAsset* SearchIndexAsset = GetSearchIndexAsset())
	{
		return Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset)->GetAnimationAssetForRole(Database->Schema->GetDefaultRole());
	}
	return nullptr;
}

const UAnimationAsset* FSearchResult::GetCurrentResultAnimationAsset(const FRole& Role) const
{
	if (const FSearchIndexAsset* SearchIndexAsset = GetSearchIndexAsset())
	{
		return Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset)->GetAnimationAssetForRole(Role);
	}
	return nullptr;
}

void FSearchResult::InitFrom(const FPoseSearchBlueprintResult& BlueprintResult)
{
	PoseCost = FPoseSearchCost(BlueprintResult.SearchCost, 0.f, 0.f, 0.f);
	PoseIdx = BlueprintResult.SelectedDatabase ? BlueprintResult.SelectedDatabase->GetPoseIndex(BlueprintResult.SelectedAnim.Get(), BlueprintResult.SelectedTime, BlueprintResult.bIsMirrored, BlueprintResult.BlendParameters) : INDEX_NONE;
	EventPoseIdx = INDEX_NONE;
	Database = BlueprintResult.SelectedDatabase;
	AssetTime = BlueprintResult.SelectedTime;
	bIsContinuingPoseSearch = BlueprintResult.bIsContinuingPoseSearch;

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	BruteForcePoseCost = FPoseSearchCost();
	BestPosePos = 0;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
}

} // namespace UE::PoseSearch

bool FPoseSearchBlueprintResult::InitFrom(const UE::PoseSearch::FSearchResult& SearchResult, float InWantedPlayRate)
{
	using namespace UE::PoseSearch;

	if (const FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset())
	{
		const UPoseSearchDatabase* Database = SearchResult.Database.Get();

		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset);
		check(DatabaseAsset);
		
#if WITH_EDITORONLY_DATA
		SelectedAnimation_DEPRECATED = DatabaseAsset->GetAnimationAsset();
#endif // WITH_EDITORONLY_DATA

		SelectedAnim = DatabaseAsset->GetAnimationAsset();
		SelectedTime = SearchResult.AssetTime;
		bIsContinuingPoseSearch = SearchResult.bIsContinuingPoseSearch;
		WantedPlayRate = InWantedPlayRate;
		bLoop = SearchIndexAsset->IsLooping();
		bIsMirrored = SearchIndexAsset->IsMirrored();
		BlendParameters = SearchIndexAsset->GetBlendParameters();
		SelectedDatabase = Database;
		SearchCost = SearchResult.PoseCost;
		Role = Database->Schema->GetDefaultRole();

		bIsInteraction = false;
		ActorRootTransforms.Reset();
		ActorRootBoneTransforms.Reset();
		AnimContexts.Reset();
		return true;
	}
	
	*this = FPoseSearchBlueprintResult();
	return false;
}

UAnimationAsset* FPoseSearchBlueprintResult::GetAnimationAssetForRole()
{
	if (UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(SelectedAnim))
	{
		return AnimationAsset;
	}
	
	if (UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(SelectedAnim))
	{
		return MultiAnimAsset->GetAnimationAsset(Role);
	}

	return nullptr;
}
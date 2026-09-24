// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchDatabase.h"

// @todo: remove this include once the deprecated method FPoseSearchDatabaseAnimationAssetBase::GetFrameAtTime is removed. 
//        used only to get UAnimationSettings::Get()->GetDefaultFrameRate()
#include "Animation/AnimationSettings.h"

#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Chooser/Internal/Chooser.h"
#include "IObjectChooser.h"
#include "PoseSearch/MultiAnimAsset.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchFeatureChannel_Group.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchIndex.inl"
#include "Serialization/ArchiveCountMem.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif //WITH_EDITOR

#if WITH_EDITOR && WITH_ENGINE
#include "Editor/EditorEngine.h"
#endif //WITH_EDITOR && WITH_ENGINE

struct FPoseSearchDatabaseAnimationAssetBase;
DECLARE_STATS_GROUP(TEXT("PoseSearch"), STATGROUP_PoseSearch, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search Brute Force"), STAT_PoseSearch_BruteForce, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search PCA/KNN"), STAT_PoseSearch_PCAKNN, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search VPTree"), STAT_PoseSearch_VPTree, STATGROUP_PoseSearch, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Search Event"), STAT_PoseSearch_Event, STATGROUP_PoseSearch, );
DEFINE_STAT(STAT_PoseSearch_BruteForce);
DEFINE_STAT(STAT_PoseSearch_PCAKNN);
DEFINE_STAT(STAT_PoseSearch_VPTree);
DEFINE_STAT(STAT_PoseSearch_Event);


namespace UE::PoseSearch
{
#if WITH_EDITOR && ENABLE_ANIM_DEBUG

static bool GVarMotionMatchCompareAgainstBruteForce = false;
static FAutoConsoleVariableRef CVarMotionMatchCompareAgainstBruteForce(TEXT("a.MotionMatch.CompareAgainstBruteForce"), GVarMotionMatchCompareAgainstBruteForce, TEXT("Compare optimized search against brute force search"));

static bool GVarMotionMatchValidateKNNSearch = false;
static FAutoConsoleVariableRef CVarMotionMatchValidateKNNSearch(TEXT("a.MotionMatch.ValidateKNNSearch"), GVarMotionMatchValidateKNNSearch, TEXT("Validate KNN search"));

// Experimental, this feature might be removed without warning, not for production use
static bool GVarMotionMatchProfileMaxHeapKNNSearch = false;
static FAutoConsoleVariableRef CVarMotionMatchProfileMaxHeapKNNSearch(TEXT("a.MotionMatch.ProfileMaxHeapKNNSearch"), GVarMotionMatchProfileMaxHeapKNNSearch, TEXT("Profile MaxHeap KNN Search"));

#endif

// Experimental, this feature might be removed without warning, not for production use
static int32 GVarMotionMatchDebugWeightGroupID = 0;
static FAutoConsoleVariableRef CVarMotionMatchDebugWeightGroupID(TEXT("a.MotionMatch.DebugWeightGroupID"), GVarMotionMatchDebugWeightGroupID, TEXT("Only the channels with this or negative DebugWeightGroupID will have non zero weights"));

struct FSearchFilters
{
	FSearchFilters(const UPoseSearchSchema* Schema, TConstArrayView<int32> NonSelectableIdx, TConstArrayView<int32> SelectableAssetIdx, bool bAddBlockTransitionFilter)
	{
		if (bAddBlockTransitionFilter)
		{
			Filters.Add(&BlockTransitionFilter);
		}

		if (NonSelectableIdxFilter.Init(NonSelectableIdx).IsFilterActive())
		{
			Filters.Add(&NonSelectableIdxFilter);
		}

		if (SelectableAssetIdxFilter.Init(SelectableAssetIdx).IsFilterActive())
		{
			Filters.Add(&SelectableAssetIdxFilter);
		}

		for (const IPoseSearchFilter* Filter : Schema->GetChannels())
		{
			if (Filter->IsFilterActive())
			{
				Filters.Add(Filter);
			}
		}
	}

	// @todo: template this with bAlignedAndPadded to be able to use faster ComparePoses
	bool AreFiltersValid(const FSearchIndex& SearchIndex, TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, TConstArrayView<float> DynamicWeightsSqrt, int32 PoseIdx
#if UE_POSE_SEARCH_TRACE_ENABLED
		, float ContinuingPoseCostAddend, float ContinuingInteractionCostAddend, UE::PoseSearch::FSearchContext& SearchContext, const UPoseSearchDatabase* Database
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	) const
	{
		for (const IPoseSearchFilter* Filter : Filters)
		{
			if (!Filter->IsFilterValid(PoseValues, QueryValues, PoseIdx, SearchIndex.PoseMetadata[PoseIdx]))
			{
#if UE_POSE_SEARCH_TRACE_ENABLED
				if (Filter == &NonSelectableIdxFilter)
				{
					// candidate already added to SearchContext.BestCandidates by PopulateNonSelectableIdx
				}
				else if (Filter == &SelectableAssetIdxFilter)
				{
					const FPoseSearchCost PoseCost(CompareFeatureVectors<false>(PoseValues, QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[PoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
					SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_AssetIdxFilter, PoseCost);
				}
				else if (Filter == &BlockTransitionFilter)
				{
					const FPoseSearchCost PoseCost(CompareFeatureVectors<false>(PoseValues, QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[PoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
					SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_BlockTransition, PoseCost);
				}
				else
				{
					const FPoseSearchCost PoseCost(CompareFeatureVectors<false>(PoseValues, QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[PoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
					SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::DiscardedBy_PoseFilter, PoseCost);
				}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
				return false;
			}
		}
		return true;
	};

private:
	struct FNonSelectableIdxFilter : public IPoseSearchFilter
	{
		const FNonSelectableIdxFilter& Init(TConstArrayView<int32> InNonSelectableIdx)
		{
			check(Algo::IsSorted(InNonSelectableIdx));
			NonSelectableIdx = InNonSelectableIdx;
			return *this;
		}

		virtual bool IsFilterActive() const override
		{
			return !NonSelectableIdx.IsEmpty();
		}

		virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata) const override
		{
			return Algo::BinarySearch(NonSelectableIdx, PoseIdx) == INDEX_NONE;
		}

		TConstArrayView<int32> NonSelectableIdx;
	};

	struct FSelectableAssetIdxFilter : public IPoseSearchFilter
	{
		const FSelectableAssetIdxFilter& Init(TConstArrayView<int32> InSelectableAssetIdxFilter)
		{
			check(Algo::IsSorted(InSelectableAssetIdxFilter));
			SelectableAssetIdxFilter = InSelectableAssetIdxFilter;
			return *this;
		}

		virtual bool IsFilterActive() const override
		{
			return !SelectableAssetIdxFilter.IsEmpty();
		}

		virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata) const override
		{
			return Algo::BinarySearch(SelectableAssetIdxFilter, int32(Metadata.GetAssetIndex())) != INDEX_NONE;
		}

		TConstArrayView<int32> SelectableAssetIdxFilter;
	};

	struct FBlockTransitionFilter : public IPoseSearchFilter
	{
		virtual bool IsFilterActive() const override
		{
			return true;
		}

		virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseMetadata& Metadata) const override
		{
			return !Metadata.IsBlockTransition();
		}
	};

	FNonSelectableIdxFilter NonSelectableIdxFilter;
	FSelectableAssetIdxFilter SelectableAssetIdxFilter;
	FBlockTransitionFilter BlockTransitionFilter;

	TArray<const IPoseSearchFilter*, TInlineAllocator<64, TMemStackAllocator<>>> Filters;
};

template<bool bReconstructPoseValues, bool bAlignedAndPadded>
static inline void EvaluatePoseKernel(UE::PoseSearch::FSearchResult& Result, const UE::PoseSearch::FSearchIndex& SearchIndex, TConstArrayView<float> QueryValues, TArrayView<float> ReconstructedPoseValuesBuffer,
	int32 PoseIdx, int32 EventPoseIdx, const UE::PoseSearch::FSearchFilters& SearchFilters, float ContinuingPoseCostAddend, float ContinuingInteractionCostAddend,
	UE::PoseSearch::FSearchContext& SearchContext, const UPoseSearchDatabase* Database, TConstArrayView<float> DynamicWeightsSqrt, bool bUpdateBestCandidates, int32 ResultIndex = -1)
{
	using namespace UE::PoseSearch;

	const TConstArrayView<float> PoseValues = bReconstructPoseValues ? SearchIndex.GetReconstructedPoseValues(PoseIdx, ReconstructedPoseValuesBuffer) : SearchIndex.GetPoseValues(PoseIdx);

	if (SearchFilters.AreFiltersValid(SearchIndex, PoseValues, QueryValues, DynamicWeightsSqrt, PoseIdx
#if UE_POSE_SEARCH_TRACE_ENABLED
		, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, Database
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	))
	{
		const FPoseSearchCost PoseCost(CompareFeatureVectors<bAlignedAndPadded>(PoseValues, QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[PoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
		if (PoseCost < Result.PoseCost)
		{
			Result.PoseCost = PoseCost;
			Result.PoseIdx = PoseIdx;
			Result.EventPoseIdx = EventPoseIdx;

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
			if (bUpdateBestCandidates)
			{
				Result.BestPosePos = ResultIndex;
			}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
		}

#if UE_POSE_SEARCH_TRACE_ENABLED
		if (bUpdateBestCandidates)
		{
			// @todo: add tracking for EventPoseIdx
			SearchContext.Track(Database, PoseIdx, EPoseCandidateFlags::Valid_Pose, PoseCost);
		}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}
}

static void IterateOverBlendSpaceSamplingParameter(const UBlendSpace* BlendSpace, bool bUseSingleSample, const FVector& SingleSampleBlendParameters,
	bool bUseGridForSampling, int32 NumberOfHorizontalSamples, int32 NumberOfVerticalSamples,
	const TFunction<void(const FVector& BlendParameters)>& ProcessSamplingParameter)
{
	check(BlendSpace);
	
	if (bUseSingleSample)
	{
		ProcessSamplingParameter(SingleSampleBlendParameters);
	}
	else if (bUseGridForSampling)
	{
		for (const FBlendSample& BlendSample : BlendSpace->GetBlendSamples())
		{
			ProcessSamplingParameter(BlendSample.SampleValue);
		}
	}
	else
	{
		const int32 HorizontalBlendNum = FMath::Max(NumberOfHorizontalSamples, 1);
		const int32 VerticalBlendNum = BlendSpace->IsA<UBlendSpace1D>() ? 1 : FMath::Max(NumberOfVerticalSamples, 1);

		const FBlendParameter& HorizontalBlendParameter = BlendSpace->GetBlendParameter(0);
		const FBlendParameter& VerticalBlendParameter = BlendSpace->GetBlendParameter(1);

		const int32 WrapInputHorizontalBlendNum = HorizontalBlendParameter.bWrapInput ? HorizontalBlendNum + 1 : HorizontalBlendNum;
		const int32 WrapInputVerticalBlendNum = VerticalBlendParameter.bWrapInput ? VerticalBlendNum + 1 : VerticalBlendNum;

		for (int32 HorizontalBlendIndex = 0; HorizontalBlendIndex < HorizontalBlendNum; HorizontalBlendIndex++)
		{
			for (int32 VerticalBlendIndex = 0; VerticalBlendIndex < VerticalBlendNum; VerticalBlendIndex++)
			{
				const FVector BlendParameters = FVector(
						WrapInputHorizontalBlendNum > 1 ? 
							HorizontalBlendParameter.Min + (HorizontalBlendParameter.Max - HorizontalBlendParameter.Min) * ((float)HorizontalBlendIndex) / (WrapInputHorizontalBlendNum - 1) :
							HorizontalBlendParameter.Min,
						WrapInputVerticalBlendNum > 1 ?
							VerticalBlendParameter.Min + (VerticalBlendParameter.Max - VerticalBlendParameter.Min) * ((float)VerticalBlendIndex) / (WrapInputVerticalBlendNum - 1) :
							VerticalBlendParameter.Min,
						0.f);

				ProcessSamplingParameter(BlendParameters);
			}
		}
	}
}

static bool IsBlendSpaceRootMotionEnabled(const UBlendSpace* BlendSpace)
{
	check(BlendSpace);

	bool bIsRootMotionUsedInBlendSpace = false;
	BlendSpace->ForEachImmutableSample([&bIsRootMotionUsedInBlendSpace](const FBlendSample& Sample)
		{
			const UAnimSequence* Sequence = Sample.Animation.Get();
			if (IsValid(Sequence) && Sequence->HasRootMotion())
			{
				bIsRootMotionUsedInBlendSpace = true;
			}
		});

	return bIsRootMotionUsedInBlendSpace;
}

} // namespace UE::PoseSearch

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAnimationAssetBase
float FPoseSearchDatabaseAnimationAssetBase::GetPlayLength(const FVector& BlendParameters) const
{
	if (const UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(GetAnimationAsset()))
	{
		return AnimationAsset->GetPlayLength();
	}
	return 0;
}

#if WITH_EDITOR
int32 FPoseSearchDatabaseAnimationAssetBase::GetFrameAtTime(float Time) const
{
	if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(GetAnimationAsset()))
	{
		return SequenceBase->GetFrameAtTime(Time);
	}

	// estimating the frame for blend spaces in a non precise way. Anyways this method is deprecated and should not be used
	const float RealAssetTime = GetPlayLength(FVector::ZeroVector);
	const FFrameRate& DefaultFrameRate = UAnimationSettings::Get()->GetDefaultFrameRate();
	return DefaultFrameRate.AsFrameTime(RealAssetTime).RoundToFrame().Value;
}

bool FPoseSearchDatabaseAnimationAssetBase::IsSkeletonCompatible(TObjectPtr<const UPoseSearchSchema> InSchema) const
{
	if (InSchema)
	{
		TArray<FPoseSearchRoledSkeleton> RoledSkeletons = InSchema->GetRoledSkeletons();

		if (GetAnimationAsset())
		{
			const int32 NumRoles = GetNumRoles();
			for (int RoleIdx = 0; RoleIdx < NumRoles; ++RoleIdx)
			{
				UE::PoseSearch::FRole Role = GetRole(RoleIdx);
				FAssetData AssetData = IAssetRegistry::Get()->GetAssetByObjectPath(FSoftObjectPath(GetAnimationAssetForRole(Role)));
		
				for (const FPoseSearchRoledSkeleton& RoledSkeleton : RoledSkeletons)
				{
					if (RoledSkeleton.Role == Role)
					{
						// Match skeleton
						if (RoledSkeleton.Skeleton && RoledSkeleton.Skeleton->IsCompatibleForEditor(AssetData))
						{
							return true;
						}
					}
				}
			}
		}
	}
	
	return false;
}

USkeletalMesh* FPoseSearchDatabaseAnimationAssetBase::GetPreviewMeshForRole(const UE::PoseSearch::FRole& Role) const
{
	return nullptr;
}

void FPoseSearchDatabaseAnimationAssetBase::IterateOverSamplingParameter(const TFunction<void(const FVector& BlendParameters)>& ProcessSamplingParameter) const
{
	if (GetAnimationAsset())
	{
		ProcessSamplingParameter(FVector::ZeroVector);
	}
}

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
const FString FPoseSearchDatabaseAnimationAssetBase::GetName() const
{
	return GetNameSafe(GetAnimationAsset());
}
#endif //WITH_EDITORONLY_DATA

UAnimationAsset* FPoseSearchDatabaseAnimationAssetBase::GetAnimationAssetForRole(const UE::PoseSearch::FRole& Role) const
{
	check(GetNumRoles() == 1);
	return Cast<UAnimationAsset>(GetAnimationAsset());
}

FTransform FPoseSearchDatabaseAnimationAssetBase::GetRootTransformOriginForRole(const UE::PoseSearch::FRole& Role) const
{
	check(GetNumRoles() == 1);
	return FTransform::Identity;
}

#if WITH_EDITORONLY_DATA

bool FPoseSearchDatabaseAnimationAssetBase::UpdateFrom(const FPoseSearchDatabaseAnimationAssetBase& Source)
{
	if (BranchInId != 0 && BranchInId == Source.BranchInId)
	{
		SetSamplingRange(Source.GetSamplingRange());
		return true;
	}
	return false;
}

int64 FPoseSearchDatabaseAnimationAssetBase::GetEditorMemSize() const
{
	FArchiveCountMem EditorMemCount(GetAnimationAsset());
	return EditorMemCount.GetNum();
}

FFloatInterval FPoseSearchDatabaseAnimationAssetBase::GetEffectiveSamplingRange(const FVector& BlendParameters) const
{
	return GetEffectiveSamplingRange(GetPlayLength(BlendParameters), GetSamplingRange());
}

FFloatInterval FPoseSearchDatabaseAnimationAssetBase::GetEffectiveSamplingRange() const
{
	return GetEffectiveSamplingRange(GetPlayLength(FVector::ZeroVector), GetSamplingRange());
}

FFloatInterval FPoseSearchDatabaseAnimationAssetBase::GetEffectiveSamplingRange(float PlayLength, const FFloatInterval& SamplingRange)
{
	const bool bSampleAll = (SamplingRange.Min == 0.0f) && (SamplingRange.Max == 0.0f);
	FFloatInterval Range;
	Range.Min = bSampleAll ? 0.0f : SamplingRange.Min;
	Range.Max = bSampleAll ? PlayLength : FMath::Min(PlayLength, SamplingRange.Max);

	if (Range.Min > Range.Max)
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("Sampling range minimum (%f) is greated than max (%f). Setting min to be equal to max."), Range.Min, Range.Max)
		
		Range.Min = Range.Max;
	}

	return Range;
}
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseSequence
UObject* FPoseSearchDatabaseSequence::GetAnimationAsset() const
{
	return Sequence.Get();
}

#if WITH_EDITORONLY_DATA
UClass* FPoseSearchDatabaseSequence::GetAnimationAssetStaticClass() const
{
	return UAnimSequence::StaticClass();
}

bool FPoseSearchDatabaseSequence::IsLooping() const
{
	return Sequence &&
		Sequence->bLoop &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
}

bool FPoseSearchDatabaseSequence::IsRootMotionEnabled() const
{
	return Sequence ? Sequence->HasRootMotion() : false;
}
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseBlendSpace
UObject* FPoseSearchDatabaseBlendSpace::GetAnimationAsset() const
{
	return BlendSpace.Get();
}

float FPoseSearchDatabaseBlendSpace::GetPlayLength(const FVector& BlendParameters) const
{
	int32 TriangulationIndex = 0;
	TArray<FBlendSampleData> BlendSamples;
	BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true);
	const float PlayLength = BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);
	return PlayLength;
}

//#if WITH_EDITOR
//int32 FPoseSearchDatabaseBlendSpace::GetFrameAtTime(float Time) const
//{
//	// returning the percentage of time as value to diplay in the pose search debugger (NoTe: BlendSpace->GetPlayLength() is one)
//	return FMath::RoundToInt(Time * 100.f);
//}
//#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
UClass* FPoseSearchDatabaseBlendSpace::GetAnimationAssetStaticClass() const
{
	return UBlendSpace::StaticClass();
}

bool FPoseSearchDatabaseBlendSpace::IsLooping() const
{
	return BlendSpace && BlendSpace->bLoop;
}

bool FPoseSearchDatabaseBlendSpace::IsRootMotionEnabled() const
{
	if (BlendSpace)
	{
		return UE::PoseSearch::IsBlendSpaceRootMotionEnabled(BlendSpace);
	}
	return false;
}

FFloatInterval FPoseSearchDatabaseBlendSpace::GetEffectiveSamplingRange(const FVector& BlendParameters) const
{
	if (BlendSpace)
	{
		const float PlayLength = GetPlayLength(BlendParameters);

		// scaling blend space SamplingRange from the space [0, 1] to [0, PlayLength] with PlayLength calculated from the BlendSamples
		FFloatInterval ScaledSamplingRange = SamplingRange;
		ScaledSamplingRange.Min *= PlayLength;
		ScaledSamplingRange.Max *= PlayLength;

		return FPoseSearchDatabaseAnimationAssetBase::GetEffectiveSamplingRange(PlayLength, ScaledSamplingRange);
	}

	return FFloatInterval(0.f, 0.f);
}

void FPoseSearchDatabaseBlendSpace::IterateOverSamplingParameter(const TFunction<void(const FVector& BlendParameters)>& ProcessSamplingParameter) const
{
	if (BlendSpace)
	{
		UE::PoseSearch::IterateOverBlendSpaceSamplingParameter(BlendSpace, bUseSingleSample, FVector(BlendParamX, BlendParamY, 0.f),
			bUseGridForSampling, NumberOfHorizontalSamples, NumberOfVerticalSamples, ProcessSamplingParameter);
	}
}

void FPoseSearchDatabaseBlendSpace::GetBlendSpaceParameterSampleRanges(int32& HorizontalBlendNum, int32& VerticalBlendNum) const
{
	check(BlendSpace);

	if (bUseSingleSample)
	{
		HorizontalBlendNum = 1;
		VerticalBlendNum = 1;
	}
	else if (bUseGridForSampling)
	{
		HorizontalBlendNum = BlendSpace->GetBlendParameter(0).GridNum + 1;
		VerticalBlendNum = BlendSpace->IsA<UBlendSpace1D>() ? 1 : BlendSpace->GetBlendParameter(1).GridNum + 1;
	}
	else
	{
		HorizontalBlendNum = FMath::Max(NumberOfHorizontalSamples, 1);
		VerticalBlendNum = BlendSpace->IsA<UBlendSpace1D>() ? 1 : FMath::Max(NumberOfVerticalSamples, 1);
	}

	check(HorizontalBlendNum >= 1 && VerticalBlendNum >= 1);
}

FVector FPoseSearchDatabaseBlendSpace::BlendParameterForSampleRanges(int32 HorizontalBlendIndex, int32 VerticalBlendIndex) const
{
	check(BlendSpace);

	if (bUseSingleSample)
	{
		check(HorizontalBlendIndex == 0 && VerticalBlendIndex == 0);
		return FVector(BlendParamX, BlendParamY, 0.f);
	}
	
	const bool bWrapInputOnHorizontalAxis = BlendSpace->GetBlendParameter(0).bWrapInput;
	const bool bWrapInputOnVerticalAxis = BlendSpace->GetBlendParameter(1).bWrapInput;

	int32 HorizontalBlendNum, VerticalBlendNum;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GetBlendSpaceParameterSampleRanges(HorizontalBlendNum, VerticalBlendNum);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (bWrapInputOnHorizontalAxis)
	{
		++HorizontalBlendNum;
	}

	if (bWrapInputOnVerticalAxis)
	{
		++VerticalBlendNum;
	}

	const float HorizontalBlendMin = BlendSpace->GetBlendParameter(0).Min;
	const float HorizontalBlendMax = BlendSpace->GetBlendParameter(0).Max;

	const float VerticalBlendMin = BlendSpace->GetBlendParameter(1).Min;
	const float VerticalBlendMax = BlendSpace->GetBlendParameter(1).Max;

	return FVector(
		HorizontalBlendNum > 1 ? 
			HorizontalBlendMin + (HorizontalBlendMax - HorizontalBlendMin) * 
			((float)HorizontalBlendIndex) / (HorizontalBlendNum - 1) : 
		HorizontalBlendMin,
		VerticalBlendNum > 1 ? 
			VerticalBlendMin + (VerticalBlendMax - VerticalBlendMin) * 
			((float)VerticalBlendIndex) / (VerticalBlendNum - 1) : 
		VerticalBlendMin,
		0.f);
}

#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAnimComposite
UObject* FPoseSearchDatabaseAnimComposite::GetAnimationAsset() const
{
	return AnimComposite.Get();
}

#if WITH_EDITORONLY_DATA
UClass* FPoseSearchDatabaseAnimComposite::GetAnimationAssetStaticClass() const
{
	return UAnimComposite::StaticClass();
}

bool FPoseSearchDatabaseAnimComposite::IsLooping() const
{
	return AnimComposite &&
		AnimComposite->bLoop &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
}

bool FPoseSearchDatabaseAnimComposite::IsRootMotionEnabled() const
{
	return AnimComposite ? AnimComposite->HasRootMotion() : false;
}
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseAnimMontage
UObject* FPoseSearchDatabaseAnimMontage::GetAnimationAsset() const
{
	return AnimMontage.Get();
}

#if WITH_EDITORONLY_DATA
UClass* FPoseSearchDatabaseAnimMontage::GetAnimationAssetStaticClass() const
{
	return UAnimMontage::StaticClass();
}

bool FPoseSearchDatabaseAnimMontage::IsLooping() const
{
	return AnimMontage &&
		AnimMontage->bLoop &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
}

bool FPoseSearchDatabaseAnimMontage::IsRootMotionEnabled() const
{
	return AnimMontage ? AnimMontage->HasRootMotion() : false;
}
#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// FPoseSearchDatabaseMultiAnimAsset
#if WITH_EDITOR
USkeletalMesh* FPoseSearchDatabaseMultiAnimAsset::GetPreviewMeshForRole(const UE::PoseSearch::FRole& Role) const
{
	return MultiAnimAsset ? MultiAnimAsset->GetPreviewMesh(Role) : nullptr;
}
#endif // WITH_EDITOR

UObject* FPoseSearchDatabaseMultiAnimAsset::GetAnimationAsset() const
{
	return MultiAnimAsset.Get();
}

float FPoseSearchDatabaseMultiAnimAsset::GetPlayLength(const FVector& BlendParameters) const
{
	return MultiAnimAsset ? MultiAnimAsset->GetPlayLength(BlendParameters) : 0.f;
}

int32 FPoseSearchDatabaseMultiAnimAsset::GetNumRoles() const
{
	return MultiAnimAsset ? MultiAnimAsset->GetNumRoles() : 0;
}

UE::PoseSearch::FRole FPoseSearchDatabaseMultiAnimAsset::GetRole(int32 RoleIndex) const
{
	return MultiAnimAsset ? MultiAnimAsset->GetRole(RoleIndex) : UE::PoseSearch::DefaultRole;
}

UAnimationAsset* FPoseSearchDatabaseMultiAnimAsset::GetAnimationAssetForRole(const UE::PoseSearch::FRole& Role) const
{
	return MultiAnimAsset ? MultiAnimAsset->GetAnimationAsset(Role) : nullptr;
}

FTransform FPoseSearchDatabaseMultiAnimAsset::GetRootTransformOriginForRole(const UE::PoseSearch::FRole& Role) const
{
	return MultiAnimAsset ? MultiAnimAsset->GetOrigin(Role) : FTransform::Identity;
}

#if WITH_EDITORONLY_DATA
UClass* FPoseSearchDatabaseMultiAnimAsset::GetAnimationAssetStaticClass() const
{
	return UMultiAnimAsset::StaticClass();
}

bool FPoseSearchDatabaseMultiAnimAsset::IsLooping() const
{
	return MultiAnimAsset &&
		MultiAnimAsset->IsLooping() &&
		SamplingRange.Min == 0.f &&
		SamplingRange.Max == 0.f;
}

bool FPoseSearchDatabaseMultiAnimAsset::IsRootMotionEnabled() const
{
	return MultiAnimAsset ? MultiAnimAsset->HasRootMotion() : false;
}

void FPoseSearchDatabaseMultiAnimAsset::IterateOverSamplingParameter(const TFunction<void(const FVector& BlendParameters)>& ProcessSamplingParameter) const
{
	if (MultiAnimAsset)
	{
		const UBlendSpace* BlendSpace = nullptr;
		for (int32 RoleIndex = 0; RoleIndex < MultiAnimAsset->GetNumRoles(); ++RoleIndex)
		{
			if (const UAnimationAsset* AnimationAsset = MultiAnimAsset->GetAnimationAsset(MultiAnimAsset->GetRole(RoleIndex)))
			{
				if (AnimationAsset->GetClass()->IsChildOf(UBlendSpace::StaticClass()))
				{
					// @todo: right now we just sample using the first blend space, but we should probably make sure if there're multiple blendspaces they are consistent with each other
					BlendSpace = Cast<UBlendSpace>(AnimationAsset);
					check(BlendSpace);
					break;
				}
			}
		}

		if (BlendSpace)
		{
			UE::PoseSearch::IterateOverBlendSpaceSamplingParameter(BlendSpace, false, FVector::ZeroVector,
				false, NumberOfHorizontalSamples, NumberOfVerticalSamples, ProcessSamplingParameter);
		}
		else
		{
			ProcessSamplingParameter(FVector::ZeroVector);
		}
	}
}

#endif // WITH_EDITORONLY_DATA

//////////////////////////////////////////////////////////////////////////
// UPoseSearchDatabase
PRAGMA_DISABLE_DEPRECATION_WARNINGS
UPoseSearchDatabase::~UPoseSearchDatabase()
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UPoseSearchDatabase::SetSearchIndex(const UE::PoseSearch::FSearchIndex& SearchIndex)
{
	check(IsInGameThread());
	SearchIndexPrivate = SearchIndex;

	UpdateCachedProperties();
}

void UPoseSearchDatabase::UpdateCachedProperties()
{
	using namespace UE::PoseSearch;

	CachedAssetMap.Reset();
	for (int32 AssetIdx = 0; AssetIdx != SearchIndexPrivate.Assets.Num(); ++AssetIdx)
	{
		const FSearchIndexAsset& SearchIndexAsset = SearchIndexPrivate.Assets[AssetIdx];

		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(SearchIndexAsset))
		{
			CachedAssetMap.FindOrAdd(DatabaseAnimationAssetBase->GetAnimationAsset()).Add(AssetIdx);
		}
	}

	for (TPair<FObjectKey, TArray<int32>>& CachedAssetMapPair : CachedAssetMap)
	{
		CachedAssetMapPair.Value.Sort();
	}
}

TConstArrayView<int32> UPoseSearchDatabase::GetAssetIndexesForSourceAsset(const UObject* SourceAsset) const
{
	using namespace UE::PoseSearch;

	if (const TArray<int32>* IndexesForSourceAsset = CachedAssetMap.Find(SourceAsset))
	{
#if DO_CHECK
		// validating the consistency of IndexesForSourceAsset retrieved from SourceAsset
		const FSearchIndex& SearchIndex = GetSearchIndex();
		for (int32 AssetIndex : *IndexesForSourceAsset)
		{
			const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIndex];
			const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(SearchIndexAsset);
	
			// if those checks fail the calling code hasn't been protected by FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex
			check(DatabaseAnimationAssetBase);
			check(DatabaseAnimationAssetBase->GetAnimationAsset() == SourceAsset);
		}
#endif // DO_CHECK

		return *IndexesForSourceAsset;
	}
	return TConstArrayView<int32>();
}

TConstArrayView<float> UPoseSearchDatabase::CalculateDynamicWeightsSqrt(TArrayView<float> DynamicWeightsSqrtBuffer) const
{
	using namespace UE::PoseSearch;
	check(IsAligned(DynamicWeightsSqrtBuffer.GetData(), alignof(VectorRegister4Float)));

	const UE::PoseSearch::FSearchIndex& SearchIndex = GetSearchIndex();
	check(DynamicWeightsSqrtBuffer.Num() == SearchIndex.WeightsSqrt.Num());

	bool bInitialized = false;
	Schema->IterateChannels([&bInitialized, &SearchIndex, &DynamicWeightsSqrtBuffer](const UPoseSearchFeatureChannel* Channel)
		{
			if (const UPoseSearchFeatureChannel_GroupBase* ChannelGroupBase = Cast<UPoseSearchFeatureChannel_GroupBase>(Channel))
			{
				if (ChannelGroupBase->DebugWeightGroupID != INDEX_NONE && ChannelGroupBase->DebugWeightGroupID != GVarMotionMatchDebugWeightGroupID)
				{
					if (!bInitialized)
					{
						// initializing DynamicWeightsSqrtBuffer with the SearchIndex.WeightsSqrt
						FMemory::Memcpy(DynamicWeightsSqrtBuffer.GetData(), SearchIndex.WeightsSqrt.GetData(), SearchIndex.WeightsSqrt.Num() * sizeof(float));
						bInitialized = true;
					}

					// zeroing out interval of weights in DynamicWeightsSqrtBuffer associated with ChannelGroupBase since its GroupID mismatch ValidWeightChannelGroup
					FMemory::Memzero(DynamicWeightsSqrtBuffer.GetData() + ChannelGroupBase->GetChannelDataOffset(), ChannelGroupBase->GetChannelCardinality() * sizeof(float));
				}
			}
		});

	if (bInitialized)
	{
		return DynamicWeightsSqrtBuffer;
	}

	return SearchIndex.WeightsSqrt;
}

#if WITH_EDITORONLY_DATA
void UPoseSearchDatabase::AppendToClassSchema(FAppendToClassSchemaContext& Context)
{
	using namespace UE::PoseSearch;

	Super::AppendToClassSchema(Context);
	
	Context.Update(&DatabaseIndexDerivedDataCacheKeyVersion, sizeof(DatabaseIndexDerivedDataCacheKeyVersion));
	Context.Update(&FDevSystemGuids::Get().POSESEARCHDB_DERIVEDDATA_VER, sizeof(FDevSystemGuids::Get().POSESEARCHDB_DERIVEDDATA_VER));
}
#endif // WITH_EDITORONLY_DATA

const UE::PoseSearch::FSearchIndex& UPoseSearchDatabase::GetSearchIndex() const
{
	// making sure the search index is consistent. if it fails the calling code hasn't been protected by FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex
	check(Schema && !SearchIndexPrivate.IsEmpty() && SearchIndexPrivate.GetNumDimensions() == Schema->SchemaCardinality);
	return SearchIndexPrivate;
}

int32 UPoseSearchDatabase::GetPoseIndexFromTime(float RealTimeInSeconds, const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const
{
	return SearchIndexAsset.GetPoseIndexFromTime(RealTimeInSeconds, Schema->SampleRate);
}

int32 UPoseSearchDatabase::GetPoseIndex(const UObject* AnimationAsset, float AnimationAssetTime, bool bMirrored, const FVector& BlendParameters) const
{
	using namespace UE::PoseSearch;

	int32 PoseIdx = INDEX_NONE;

	if (AnimationAsset)
	{
		float MinSquaredLength = UE_MAX_FLT;
		const float SampleRate = Schema->SampleRate;
		const TConstArrayView<int32> AssetIndexesForSourceAsset = GetAssetIndexesForSourceAsset(AnimationAsset);

		const FSearchIndex& SearchIndex = GetSearchIndex();
		for (int32 AssetIndex : AssetIndexesForSourceAsset)
		{
			const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIndex];
			if (SearchIndexAsset.IsMirrored() == bMirrored)
			{
				const float BlendParametersSquaredLength = (BlendParameters - SearchIndexAsset.GetBlendParameters()).SquaredLength();

				// using <= so we don't have to check for PoseIdx == INDEX_NONE, since any float will be smaller or equal than UE_MAX_FLT
				if (BlendParametersSquaredLength <= MinSquaredLength)
				{
					MinSquaredLength = BlendParametersSquaredLength;

					const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(SearchIndexAsset);

					check(DatabaseAnimationAssetBase);
					check(DatabaseAnimationAssetBase->GetAnimationAsset() == AnimationAsset);

					const float RealAssetTime = AnimationAssetTime * SearchIndexAsset.GetToRealTimeFactor();
					PoseIdx = SearchIndexAsset.GetPoseIndexFromTime(RealAssetTime, SampleRate);
				}
			}
		}
	}

	return PoseIdx;
}

void UPoseSearchDatabase::AddAnimationAsset(FInstancedStruct AnimationAsset)
{
	AnimationAssets.Add(AnimationAsset);
}

void UPoseSearchDatabase::RemoveAnimationAssetAt(int32 AnimationAssetIndex)
{
	AnimationAssets.RemoveAt(AnimationAssetIndex);
}

#if WITH_EDITOR
int32 UPoseSearchDatabase::GetNumberOfPrincipalComponents() const
{
	return FMath::Min<int32>(NumberOfPrincipalComponents, Schema->SchemaCardinality);
}
#endif //WITH_EDITOR

bool UPoseSearchDatabase::GetSkipSearchIfPossible() const
{
#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	if (UE::PoseSearch::GVarMotionMatchCompareAgainstBruteForce)
	{
		return false;
	}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	return true;
}

void UPoseSearchDatabase::PostLoad()
{
#if WITH_EDITOR
	using namespace UE::PoseSearch;

	// todo: should we SynchronizeWithExternalDependencies() here?
	SynchronizeChooser();

	// Delay indexing until BeginCacheForCookedPlatformData when running a CookCommandlet
	if (!IsRunningCookCommandlet())
	{
		ERequestAsyncBuildFlag Flag = ERequestAsyncBuildFlag::NewRequest;
#if WITH_ENGINE
		// @todo: after CL 39338693 waiting for completion on a UPoseSearchDatabase is no longer possible,
		//        because UAnimSequence are not implemented to wait for their DDC tasks (IAnimSequenceCompilingManager::FinishCompilation) in their PostLoad
		//        leading FKeyBuilder::TryAddDependency not being able to compose a DDC key for the database, therefore not being able to complete the indexing.
		// 
		// If there isn't an EditorEngine (ex. Standalone Game via -game argument) we WaitForCompletion
		//if (Cast<UEditorEngine>(GEngine) == nullptr)
		//{
		//	Flag |= ERequestAsyncBuildFlag::WaitForCompletion;
		//}
#endif // WITH_ENGINE
	
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, Flag);
	}
#endif

	Super::PostLoad();
}

bool UPoseSearchDatabase::Contains(const UObject* Object) const
{
	return !GetAssetIndexesForSourceAsset(Object).IsEmpty();
}

int32 UPoseSearchDatabase::GetNumAnimationAssets() const
{
	return AnimationAssets.Num();
}

UObject* UPoseSearchDatabase::GetAnimationAsset(int32 Index) const
{
	if (AnimationAssets.IsValidIndex(Index))
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* AnimationAssetBase = AnimationAssets[Index].GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
		{
			return AnimationAssetBase->GetAnimationAsset();
		}
	}
	return nullptr;
}

#if WITH_EDITOR
void UPoseSearchDatabase::SynchronizeWithExternalDependencies()
{
	TArray<FTopLevelAssetPath> AncestorClassNames;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(GetPackage()->GetFName(), Referencers);

	// Sort AssetRegistry results
	Algo::Sort(Referencers, [](FAssetIdentifier One, FAssetIdentifier Two) -> bool
	{
		return One.PackageName.Compare(Two.PackageName) < 0;
	});

	TArray<UAnimSequenceBase*> SequencesBase;
	for (const FAssetIdentifier& Referencer : Referencers)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(Referencer.PackageName, Assets);

		for (const FAssetData& Asset : Assets)
		{
			if (Asset.IsInstanceOf(UAnimSequenceBase::StaticClass()))
			{
				if (UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(Asset.FastGetAsset(true)))
				{
					for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
					{
						if (const UAnimNotifyState_PoseSearchBranchIn* BranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
						{
							if (BranchIn->Database == this)
							{
								SequencesBase.AddUnique(SequenceBase);
								break;
							}
						}
					}
				}
			}
		}
	}

	if (!SequencesBase.IsEmpty())
	{
		SynchronizeWithExternalDependencies(SequencesBase);
	}
}

void UPoseSearchDatabase::SynchronizeWithExternalDependencies(TConstArrayView<UAnimSequenceBase*> SequencesBase)
{
	// cannot use TSet since FInstancedStruct doesn't implement GetTypeHash
	TArray<FInstancedStruct> NewAnimationAssets;

	// collecting all the database AnimationAsset(s) that don't require synchronization
	
	for (FInstancedStruct& AnimationAsset : AnimationAssets)
	{
		FPoseSearchDatabaseAnimationAssetBase& AnimationAssetBase = AnimationAsset.GetMutable<FPoseSearchDatabaseAnimationAssetBase>();

		const bool bRequiresSynchronization = AnimationAssetBase.IsSynchronizedWithExternalDependency() && SequencesBase.Contains(AnimationAssetBase.GetAnimationAsset());
		if (!bRequiresSynchronization)
		{
			NewAnimationAssets.Add(AnimationAsset);
		}
	}

	// collecting all the SequencesBase(s) requiring synchronization
	for (UAnimSequenceBase* SequenceBase : SequencesBase)
	{
		if (SequenceBase)
		{
			for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
			{
				if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
				{
					if (PoseSearchBranchIn->Database == this)
					{
						auto GetSamplingRange = [](const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* SequenceBase) -> FFloatInterval
						{
							FFloatInterval SamplingRange(NotifyEvent.GetTime(), NotifyEvent.GetTime() + NotifyEvent.GetDuration());
							if (SamplingRange.Min <= NotifyEvent.TriggerTimeOffset && SamplingRange.Max >= SequenceBase->GetPlayLength() - NotifyEvent.TriggerTimeOffset)
							{
								SamplingRange = FFloatInterval(0.f, 0.f);
							}
							return SamplingRange;
						};

						if (UAnimSequence* Sequence = Cast<UAnimSequence>(SequenceBase))
						{
							FPoseSearchDatabaseSequence DatabaseSequence;
							DatabaseSequence.Sequence = Sequence;
							DatabaseSequence.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseSequence.BranchInId = PoseSearchBranchIn->GetBranchInId();
							NewAnimationAssets.Add(FInstancedStruct::Make(DatabaseSequence));
						}
						else if (UAnimComposite* AnimComposite = Cast<UAnimComposite>(SequenceBase))
						{
							FPoseSearchDatabaseAnimComposite DatabaseAnimComposite;
							DatabaseAnimComposite.AnimComposite = AnimComposite;
							DatabaseAnimComposite.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseAnimComposite.BranchInId = PoseSearchBranchIn->GetBranchInId();
							NewAnimationAssets.Add(FInstancedStruct::Make(DatabaseAnimComposite));
						}
						else if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(SequenceBase))
						{
							FPoseSearchDatabaseAnimMontage DatabaseAnimMontage;
							DatabaseAnimMontage.AnimMontage = AnimMontage;
							DatabaseAnimMontage.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseAnimMontage.BranchInId = PoseSearchBranchIn->GetBranchInId();
							NewAnimationAssets.Add(FInstancedStruct::Make(DatabaseAnimMontage));
						}
					}
				}
			}
		}
	}

	// updating AnimationAssets from NewAnimationAssets preserving the original sorting
	bool bModified = false;
	for (int32 AnimationAssetIndex = GetNumAnimationAssets() - 1; AnimationAssetIndex >= 0; --AnimationAssetIndex)
	{
		FPoseSearchDatabaseAnimationAssetBase& AnimationAsset = AnimationAssets[AnimationAssetIndex].GetMutable<FPoseSearchDatabaseAnimationAssetBase>();
		
		int32 FoundIndex = -1;
		for(int i=0; i < NewAnimationAssets.Num(); i++)
		{
			const FPoseSearchDatabaseAnimationAssetBase& NewAnimationAsset = NewAnimationAssets[i].Get<FPoseSearchDatabaseAnimationAssetBase>();
			if (AnimationAsset.UpdateFrom(NewAnimationAsset))
			{
				FoundIndex = i;
				break;
			}
			
		}
		
		if (FoundIndex >= 0)
		{
			NewAnimationAssets.RemoveAt(FoundIndex); 
		}
		else
		{
			AnimationAssets.RemoveAt(AnimationAssetIndex);
			bModified = true;
		}
	}

	// adding the remaining AnimationAsset(s) from AnimationAssetsSet
	for (const FInstancedStruct& AnimationAsset : NewAnimationAssets)
	{
		AnimationAssets.Add(AnimationAsset);
		bModified = true;
	}

	if (bModified)
	{
		Modify();
		NotifySynchronizeWithExternalDependencies();
	}
}

const UChooserTable* UPoseSearchDatabase::GetChooser() const
{
	return Chooser;
}

void UPoseSearchDatabase::SynchronizeChooser()
{
	if (Chooser)
	{
		TArray<UObject*, TInlineAllocator<128>> ChooserAssets;
		UChooserTable::IterateChooser(Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&ChooserAssets](UObject* Object)
			{
				if (Object)
				{
					if (Object->GetClass()->IsChildOf(UAnimSequence::StaticClass()) ||
						Object->GetClass()->IsChildOf(UAnimComposite::StaticClass()) ||
						Object->GetClass()->IsChildOf(UAnimMontage::StaticClass()) ||
						Object->GetClass()->IsChildOf(UBlendSpace::StaticClass()) ||
						Object->GetClass()->IsChildOf(UMultiAnimAsset::StaticClass()))
					{
						ChooserAssets.Add(Object);
					}
				}
				return FObjectChooserBase::EIteratorStatus::Continue;
			}));

		TArray<UObject*, TInlineAllocator<128>> DatabaseAssets;
		for (FInstancedStruct& AnimationAsset : AnimationAssets)
		{
			if (const FPoseSearchDatabaseAnimationAssetBase* AnimationAssetBase = AnimationAsset.GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
			{
				DatabaseAssets.Add(AnimationAssetBase->GetAnimationAsset());
			}
		}

		if (ChooserAssets != DatabaseAssets)
		{
			TArray<FInstancedStruct> AnimationAssetsBackup = MoveTemp(AnimationAssets);
		
			for (UObject* ChooserAsset : ChooserAssets)
			{
				// searching ChooserAsset in AnimationAssetsBackup
				bool bFoundInAnimationAssetsBackup = false;
				for (int32 AnimationAssetBackupIndex = 0; AnimationAssetBackupIndex < AnimationAssetsBackup.Num(); ++AnimationAssetBackupIndex)
				{
					if (const FPoseSearchDatabaseAnimationAssetBase* AnimationAssetBase = AnimationAssetsBackup[AnimationAssetBackupIndex].GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
					{
						if (AnimationAssetBase->GetAnimationAsset() == ChooserAsset)
						{
							bFoundInAnimationAssetsBackup = true;
							AnimationAssets.Add(AnimationAssetsBackup[AnimationAssetBackupIndex]);
							AnimationAssetsBackup.RemoveAt(AnimationAssetBackupIndex);
							break;
						}
					}
				}

				if (!bFoundInAnimationAssetsBackup)
				{
					if (UAnimSequence* Sequence = Cast<UAnimSequence>(ChooserAsset))
					{
						FPoseSearchDatabaseSequence DatabaseSequence;
						DatabaseSequence.Sequence = Sequence;
						AnimationAssets.Add(FInstancedStruct::Make(DatabaseSequence));
					}
					else if (UAnimComposite* AnimComposite = Cast<UAnimComposite>(ChooserAsset))
					{
						FPoseSearchDatabaseAnimComposite DatabaseAnimComposite;
						DatabaseAnimComposite.AnimComposite = AnimComposite;
						AnimationAssets.Add(FInstancedStruct::Make(DatabaseAnimComposite));
					}
					else if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(ChooserAsset))
					{
						FPoseSearchDatabaseAnimMontage DatabaseAnimMontage;
						DatabaseAnimMontage.AnimMontage = AnimMontage;
						AnimationAssets.Add(FInstancedStruct::Make(DatabaseAnimMontage));
					}
					else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(ChooserAsset))
					{
						FPoseSearchDatabaseBlendSpace DatabaseBlendSpace;
						DatabaseBlendSpace.BlendSpace = BlendSpace;
						AnimationAssets.Add(FInstancedStruct::Make(DatabaseBlendSpace));
					}
					else if (UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(ChooserAsset))
					{
						FPoseSearchDatabaseMultiAnimAsset DatabaseMultiAnimAsset;
						DatabaseMultiAnimAsset.MultiAnimAsset = MultiAnimAsset;
						AnimationAssets.Add(FInstancedStruct::Make(DatabaseMultiAnimAsset));
					}
				}
			}

			Modify();
			NotifySynchronizeWithExternalDependencies();
		}
	}
}

void UPoseSearchDatabase::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	using namespace UE::PoseSearch;
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::NewRequest);
}

bool UPoseSearchDatabase::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	using namespace UE::PoseSearch;
	check(IsInGameThread());
	return EAsyncBuildIndexResult::InProgress != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest);
}
#endif // WITH_EDITOR

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
void UPoseSearchDatabase::TestSynchronizeWithExternalDependencies()
{
	TArray<FInstancedStruct> AnimationAssetsCopy = AnimationAssets;
	SynchronizeWithExternalDependencies();

	if (AnimationAssetsCopy != AnimationAssets)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("TestSynchronizeWithExternalDependencies failed"));
		AnimationAssets = AnimationAssetsCopy;
	}
}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

void UPoseSearchDatabase::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
#if WITH_EDITOR
	// in case the database desynchronized with the UAnimNotifyState_PoseSearchBranchIn referencing it, we need to resyncrhonize
	SynchronizeWithExternalDependencies();
#endif

	Super::PreSaveRoot(ObjectSaveContext);
}

void UPoseSearchDatabase::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	if (!IsTemplate() && !ObjectSaveContext.IsProceduralSave())
	{
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
	}
#endif

	Super::PostSaveRoot(ObjectSaveContext);
}

void UPoseSearchDatabase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!IsTemplate())
	{
		const bool bSavingCooked = Ar.IsSaving() && Ar.IsCooking();
		const bool bLoadingCooked = Ar.IsLoadingFromCookedPackage();
		if (bSavingCooked || bLoadingCooked)
		{
			Ar << SearchIndexPrivate;
			UpdateCachedProperties();
		}
	}
}

float UPoseSearchDatabase::GetRealAssetTime(int32 PoseIdx) const
{
	check(Schema);
	const UE::PoseSearch::FSearchIndexAsset& Asset = GetSearchIndex().GetAssetForPose(PoseIdx);
	return Asset.GetTimeFromPoseIndex(PoseIdx, Schema->SampleRate);
}

float UPoseSearchDatabase::GetNormalizedAssetTime(int32 PoseIdx) const
{
	check(Schema);
	const UE::PoseSearch::FSearchIndexAsset& Asset = GetSearchIndex().GetAssetForPose(PoseIdx);
	const float ToRealTimeFactor = Asset.GetToRealTimeFactor();
	check(ToRealTimeFactor > UE_KINDA_SMALL_NUMBER);
	const float RealAssetTime = Asset.GetTimeFromPoseIndex(PoseIdx, Schema->SampleRate);
	const float NormalizedAssetTime = RealAssetTime / ToRealTimeFactor;
	return NormalizedAssetTime;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::Search(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	FSearchResult Result;

#if WITH_EDITOR
	if (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest))
	{
		SearchContext.SetAsyncBuildIndexInProgress();
		return Result;
	}
#else
	if (SearchIndexPrivate.IsEmpty())
	{
		return Result;
	}
#endif // WITH_EDITOR

	// updating the SearchContext::AssetsToConsider from the UPoseSearchDatabase::Chooser evaluation 
	// (and restore them with the original one CurrentAssetToConsider at the end of the method)
	TConstArrayView<const UObject*> CurrentAssetToConsider = SearchContext.GetAssetsToConsider();
	TArray<const UObject*, TInlineAllocator<128>> ChooserAssetToConsider;
	if (Chooser && !SearchContext.GetContexts().IsEmpty())
	{
		if (!CurrentAssetToConsider.IsEmpty())
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchDatabase::Search - database (%s) has an associated Chooser filtering the search down, so previous SearchContext.GetAssetsToConsider() context will be skipped"), *GetName());
		}

		// @todo: add MM interaction support. Maybe iterate over all the SearchContext.GetAnimContexts and use the union of the ChosenAssets?
		UChooserTable::EvaluateChooser(*SearchContext.GetContexts()[0], Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([this, &ChooserAssetToConsider](UObject* InResult)
			{
				ChooserAssetToConsider.Add(InResult);
				return FObjectChooserBase::EIteratorStatus::Continue;
			}));

		SearchContext.SetAssetsToConsider(ChooserAssetToConsider);
	}

	if (SearchContext.GetEventToSearch().IsValid())
	{
		Result = SearchEvent(SearchContext);
	}
	else
	{
		switch (PoseSearchMode)
		{
		case EPoseSearchMode::BruteForce:
			Result = SearchBruteForce(SearchContext);
			break;
		case EPoseSearchMode::VPTree:
			Result = SearchVPTree(SearchContext);
			break;
		case EPoseSearchMode::PCAKDTree:
			Result = SearchPCAKDTree(SearchContext);
			break;
		case EPoseSearchMode::EventOnly:
			break;
		}
	}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	if (GVarMotionMatchCompareAgainstBruteForce)
	{
		if (PoseSearchMode == EPoseSearchMode::PCAKDTree || PoseSearchMode == EPoseSearchMode::VPTree)
		{
			Result.BruteForcePoseCost = SearchBruteForce(SearchContext).PoseCost;
		}
		else
		{
			Result.BruteForcePoseCost = Result.PoseCost;
		}
	}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED

#if UE_POSE_SEARCH_TRACE_ENABLED
	// in case we skipped the search, or we didn't find any candidates we still have to track we requested to evaluate this database, so we keep track of this
	SearchContext.Track(this);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	SearchContext.SetAssetsToConsider(CurrentAssetToConsider);

	return Result;
}

void UPoseSearchDatabase::PopulateNonSelectableIdx(FNonSelectableIdx& NonSelectableIdx, UE::PoseSearch::FSearchContext& SearchContext
#if UE_POSE_SEARCH_TRACE_ENABLED
	, float ContinuingPoseCostAddend, float ContinuingInteractionCostAddend, TConstArrayView<float> QueryValues, TConstArrayView<float> DynamicWeightsSqrt
#endif //UE_POSE_SEARCH_TRACE_ENABLED
) const
{
	using namespace UE::PoseSearch;

	const FSearchIndex& SearchIndex = GetSearchIndex();

#if UE_POSE_SEARCH_TRACE_ENABLED
	TArray<float> BufferUsedForReconstruction;
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	NonSelectableIdx.Reset();
	if (SearchContext.IsCurrentResultFromDatabase(this))
	{
		if (const FSearchIndexAsset* CurrentIndexAsset = SearchContext.GetCurrentResult().GetSearchIndexAsset(true))
		{
			if (CurrentIndexAsset->IsDisableReselection())
			{
				// excluding all the poses with CurrentIndexAsset->GetSourceAssetIdx()
				const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(CurrentIndexAsset->GetSourceAssetIdx());
				check(DatabaseAnimationAssetBase);

				for (int32 AssetIndex : GetAssetIndexesForSourceAsset(DatabaseAnimationAssetBase->GetAnimationAsset()))
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIndex];
					const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
					const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
					for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
					{
						// no need to AddUnique since there's no overlapping between pose indexes in the FSearchIndexAsset(s)
						NonSelectableIdx.Add(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
						const TConstArrayView<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseIdx, BufferUsedForReconstruction);
						const FPoseSearchCost PoseCost(CompareFeatureVectors<false>(PoseValues, QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[PoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
						SearchContext.Track(this, PoseIdx, EPoseCandidateFlags::DiscardedBy_AssetReselection, PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
					}
				}
			}
			else if (!FMath::IsNearlyEqual(SearchContext.GetPoseJumpThresholdTime().Min, SearchContext.GetPoseJumpThresholdTime().Max))
			{
				const int32 CurrentResultPoseIdx = SearchContext.GetCurrentResult().PoseIdx;
				const int32 UnboundMinPoseIdx = CurrentResultPoseIdx + FMath::FloorToInt(SearchContext.GetPoseJumpThresholdTime().Min * Schema->SampleRate);
				const int32 UnboundMaxPoseIdx = CurrentResultPoseIdx + FMath::CeilToInt(SearchContext.GetPoseJumpThresholdTime().Max * Schema->SampleRate);
				const int32 CurrentIndexAssetFirstPoseIdx = CurrentIndexAsset->GetFirstPoseIdx();
				const int32 CurrentIndexAssetNumPoses = CurrentIndexAsset->GetNumPoses();
				const bool bIsLooping = CurrentIndexAsset->IsLooping();

				if (bIsLooping)
				{
					for (int32 UnboundPoseIdx = UnboundMinPoseIdx; UnboundPoseIdx < UnboundMaxPoseIdx; ++UnboundPoseIdx)
					{
						const int32 Modulo = (UnboundPoseIdx - CurrentIndexAssetFirstPoseIdx) % CurrentIndexAssetNumPoses;
						const int32 CurrentIndexAssetFirstPoseIdxPlusModulo = CurrentIndexAssetFirstPoseIdx + Modulo;
						const int32 PoseIdx = Modulo >= 0 ? CurrentIndexAssetFirstPoseIdxPlusModulo : CurrentIndexAssetFirstPoseIdxPlusModulo + CurrentIndexAssetNumPoses;

						NonSelectableIdx.AddUnique(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
						const TConstArrayView<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseIdx, BufferUsedForReconstruction);
						const FPoseSearchCost PoseCost(CompareFeatureVectors<false>(PoseValues, QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[PoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
						SearchContext.Track(this, PoseIdx, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime, PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
					}
				}
				else
				{
					const int32 MinPoseIdx = FMath::Max(CurrentIndexAssetFirstPoseIdx, UnboundMinPoseIdx);
					const int32 MaxPoseIdx = FMath::Min(CurrentIndexAssetFirstPoseIdx + CurrentIndexAssetNumPoses, UnboundMaxPoseIdx);

					for (int32 PoseIdx = MinPoseIdx; PoseIdx < MaxPoseIdx; ++PoseIdx)
					{
						NonSelectableIdx.AddUnique(PoseIdx);

#if UE_POSE_SEARCH_TRACE_ENABLED
						const TConstArrayView<float> PoseValues = SearchIndex.GetPoseValuesSafe(PoseIdx, BufferUsedForReconstruction);
						const FPoseSearchCost PoseCost(CompareFeatureVectors<false>(PoseValues, QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[PoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
						SearchContext.Track(this, PoseIdx, EPoseCandidateFlags::DiscardedBy_PoseJumpThresholdTime, PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
					}
				}
			}
		}
	}

	if (SearchContext.GetPoseIndicesHistory())
	{
		const FObjectKey DatabaseKey(this);
		for (auto It = SearchContext.GetPoseIndicesHistory()->IndexToTime.CreateConstIterator(); It; ++It)
		{
			const FHistoricalPoseIndex& HistoricalPoseIndex = It.Key();
			if (HistoricalPoseIndex.DatabaseKey == DatabaseKey)
			{
				NonSelectableIdx.AddUnique(HistoricalPoseIndex.PoseIndex);

#if UE_POSE_SEARCH_TRACE_ENABLED
				check(HistoricalPoseIndex.PoseIndex >= 0);

				// if we're editing the database and removing assets it's possible that the PoseIndicesHistory contains invalid pose indexes
				if (HistoricalPoseIndex.PoseIndex < SearchIndex.GetNumPoses())
				{
					const FPoseSearchCost PoseCost(CompareFeatureVectors<false>(SearchIndex.GetPoseValuesSafe(HistoricalPoseIndex.PoseIndex, BufferUsedForReconstruction), QueryValues, DynamicWeightsSqrt), SearchIndex.PoseMetadata[HistoricalPoseIndex.PoseIndex].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
					SearchContext.Track(this, HistoricalPoseIndex.PoseIndex, EPoseCandidateFlags::DiscardedBy_PoseReselectHistory, PoseCost);
				}
#endif // UE_POSE_SEARCH_TRACE_ENABLED
			}
		}
	}

	NonSelectableIdx.Sort();
}

void UPoseSearchDatabase::PopulateSelectableAssetIdx(FSelectableAssetIdx& SelectableAssetIdx, TConstArrayView<const UObject*> AssetsToConsider) const
{
	SelectableAssetIdx.Reset();
	if (!AssetsToConsider.IsEmpty())
	{
		for (const UObject* AssetToConsider : AssetsToConsider)
		{
			SelectableAssetIdx.Append(GetAssetIndexesForSourceAsset(AssetToConsider));
		}

		if (!SelectableAssetIdx.IsEmpty())
		{
			if (SelectableAssetIdx.Num() != GetSearchIndex().Assets.Num())
			{
				SelectableAssetIdx.Sort();
			}
			else
			{
				// SelectableAssetIdx contains ALL the Database->GetSearchIndex().Assets. 
				// We reset SelectableAssetIdx since it has the same meaning, and it'll perform better
				SelectableAssetIdx.Reset();
			}
		}
	}
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchContinuingPose(UE::PoseSearch::FSearchContext& SearchContext) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_ContinuingPose);

	using namespace UE::PoseSearch;

	check(SearchContext.GetCurrentResult().Database.Get() == this);

	FSearchResult Result;
	Result.bIsContinuingPoseSearch = true;

#if WITH_EDITOR
	if (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(this, ERequestAsyncBuildFlag::ContinueRequest))
	{
		SearchContext.SetAsyncBuildIndexInProgress();
		return Result;
	}
#endif // WITH_EDITOR

	// extracting notifies from the database animation asset at time SampleTime to search for UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias eventually overriding the database ContinuingPoseCostBias
	const FSearchIndex& SearchIndex = GetSearchIndex();
	const int32 PoseIdx = SearchContext.GetCurrentResult().PoseIdx;
	const FSearchIndexAsset& SearchIndexAsset = SearchIndex.GetAssetForPose(PoseIdx);
	const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(SearchIndexAsset);
	check(DatabaseAnimationAssetBase);

	float ContinuingPoseCostAddend = ContinuingPoseCostBias;
	const float SampleTime = GetRealAssetTime(PoseIdx);
	FAnimNotifyContext PreAllocatedNotifyContext;
	for (int32 RoleIndex = 0; RoleIndex < DatabaseAnimationAssetBase->GetNumRoles(); ++RoleIndex)
	{
		if (const UAnimationAsset* AnimationAsset = DatabaseAnimationAssetBase->GetAnimationAssetForRole(DatabaseAnimationAssetBase->GetRole(RoleIndex)))
		{
			// sampler used only to extract the notify states. RootTransformOrigin can be set as Identity, since will not be relevant
			const FAnimationAssetSampler SequenceBaseSampler(AnimationAsset, FTransform::Identity, SearchIndexAsset.GetBlendParameters(), FAnimationAssetSampler::DefaultRootTransformSamplingRate, false, false);

			bool bDone = false;
			SequenceBaseSampler.ExtractAnimNotifyStates(SampleTime, PreAllocatedNotifyContext, [&ContinuingPoseCostAddend, &bDone](const UAnimNotifyState* AnimNotifyState)
				{
					if (const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias* NotifyStateContinuingPoseCostBias = Cast<const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>(AnimNotifyState))
					{
						ContinuingPoseCostAddend = NotifyStateContinuingPoseCostBias->CostAddend;
						bDone = true;
						return false;
					}
					return true;
				});

			if (bDone)
			{
				break;
			}
		}
	}

	const float ContinuingInteractionCostAddend = SearchContext.IsContinuingInteraction() ? ContinuingInteractionCostBias : 0.f;

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend + ContinuingPoseCostAddend + ContinuingInteractionCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > (SearchIndex.MinCostAddend + ContinuingPoseCostAddend + ContinuingInteractionCostAddend))
	{
		const int32 NumDimensions = Schema->SchemaCardinality;
		// FMemory_Alloca is forced 16 bytes aligned
		TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
		const TConstArrayView<float> PoseValues = SearchIndex.IsValuesEmpty() ? SearchIndex.GetReconstructedPoseValues(PoseIdx, ReconstructedPoseValuesBuffer) : SearchIndex.GetPoseValues(PoseIdx);

		// @todo: perhaps store the DynamicWeightsSqrt into the GetOrBuildQuery to share between the continuing pose and the full search
		TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

		const int32 ContinuingPoseIdx = SearchContext.GetCurrentResult().PoseIdx;
		// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
		if (NumDimensions % 4 == 0)
		{
			Result.PoseCost = FPoseSearchCost(CompareFeatureVectors<true>(PoseValues, SearchContext.GetOrBuildQuery(Schema), DynamicWeightsSqrt), SearchIndex.PoseMetadata[ContinuingPoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
		}
		// data is not 16 bytes padded
		else
		{
			Result.PoseCost = FPoseSearchCost(CompareFeatureVectors<false>(PoseValues, SearchContext.GetOrBuildQuery(Schema), DynamicWeightsSqrt), SearchIndex.PoseMetadata[ContinuingPoseIdx].GetCostAddend(), ContinuingPoseCostAddend, ContinuingInteractionCostAddend);
		}

		Result.AssetTime = SearchContext.GetCurrentResult().AssetTime;
		Result.PoseIdx = PoseIdx;
		Result.Database = this;

#if UE_POSE_SEARCH_TRACE_ENABLED
		SearchContext.Track(this, ContinuingPoseIdx, EPoseCandidateFlags::Valid_ContinuingPose, Result.PoseCost);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED
	if (GVarMotionMatchCompareAgainstBruteForce)
	{
		Result.BruteForcePoseCost = Result.PoseCost;
	}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG && UE_POSE_SEARCH_TRACE_ENABLED

	check(Result.DebugValidate());
	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchPCAKDTree(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_PCAKNN);

	using namespace UE::PoseSearch;

	FSearchResult Result;

	const int32 NumDimensions = Schema->SchemaCardinality;
	const FSearchIndex& SearchIndex = GetSearchIndex();
	const float ContinuingPoseCostAddend = 0.f;
	const float ContinuingInteractionCostAddend = SearchContext.IsContinuingInteraction() ? ContinuingInteractionCostBias : 0.f;

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		const uint32 ClampedNumberOfPrincipalComponents = SearchIndex.GetNumberOfPrincipalComponents();
		const uint32 ClampedKDTreeQueryNumNeighbors = FMath::Clamp<uint32>(KDTreeQueryNumNeighbors, 1, SearchIndex.GetNumPoses());
		const bool bArePCAValuesPruned = SearchIndex.PCAValuesVectorToPoseIndexes.Num() > 0;

		//stack allocated temporaries
		TArrayView<float> ProjectedQueryValues((float*)FMemory_Alloca(ClampedNumberOfPrincipalComponents * sizeof(float)), ClampedNumberOfPrincipalComponents);
	
		// @todo: perhaps store the DynamicWeightsSqrt into the GetOrBuildQuery to share between the continuing pose and the full search
		TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		check(QueryValues.Num() == NumDimensions);

		FSelectableAssetIdx SelectableAssetIdx;
		PopulateSelectableAssetIdx(SelectableAssetIdx, SearchContext.GetAssetsToConsider());

		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext
#if UE_POSE_SEARCH_TRACE_ENABLED
			, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, QueryValues, DynamicWeightsSqrt
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);

		bool bRunNonSelectableIdxPostKDTree = bArePCAValuesPruned;
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
		bRunNonSelectableIdxPostKDTree |= GVarMotionMatchValidateKNNSearch;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

		// projecting QueryValues into the PCA space 
		TConstArrayView<float> PCAQueryValues = SearchIndex.PCAProject(QueryValues, ProjectedQueryValues);
		check(PCAQueryValues.Num() == ClampedNumberOfPrincipalComponents);

		int32 NumResults = 0;

		TArrayView<FKDTree::FKNNMaxHeapResultSet::FResult> Results((FKDTree::FKNNMaxHeapResultSet::FResult*)FMemory_Alloca(ClampedKDTreeQueryNumNeighbors * sizeof(FKDTree::FKNNMaxHeapResultSet::FResult)), ClampedKDTreeQueryNumNeighbors);

#if WITH_EDITOR && ENABLE_ANIM_DEBUG
		const double StartTimeMaxHeap = GVarMotionMatchProfileMaxHeapKNNSearch ? FPlatformTime::Seconds() : 0;
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

		if (bRunNonSelectableIdxPostKDTree || NonSelectableIdx.IsEmpty())
		{
			FKDTree::FKNNMaxHeapResultSet ResultSet(Results);
			NumResults = SearchIndex.KDTree.FindNeighbors(ResultSet, PCAQueryValues);
		}
		else
		{
			FKDTree::FFilteredKNNMaxHeapResultSet ResultSet(Results, NonSelectableIdx);
			NumResults = SearchIndex.KDTree.FindNeighbors(ResultSet, PCAQueryValues);
		}

#if WITH_EDITOR && ENABLE_ANIM_DEBUG

		TArrayView<int32> ResultIndexes((int32*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(int32)), ClampedKDTreeQueryNumNeighbors + 1);
		TArrayView<float> ResultDistanceSqr((float*)FMemory_Alloca((ClampedKDTreeQueryNumNeighbors + 1) * sizeof(float)), ClampedKDTreeQueryNumNeighbors + 1);

		if (GVarMotionMatchProfileMaxHeapKNNSearch)
		{
			// debug code to log profiling comparison between FKNNResultSet/FFilteredKNNResultSet vs FKNNMaxHeapResultSet/FFilteredKNNMaxHeapResultSet
			const double EndTimeMaxHeap = FPlatformTime::Seconds();
			const double StartTimeRegular = EndTimeMaxHeap;
			
			if (bRunNonSelectableIdxPostKDTree || NonSelectableIdx.IsEmpty())
			{
				FKDTree::FKNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr);
				SearchIndex.KDTree.FindNeighbors(ResultSet, PCAQueryValues);
			}
			else
			{
				FKDTree::FFilteredKNNResultSet ResultSet(ClampedKDTreeQueryNumNeighbors, ResultIndexes, ResultDistanceSqr, NonSelectableIdx);
				SearchIndex.KDTree.FindNeighbors(ResultSet, PCAQueryValues);
			}
			const double EndTimeRegular(FPlatformTime::Seconds());

			const double DeltaTimeRegular = EndTimeRegular - StartTimeRegular;
			const double DeltaTimeMaxHeap = EndTimeMaxHeap - StartTimeMaxHeap;
			const int32 Winner = DeltaTimeRegular == DeltaTimeMaxHeap ? 0 : DeltaTimeRegular < DeltaTimeMaxHeap ? -1 : 1;
			UE_LOG(LogPoseSearch, Log, TEXT("Profiling: Regular(%f), MaxHeap(%f), Winner(%d)"), DeltaTimeRegular, DeltaTimeMaxHeap, Winner);
		}

		// SortedResultsIndexes contains the sorted indexes by Distance of the Results. This is because FKNNMaxHeapResultSet doesn't return a sorted array like FKNNResultSet does
		// and we need to feed EvaluatePoseKernel with the position in the search (from the KDTree in PCA space) of the best result position, 
		// to be able to plot a graph SCostTimelineView::BestPosePosView->CurveData->Points, useful to understand how to tune the KDTreeQueryNumNeighbors property:
		// if your graph NEVER shows a value higher than 50, than it's safe to set KDTreeQueryNumNeighbors to 50! Since lowering KDTreeQueryNumNeighbors will improve performances!
		TArrayView<int32> SortedResultsIndexes((int32*)FMemory_Alloca(NumResults * sizeof(int32)), NumResults);
		for (int32 Index = 0; Index < NumResults; ++Index)
		{
			SortedResultsIndexes[Index] = Index;
		}
		SortedResultsIndexes.Sort([&Results](const int32 IndexA, const int32 IndexB) { return Results[IndexA].Distance < Results[IndexB].Distance; });

		if (GVarMotionMatchProfileMaxHeapKNNSearch)
		{
			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				const int32 ExpectdIndex = ResultIndexes[ResultIndex];
				const int32 ActualIndex = Results[SortedResultsIndexes[ResultIndex]].Index;

				if (ExpectdIndex != ActualIndex)
				{
					const float ExpectdDistanceSqr = ResultDistanceSqr[ResultIndex];
					const float ActualDistanceSqr = Results[SortedResultsIndexes[ResultIndex]].Distance;
					UE_LOG(LogPoseSearch, Warning, TEXT("Inconsistent Result at index %d (%d-%d) (%f-%f)"), ResultIndex, ExpectdIndex, ActualIndex, ExpectdDistanceSqr, ActualDistanceSqr);
				}
			}
		}

		if (GVarMotionMatchValidateKNNSearch)
		{
			const int32 NumPCAValuesVectors = SearchIndex.GetNumPCAValuesVectors(ClampedNumberOfPrincipalComponents);

			TArray<TPair<int32, float>> PCAValueIndexCost;
			PCAValueIndexCost.SetNumUninitialized(NumPCAValuesVectors);

			// validating that the best n "ClampedKDTreeQueryNumNeighbors" are actually the best candidates
			for (int32 PCAValueIndex = 0; PCAValueIndex < NumPCAValuesVectors; ++PCAValueIndex)
			{
				PCAValueIndexCost[PCAValueIndex].Key = PCAValueIndex;
				PCAValueIndexCost[PCAValueIndex].Value = CompareFeatureVectors(SearchIndex.GetPCAPoseValues(PCAValueIndex), PCAQueryValues);
			}

			PCAValueIndexCost.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B)
				{
					return A.Value < B.Value;
				});

			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				if (PCAValueIndexCost[ResultIndex].Key != Results[ResultIndex].Index)
				{
					if (!FMath::IsNearlyEqual(PCAValueIndexCost[ResultIndex].Value, Results[ResultIndex].Distance, UE_KINDA_SMALL_NUMBER))
					{
						UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchDatabase::SearchPCAKDTree - KDTree search order is inconsistent with exaustive search in PCA space"));
					}
					else
					{
						UE_LOG(LogPoseSearch, Log, TEXT("UPoseSearchDatabase::SearchPCAKDTree - found two points at the same distance from the query in different order between KDTree and exaustive search"));
					}
				}
				else if (!FMath::IsNearlyEqual(PCAValueIndexCost[ResultIndex].Value, Results[ResultIndex].Distance, UE_KINDA_SMALL_NUMBER))
				{
					UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchDatabase::SearchPCAKDTree - KDTree search cost is inconsistent with exaustive search in PCA space"));
				}
			}
		}
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG

		// NonSelectableIdx are already filtered out inside the kdtree search.
		// Also kdtrees don't contain block transition poses by construction, so FSearchFilters input bAddBlockTransitionFilter can be set to false
		const FSearchFilters SearchFilters(Schema, bRunNonSelectableIdxPostKDTree ? NonSelectableIdx : TConstArrayView<int32>(), SelectableAssetIdx, false);
		
		// are the PCAValues pruned out of duplicates (multiple poses are associated with the same PCAValuesVectorIdx)
		if (bArePCAValuesPruned)
		{
			// @todo: reconstruction is not yet supported with pruned PCAValues
			check(!SearchIndex.IsValuesEmpty());
			
			const int32 MaxNumEvaluatePoseKernelCalls = KDTreeQueryNumNeighborsWithDuplicates > 0 ? KDTreeQueryNumNeighborsWithDuplicates : INT32_MAX;

			if (NumDimensions % 4 == 0)
			{
				int32 NumEvaluatePoseKernelCalls = 0;
				for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
				{
					const TConstArrayView<int32> PoseIndexes = SearchIndex.PCAValuesVectorToPoseIndexes[Results[ResultIndex].Index];
					for (int32 Index = 0; Index < PoseIndexes.Num() && NumEvaluatePoseKernelCalls < MaxNumEvaluatePoseKernelCalls; ++Index, ++NumEvaluatePoseKernelCalls)
					{
						EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIndexes[Index], INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
							, SortedResultsIndexes[ResultIndex]
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
					}
				}
			}
			else
			{
				int32 NumEvaluatePoseKernelCalls = 0;
				for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
				{
					const TConstArrayView<int32> PoseIndexes = SearchIndex.PCAValuesVectorToPoseIndexes[Results[ResultIndex].Index];
					for (int32 Index = 0; Index < PoseIndexes.Num() && NumEvaluatePoseKernelCalls < MaxNumEvaluatePoseKernelCalls; ++Index, ++NumEvaluatePoseKernelCalls)
					{
						EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIndexes[Index], INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
							, SortedResultsIndexes[ResultIndex]
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
					}
				}
			}
		}
		// do we need to reconstruct pose values?
		else if (SearchIndex.IsValuesEmpty())
		{
			// FMemory_Alloca is forced 16 bytes aligned
			TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
			check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				EvaluatePoseKernel<true, false>(Result, SearchIndex, QueryValues, ReconstructedPoseValuesBuffer, Results[ResultIndex].Index, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
					, SortedResultsIndexes[ResultIndex]
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
				);
			}
		}
		// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
		else if (NumDimensions % 4 == 0)
		{
			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), Results[ResultIndex].Index, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
					, SortedResultsIndexes[ResultIndex]
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
				);
			}
		}
		// no reconstruction, but data is not 16 bytes padded
		else
		{
			for (int32 ResultIndex = 0; ResultIndex < NumResults; ++ResultIndex)
			{
				EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), Results[ResultIndex].Index, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
					, SortedResultsIndexes[ResultIndex]
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// @todo: perhaps store the DynamicWeightsSqrt into the GetOrBuildQuery to share between the continuing pose and the full search
		TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, QueryValues, DynamicWeightsSqrt);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = GetNormalizedAssetTime(Result.PoseIdx);
		Result.Database = this;
	}

	check(Result.DebugValidate());
	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchVPTree(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_VPTree);

	using namespace UE::PoseSearch;

	FSearchResult Result;

	const FSearchIndex& SearchIndex = GetSearchIndex();
	const float ContinuingPoseCostAddend = 0.f;
	const float ContinuingInteractionCostAddend = SearchContext.IsContinuingInteraction() ? ContinuingInteractionCostBias : 0.f;

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		// @todo: perhaps store the DynamicWeightsSqrt into the GetOrBuildQuery to share between the continuing pose and the full search
		const int32 NumDimensions = Schema->SchemaCardinality;
		TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		check(QueryValues.Num() == NumDimensions);

		FSelectableAssetIdx SelectableAssetIdx;
		PopulateSelectableAssetIdx(SelectableAssetIdx, SearchContext.GetAssetsToConsider());

		// @todo: implement filtering within the VPTree as KDTree does
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext
#if UE_POSE_SEARCH_TRACE_ENABLED
			, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, QueryValues, DynamicWeightsSqrt
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);
		
		const FSearchFilters SearchFilters(Schema, NonSelectableIdx, SelectableAssetIdx, SearchIndex.bAnyBlockTransition);

		// @todo: implement a FVPTreeDataSource for aligned and padded features vector like CompareAlignedPoses does 
		FVPTreeDataSource DataSource(SearchIndex);
		FVPTreeResultSet ResultSet(KDTreeQueryNumNeighbors);
		SearchIndex.VPTree.FindNeighbors(QueryValues, ResultSet, DataSource);
		
		int32 NumEvaluatePoseKernelCalls = 0;
		const TConstArrayView<FIndexDistance> UnsortedResults = ResultSet.GetUnsortedResults();

		const bool bAreValuesPruned = SearchIndex.ValuesVectorToPoseIndexes.Num() > 0;
		if (bAreValuesPruned)
		{
			const int32 MaxNumEvaluatePoseKernelCalls = KDTreeQueryNumNeighborsWithDuplicates > 0 ? KDTreeQueryNumNeighborsWithDuplicates : INT32_MAX;
			for (int32 ResultIndex = 0; ResultIndex < UnsortedResults.Num(); ++ResultIndex)
			{
				const FIndexDistance& IndexDistance = UnsortedResults[ResultIndex];

				// @todo: IndexDistance.Distance is the Sqrt(DissimilarityCost), so there's no need to calculate it again in SearchIndex.ComparePoses
				const TConstArrayView<int32> PoseIndexes = SearchIndex.ValuesVectorToPoseIndexes[IndexDistance.Index];
				for (int32 Index = 0; Index < PoseIndexes.Num() && NumEvaluatePoseKernelCalls < MaxNumEvaluatePoseKernelCalls; ++Index, ++NumEvaluatePoseKernelCalls)
				{
					EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIndexes[Index], INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
						, ResultIndex
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
				}
			}
		}
		else
		{
			for (int32 ResultIndex = 0; ResultIndex < UnsortedResults.Num(); ++ResultIndex)
			{
				const FIndexDistance& IndexDistance = UnsortedResults[ResultIndex];

				// @todo: IndexDistance.Distance is the Sqrt(DissimilarityCost), so there's no need to calculate it again in SearchIndex.ComparePoses
				EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), IndexDistance.Index, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
					, ResultIndex
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
				);
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// @todo: perhaps store the DynamicWeightsSqrt into the GetOrBuildQuery to share between the continuing pose and the full search
		const int32 NumDimensions = Schema->SchemaCardinality;
		TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, QueryValues, DynamicWeightsSqrt);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = GetNormalizedAssetTime(Result.PoseIdx);
		Result.Database = this;
	}

	check(Result.DebugValidate());
	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchBruteForce(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_BruteForce);
	
	using namespace UE::PoseSearch;
	
	FSearchResult Result;

	const FSearchIndex& SearchIndex = GetSearchIndex();
	const float ContinuingPoseCostAddend = 0.f;
	const float ContinuingInteractionCostAddend = SearchContext.IsContinuingInteraction() ? ContinuingInteractionCostBias : 0.f;

	// since any PoseCost calculated here is at least SearchIndex.MinCostAddend,
	// there's no point in performing the search if CurrentBestTotalCost is already better than that
	if (!GetSkipSearchIfPossible() || SearchContext.GetCurrentBestTotalCost() > SearchIndex.MinCostAddend)
	{
		// @todo: perhaps store the DynamicWeightsSqrt into the GetOrBuildQuery to share between the continuing pose and the full search
		const int32 NumDimensions = Schema->SchemaCardinality;
		TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);

		FSelectableAssetIdx SelectableAssetIdx;
		PopulateSelectableAssetIdx(SelectableAssetIdx, SearchContext.GetAssetsToConsider());

		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext
#if UE_POSE_SEARCH_TRACE_ENABLED
			, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, QueryValues, DynamicWeightsSqrt
#endif // UE_POSE_SEARCH_TRACE_ENABLED
		);

		const bool bUpdateBestCandidates = PoseSearchMode == EPoseSearchMode::BruteForce;
		const FSearchFilters SearchFilters(Schema, NonSelectableIdx, FSelectableAssetIdx(), SearchIndex.bAnyBlockTransition);

		if (SelectableAssetIdx.IsEmpty())
		{
			// do we need to reconstruct pose values?
			if (SearchIndex.IsValuesEmpty())
			{
				// FMemory_Alloca is forced 16 bytes aligned
				TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
				check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));
				for (int32 PoseIdx = 0; PoseIdx < SearchIndex.GetNumPoses(); ++PoseIdx)
				{
					EvaluatePoseKernel<true, false>(Result, SearchIndex, QueryValues, ReconstructedPoseValuesBuffer, PoseIdx, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, bUpdateBestCandidates
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
						, PoseIdx
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
					);
				}
			}
			// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
			else if (NumDimensions % 4 == 0)
			{
				for (int32 PoseIdx = 0; PoseIdx < SearchIndex.GetNumPoses(); ++PoseIdx)
				{
					EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, bUpdateBestCandidates
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
						, PoseIdx
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
				}
			}
			// no reconstruction, but data is not 16 bytes padded
			else
			{
				for (int32 PoseIdx = 0; PoseIdx < SearchIndex.GetNumPoses(); ++PoseIdx)
				{
					EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, bUpdateBestCandidates
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
						, PoseIdx
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
				}
			}
		}
		else
		{
			int32 ResultIndex = -1;

			// do we need to reconstruct pose values?
			if (SearchIndex.IsValuesEmpty())
			{
				// FMemory_Alloca is forced 16 bytes aligned
				TArrayView<float> ReconstructedPoseValuesBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
				check(IsAligned(ReconstructedPoseValuesBuffer.GetData(), alignof(VectorRegister4Float)));

				for (int32 AssetIdx : SelectableAssetIdx)
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIdx];
					const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
					const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
					for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
					{
						EvaluatePoseKernel<true, false>(Result, SearchIndex, QueryValues, ReconstructedPoseValuesBuffer, PoseIdx, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, bUpdateBestCandidates
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
							, ++ResultIndex
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
					}
				}
			}
			// is the data padded at 16 bytes (and 16 bytes aligned by construction)?
			else if (NumDimensions % 4 == 0)
			{
				for (int32 AssetIdx : SelectableAssetIdx)
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIdx];
					const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
					const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
					for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
					{
						EvaluatePoseKernel<false, true>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, bUpdateBestCandidates
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
							, ++ResultIndex
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
					}
				}
			}
			// no reconstruction, but data is not 16 bytes padded
			else
			{
				for (int32 AssetIdx : SelectableAssetIdx)
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIdx];
					const int32 FirstPoseIdx = SearchIndexAsset.GetFirstPoseIdx();
					const int32 LastPoseIdx = FirstPoseIdx + SearchIndexAsset.GetNumPoses();
					for (int32 PoseIdx = FirstPoseIdx; PoseIdx < LastPoseIdx; ++PoseIdx)
					{
						EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, INDEX_NONE, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, bUpdateBestCandidates
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
							, ++ResultIndex
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
						);
					}
				}
			}
		}
	}
	else
	{
#if UE_POSE_SEARCH_TRACE_ENABLED
		// @todo: perhaps store the DynamicWeightsSqrt into the GetOrBuildQuery to share between the continuing pose and the full search
		const int32 NumDimensions = Schema->SchemaCardinality;
		TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
		const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

		// calling just for reporting non selectable poses
		TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);
		FNonSelectableIdx NonSelectableIdx;
		PopulateNonSelectableIdx(NonSelectableIdx, SearchContext, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, QueryValues, DynamicWeightsSqrt);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = GetNormalizedAssetTime(Result.PoseIdx);
		Result.Database = this;
	}

	check(Result.DebugValidate());
	return Result;
}

UE::PoseSearch::FSearchResult UPoseSearchDatabase::SearchEvent(UE::PoseSearch::FSearchContext& SearchContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Event);

	using namespace UE::PoseSearch;

	FSearchResult Result;

	const FSearchIndex& SearchIndex = GetSearchIndex();
	if (SearchIndex.IsValuesEmpty())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchDatabase::SearchEvent unimplemented for reconstructed poses"));
	}
	else
	{
		const FPoseSearchEvent& EventToSearch = SearchContext.GetEventToSearch();
		const TConstArrayView<int32> PosesWithEvent = SearchIndex.EventData.GetPosesWithEvent(EventToSearch.EventTag);
		if (!PosesWithEvent.IsEmpty())
		{
			const float ContinuingPoseCostAddend = 0.f;
			const float ContinuingInteractionCostAddend = SearchContext.IsContinuingInteraction() ? ContinuingInteractionCostBias : 0.f;

			TConstArrayView<float> QueryValues = SearchContext.GetOrBuildQuery(Schema);

			const int32 NumDimensions = Schema->SchemaCardinality;
			TArrayView<float> DynamicWeightsSqrtBuffer((float*)FMemory_Alloca(NumDimensions * sizeof(float)), NumDimensions);
			const TConstArrayView<float> DynamicWeightsSqrt = CalculateDynamicWeightsSqrt(DynamicWeightsSqrtBuffer);

			FSelectableAssetIdx SelectableAssetIdx;
			PopulateSelectableAssetIdx(SelectableAssetIdx, SearchContext.GetAssetsToConsider());

			FNonSelectableIdx NonSelectableIdx;
			if (EventToSearch.bEnablePoseFilters)
			{
				PopulateNonSelectableIdx(NonSelectableIdx, SearchContext
#if UE_POSE_SEARCH_TRACE_ENABLED
					, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, QueryValues, DynamicWeightsSqrt
#endif // UE_POSE_SEARCH_TRACE_ENABLED
				);
			}

			FSearchFilters SearchFilters(Schema, NonSelectableIdx, SelectableAssetIdx, SearchIndex.bAnyBlockTransition);

			if (FMath::IsNearlyZero(EventToSearch.TimeToEvent))
			{
				for (int32 EventPoseIdx : PosesWithEvent)
				{
					EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), EventPoseIdx, EventPoseIdx, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
						, EventPoseIdx
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
					);
				}
			}
			else
			{
				const float TimeToEventStart = EventToSearch.TimeToEvent * EventToSearch.PlayRateRangeOverride.Min;
				const float TimeToEventEnd = EventToSearch.TimeToEvent * EventToSearch.PlayRateRangeOverride.Max;
				for (int32 EventPoseIdx : PosesWithEvent)
				{
					// Calculating the pose which is TimeToEvent seconds before the event
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.GetAssetForPose(EventPoseIdx);
					const float EventTime = SearchIndexAsset.GetTimeFromPoseIndex(EventPoseIdx, Schema->SampleRate);
					SearchIndexAsset.IteratePoseIndexesOverTime(EventTime - TimeToEventEnd, EventTime - TimeToEventStart, Schema->SampleRate,
						[this, &Result, &SearchIndex, &QueryValues, EventPoseIdx, &SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, &SearchContext, &DynamicWeightsSqrt](int32 PoseIdx)
						{
							EvaluatePoseKernel<false, false>(Result, SearchIndex, QueryValues, TArrayView<float>(), PoseIdx, EventPoseIdx, SearchFilters, ContinuingPoseCostAddend, ContinuingInteractionCostAddend, SearchContext, this, DynamicWeightsSqrt, true
#if WITH_EDITOR && ENABLE_ANIM_DEBUG
								, PoseIdx
#endif // WITH_EDITOR && ENABLE_ANIM_DEBUG
							);
						});
				}
			}
		}
	}

	// finalizing Result properties
	if (Result.PoseIdx != INDEX_NONE)
	{
		Result.AssetTime = GetNormalizedAssetTime(Result.PoseIdx);
		Result.Database = this;
	}

	return Result;
}
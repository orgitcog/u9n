// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BonePose.h"
#include "Containers/RingBuffer.h"
#include "DrawDebugHelpers.h"
#include "Misc/MTTransactionallySafeAccessDetector.h"
#include "PoseSearch/PoseHistoryProvider.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "Animation/TrajectoryTypes.h"
#include "UObject/ObjectKey.h"

#include "PoseSearchHistory.generated.h"

#define UE_API POSESEARCH_API

struct FAnimInstanceProxy;
class USkeleton;
class UWorld;

namespace UE::PoseSearch
{

struct FSearchResult;
typedef uint16 FComponentSpaceTransformIndex;
typedef TPair<FBoneIndexType, FComponentSpaceTransformIndex> FBoneToTransformPair;
typedef TMap<FBoneIndexType, FComponentSpaceTransformIndex> FBoneToTransformMap;

struct IComponentSpacePoseProvider
{
	virtual ~IComponentSpacePoseProvider() {}
	virtual FTransform CalculateComponentSpaceTransform(const FSkeletonPoseBoneIndex SkeletonBoneIdx) = 0;
	virtual const USkeleton* GetSkeletonAsset() const = 0;
};

struct FComponentSpacePoseProvider : public IComponentSpacePoseProvider
{
	POSESEARCH_API FComponentSpacePoseProvider(FCSPose<FCompactPose>& InComponentSpacePose);

	virtual FTransform CalculateComponentSpaceTransform(const FSkeletonPoseBoneIndex SkeletonBoneIdx) override;
	virtual const USkeleton* GetSkeletonAsset() const override;

private:
	FCSPose<FCompactPose>& ComponentSpacePose;
};

struct FAIPComponentSpacePoseProvider : public IComponentSpacePoseProvider
{
	POSESEARCH_API FAIPComponentSpacePoseProvider(const FAnimInstanceProxy* InAnimInstanceProxy);

	virtual FTransform CalculateComponentSpaceTransform(const FSkeletonPoseBoneIndex SkeletonBoneIdx) override;
	virtual const USkeleton* GetSkeletonAsset() const override;

private:
	FCSPose<FCompactPose> ComponentSpacePose;
};

struct FPoseHistoryEntry
{
	// collected bones transforms in component space
	TArray<FQuat4f> ComponentSpaceRotations;
	TArray<FVector> ComponentSpacePositions;
	TArray<FVector3f> ComponentSpaceScales;
	TArray<float> CurveValues;
	float AccumulatedSeconds = 0.f;

	void Update(float Time, IComponentSpacePoseProvider& ComponentSpacePoseProvider, const FBoneToTransformMap& BoneToTransformMap, bool bStoreScales, const FBlendedCurve& Curves, const TConstArrayView<FName>& CollectedCurves);

	POSESEARCH_API void SetNum(int32 Num, bool bStoreScales);
	int32 Num() const;

	POSESEARCH_API void SetComponentSpaceTransform(int32 Index, const FTransform& Transform);
	FTransform GetComponentSpaceTransform(int32 Index) const;
	float GetCurveValue(int32 Index) const;
};
FArchive& operator<<(FArchive& Ar, FPoseHistoryEntry& Entry);

struct IPoseHistory : public TSharedFromThis<IPoseHistory, ESPMode::ThreadSafe>
{
	virtual ~IPoseHistory() {}
	
	// returns transform of the the skeleton bone index BoneIndexType (encoded as FBoneIndexType) relative to the transform of ReferenceBoneIndexType:
	// if ReferenceBoneIndexType is 0 (RootBoneIndexType), OutBoneTransform is in root bone space
	// if ReferenceBoneIndexType is FBoneIndexType(-1) (ComponentSpaceIndexType), OutBoneTransform is in component space
	// if ReferenceBoneIndexType is FBoneIndexType(-2) (WorldSpaceIndexType), OutBoneTransform is in world space
	virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = false) const = 0;
	// @todo: consider consolidating into a (templated?) get X value at time once we add custom attributes to pose history.
	virtual bool GetCurveValueAtTime(float Time, const FName& CurveName, float& OutCurveValue, bool bExtrapolate = false) const = 0;
	
	// @note for review: Here we are just changing the return type to force users to update immediately. Will cause project to not compile for users who override the pure virtual func getter, not sure how we can gracefully deprecate this.
	// Another option is to just have a new function signature GetTransformTrajectory() and add a final tag to the GetTrajectory() to also force users to upgrade.
	// Lastly, to me this seems like a struct not many licensees would inherent from tbh.
	virtual const FTransformTrajectory& GetTrajectory() const = 0;
	
	// Experimental, this feature might be removed without warning, not for production use
	virtual void SetTrajectory(const FTransformTrajectory& InTrajectory, float InTrajectorySpeedMultiplier = 1.f) = 0;
	// Experimental, this feature might be removed without warning, not for production use
	virtual void GenerateTrajectory(const UObject* AnimContext, float DeltaTime) = 0;
	// @todo: deprecate this API. TrajectorySpeedMultiplier should be a global query scaling value passed as input parameter of FSearchContext during config BuildQuery
	virtual float GetTrajectorySpeedMultiplier() const = 0;
	virtual bool IsEmpty() const = 0;

	virtual const FBoneToTransformMap& GetBoneToTransformMap() const = 0;
	virtual const TConstArrayView<FName> GetCollectedCurves() const = 0;
	virtual int32 GetNumEntries() const = 0;
	virtual const FPoseHistoryEntry& GetEntry(int32 EntryIndex) const = 0;

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(const UWorld* World, FColor Color) const = 0;
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const = 0;
	UE_API virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color, float Time, float PointSize = 6.f, bool bExtrapolate = false) const;
#endif
};

struct FArchivedPoseHistory : public IPoseHistory
{
	UE_API void InitFrom(const IPoseHistory* PoseHistory);

	// IPoseHistory interface
	UE_API virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = false) const override;
	UE_API virtual bool GetCurveValueAtTime(float Time, const FName& CurveName, float& outcurvevalue, bool bExtrapolate = false) const override;
	virtual const FTransformTrajectory& GetTrajectory() const override { return Trajectory; }
	// Experimental, this feature might be removed without warning, not for production use
	virtual void SetTrajectory(const FTransformTrajectory& InTrajectory, float InTrajectorySpeedMultiplier = 1.f) override { Trajectory = InTrajectory; }
	// Experimental, this feature might be removed without warning, not for production use
	virtual void GenerateTrajectory(const UObject* AnimContext, float DeltaTime) override { checkNoEntry(); }
	virtual float GetTrajectorySpeedMultiplier() const override { return 1.f; }
	virtual bool IsEmpty() const override { return Entries.IsEmpty(); }
	virtual const FBoneToTransformMap& GetBoneToTransformMap() const override { return BoneToTransformMap; }
	virtual const TConstArrayView<FName> GetCollectedCurves() const override { return CollectedCurves; }
	virtual int32 GetNumEntries() const override { return Entries.Num(); }
	virtual const FPoseHistoryEntry& GetEntry(int32 EntryIndex) const override { return Entries[EntryIndex]; }

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	UE_API virtual void DebugDraw(const UWorld* World, FColor Color) const override;
	virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const override { unimplemented(); }
#endif
	// End of IPoseHistory interface

	FBoneToTransformMap BoneToTransformMap;
	// @todo: Make this a map if this is expected to be big.
	TArray<FName> CollectedCurves;
	TArray<FPoseHistoryEntry> Entries;
	
	FTransformTrajectory Trajectory;
};
FArchive& operator<<(FArchive& Ar, FArchivedPoseHistory& Entry);

struct FPoseHistory : public IPoseHistory
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPoseHistory() = default;
	virtual ~FPoseHistory() override = default;
	UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FPoseHistory(const FPoseHistory& Other);
	UE_API FPoseHistory(FPoseHistory&& Other);
	UE_API FPoseHistory& operator=(const FPoseHistory& Other);
	UE_API FPoseHistory& operator=(FPoseHistory&& Other);

	// @todo: deprecate and delete this method in favor of providing the trajectory via SetTrajectory
	UE_API void GenerateTrajectory(const UObject* AnimContext, float DeltaTime, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling);
	
	UE_DEPRECATED(5.6, "No longer necessary method")
	void PreUpdate() {}

	UE_API void Initialize_AnyThread(int32 InNumPoses, float InSamplingInterval);
	UE_API void EvaluateComponentSpace_AnyThread(float DeltaTime, IComponentSpacePoseProvider& ComponentSpacePoseProvider, bool bStoreScales,
		float RootBoneRecoveryTime, float RootBoneTranslationRecoveryRatio, float RootBoneRotationRecoveryRatio,
		bool bNeedsReset, bool bCacheBones, const TArray<FBoneIndexType>& RequiredBones, 
		const FBlendedCurve& Curves = FBlendedCurve(), const TConstArrayView<FName>& CollectedCurves = TConstArrayView<FName>());

	UE_DEPRECATED(5.6, "Use other EvaluateComponentSpace_AnyThread signatures instead")
	UE_API void EvaluateComponentSpace_AnyThread(float DeltaTime, FCSPose<FCompactPose>& ComponentSpacePose, bool bStoreScales,
		float RootBoneRecoveryTime, float RootBoneTranslationRecoveryRatio, float RootBoneRotationRecoveryRatio,
		bool bNeedsReset, bool bCacheBones, const TArray<FBoneIndexType>& RequiredBones,
		const FBlendedCurve& Curves = FBlendedCurve(), const TConstArrayView<FName>& CollectedCurves = TConstArrayView<FName>());
		
	// IPoseHistory interface
	UE_API virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = false) const override;
	UE_API virtual bool GetCurveValueAtTime(float Time, const FName& CurveName, float& outcurvevalue, bool bExtrapolate = false) const override;
	UE_API virtual const FTransformTrajectory& GetTrajectory() const override;
	// Experimental, this feature might be removed without warning, not for production use
	UE_API virtual void SetTrajectory(const FTransformTrajectory& InTrajectory, float InTrajectorySpeedMultiplier = 1.f) override;
	// Experimental, this feature might be removed without warning, not for production use
	UE_API virtual void GenerateTrajectory(const UObject* AnimContext, float DeltaTime) override;
	UE_API virtual float GetTrajectorySpeedMultiplier() const override;
	UE_API virtual bool IsEmpty() const override;
	UE_API virtual const FBoneToTransformMap& GetBoneToTransformMap() const override;
	UE_API virtual const TConstArrayView<FName> GetCollectedCurves() const override;

	// We need to handle the changes from CL 39422514 since licensees still haven't gotten the move of this function to the IPoseHistory, as of 5.5, therefore this function still needs to be marked as deprecated even tho it was deleted later on.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "SetTrajectory() that takes in FPoseSearchQueryTrajectory is deprecated. Use version that takes FTransformTrajectory instead.")
	UE_API void SetTrajectory(const FPoseSearchQueryTrajectory& InTrajectory, float InTrajectorySpeedMultiplier = 1.f);
	UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	virtual int32 GetNumEntries() const override;
	UE_API virtual const FPoseHistoryEntry& GetEntry(int32 EntryIndex) const override;

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(const UWorld* World, FColor Color) const override { unimplemented(); }
	UE_API virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const override;
#endif
	// End of IPoseHistory interface
	
	int32 GetMaxNumPoses() const { return MaxNumPoses; }
	float GetSamplingInterval() const { return SamplingInterval; }

private:

	// caching MaxNumPoses, since FData::Entries.Max() is a padded number
	int32 MaxNumPoses = 0;

	float SamplingInterval = 0.f;
	
	FTransformTrajectory Trajectory;
	
	FPoseSearchTrajectoryData::FState TrajectoryDataState;
	// @todo: deprecate this member and expose it via blue print logic or as global query scaling multiplier
	float TrajectorySpeedMultiplier = 1.f;

	struct FPoseData
	{
		// skeleton from the last Update, to keep tracking skeleton changes, and support compatible skeletons
		TWeakObjectPtr<const USkeleton> LastUpdateSkeleton;

		// Map from skeleton bone indexes (encoded as FBoneIndexType) to internal pose history transform index (FComponentSpaceTransformIndex).
		// If Empty all the bones get collected
		FBoneToTransformMap BoneToTransformMap;
		
		// list of curves that we want to collect into our history.
		TArray<FName> CollectedCurves;

		// GetTypeHash for BoneToTransformMap
		uint32 BoneToTransformMapTypeHash = 0;

		// ring buffer of collected bones
		TRingBuffer<FPoseHistoryEntry> Entries;
	} PoseData;

	UE_MT_DECLARE_TS_RW_ACCESS_DETECTOR(PoseDataThreadSafeCounter);
};

// pose history struct that can generate a trajectory properly by calling it's IPoseHistory::GenerateTrajectory API (something other IPoseHistory don't do fully)
struct FGenerateTrajectoryPoseHistory : public FPoseHistory
{
	bool bGenerateTrajectory = false;
	bool bIsTrajectoryGeneratedBeforePreUpdate = false;
	FPoseSearchTrajectoryData::FSampling TrajectoryDataSampling;
	FPoseSearchTrajectoryData TrajectoryData;

	UE_API virtual void GenerateTrajectory(const UObject* AnimContext, float DeltaTime) override;
};

struct FMemStackPoseHistory : public IPoseHistory
{
	UE_API void Init(const IPoseHistory* InPoseHistory);

	// IPoseHistory interface
	UE_API virtual bool GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton = nullptr, FBoneIndexType BoneIndexType = RootBoneIndexType, FBoneIndexType ReferenceBoneIndexType = ComponentSpaceIndexType, bool bExtrapolate = false) const override;
	UE_API virtual bool GetCurveValueAtTime(float Time, const FName& CurveName, float& outcurvevalue, bool bExtrapolate = false) const override;
	virtual const FTransformTrajectory& GetTrajectory() const override { check(PoseHistory); return PoseHistory->GetTrajectory(); }
	// Experimental, this feature might be removed without warning, not for production use
	UE_API virtual void SetTrajectory(const FTransformTrajectory& InTrajectory, float InTrajectorySpeedMultiplier = 1.f) override;
	// Experimental, this feature might be removed without warning, not for production use
	UE_API virtual void GenerateTrajectory(const UObject* AnimContext, float DeltaTime) override;
	virtual float GetTrajectorySpeedMultiplier() const override { check(PoseHistory); return PoseHistory->GetTrajectorySpeedMultiplier(); }
	virtual bool IsEmpty() const override { check(PoseHistory); return PoseHistory->IsEmpty() && FutureEntries.IsEmpty(); }
	virtual const FBoneToTransformMap& GetBoneToTransformMap() const override { check(PoseHistory); return PoseHistory->GetBoneToTransformMap(); }
	virtual const TConstArrayView<FName> GetCollectedCurves() const override { check(PoseHistory); return PoseHistory->GetCollectedCurves(); }
	
	UE_API virtual int32 GetNumEntries() const override;
	UE_API virtual const FPoseHistoryEntry& GetEntry(int32 EntryIndex) const override;
	
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	virtual void DebugDraw(const UWorld* World, FColor Color) const override { unimplemented(); }
	UE_API virtual void DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const override;
#endif
	// End of IPoseHistory interface

	UE_API void AddFutureRootBone(float Time, const FTransform& FutureRootBoneTransform, bool bStoreScales);
	UE_API void AddFuturePose(float Time, IComponentSpacePoseProvider& ComponentSpacePoseProvider, const FBlendedCurve& Curves = FBlendedCurve());
	
	// Experimental, this feature might be removed without warning, not for production use
	// extracting a pose from AnimationAsset at AnimationTime and  (using BlendParameters in case it's a blend space) and placing it in the FMemStackPoseHistory at IntervalTime seconds
	// if FiniteDeltaTime is > FiniteDeltaTime a second pose will be extracted at AnimationTime - FiniteDeltaTime and placed at IntervalTime - FiniteDeltaTime (for MM to be able to calculate velocities)
	UE_API void ExtractAndAddFuturePoses(const UAnimationAsset* AnimationAsset, float AnimationTime, float FiniteDeltaTime, const FVector& BlendParameters, float IntervalTime, const USkeleton* OverrideSkeleton = nullptr, bool bUseRefPoseRootBone = false);

	UE_DEPRECATED(5.6, "Use other AddFuturePose signatures instead")
	UE_API void AddFuturePose(float Time, FCSPose<FCompactPose>& ComponentSpacePose, const FBlendedCurve& Curves = FBlendedCurve());

	const IPoseHistory* GetThisOrPoseHistory() const { return FutureEntries.IsEmpty() ? PoseHistory : this; }

private:
	const IPoseHistory* PoseHistory = nullptr;
	TArray<FPoseHistoryEntry, TInlineAllocator<4, TMemStackAllocator<>>> FutureEntries;
};

struct FHistoricalPoseIndex
{
	bool operator==(const FHistoricalPoseIndex& Index) const
	{
		return PoseIndex == Index.PoseIndex && DatabaseKey == Index.DatabaseKey;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FHistoricalPoseIndex& Index)
	{
		return HashCombineFast(::GetTypeHash(Index.PoseIndex), GetTypeHash(Index.DatabaseKey));
	}

	int32 PoseIndex = INDEX_NONE;
	FObjectKey DatabaseKey;
};

struct FPoseIndicesHistory
{
	void Update(const FSearchResult& SearchResult, float DeltaTime, float MaxTime);
	void Reset() { IndexToTime.Reset(); }
	TMap<FHistoricalPoseIndex, float> IndexToTime;
};

} // namespace UE::PoseSearch

USTRUCT(BlueprintType)
struct FPoseHistoryReference
{
	GENERATED_BODY()
	TSharedPtr<UE::PoseSearch::IPoseHistory> PoseHistory;
};

#undef UE_API

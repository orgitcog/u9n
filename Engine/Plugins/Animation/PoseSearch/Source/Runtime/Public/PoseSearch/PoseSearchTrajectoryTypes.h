// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimTypes.h"
#include "PoseSearchTrajectoryTypes.generated.h"

struct FAnimInstanceProxy;
struct FTransformTrajectory;
struct FTransformTrajectorySample;


USTRUCT(BlueprintType, Category="Pose Search Trajectory")
struct UE_DEPRECATED(5.6, "FPoseSearchQueryTrajectorySample has been deprecated. Use FTransformTrajectorySample instead.")
POSESEARCH_API FPoseSearchQueryTrajectorySample 
{
	GENERATED_BODY()

	FPoseSearchQueryTrajectorySample(const FQuat& InFacing, const FVector& InPosition, float InAccumulatedSeconds);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Note: We need to explicitly disable warnings on these constructors/operators for clang to be happy with deprecated variables
	FPoseSearchQueryTrajectorySample() = default;
	~FPoseSearchQueryTrajectorySample() = default;
	FPoseSearchQueryTrajectorySample(const FPoseSearchQueryTrajectorySample&) = default;
	FPoseSearchQueryTrajectorySample(FPoseSearchQueryTrajectorySample&&) = default;
	FPoseSearchQueryTrajectorySample& operator=(const FPoseSearchQueryTrajectorySample&) = default;
	FPoseSearchQueryTrajectorySample& operator=(FPoseSearchQueryTrajectorySample&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	UE_DEPRECATED(5.6, "Use FTransfromTrajectorySample instead. See FTransfromTrajectorySample::Facing.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use FTransfromTrajectorySample instead. See FTransfromTrajectorySample::Facing."))
	FQuat Facing = FQuat::Identity;

	UE_DEPRECATED(5.6, "Use FTransfromTrajectorySample instead. See FTransfromTrajectorySample::Position.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use FTransfromTrajectorySample instead. See FTransfromTrajectorySample::Position."))
	FVector Position = FVector::ZeroVector;

	UE_DEPRECATED(5.6, "Use FTransfromTrajectorySample instead. See FTransfromTrajectorySample::AccumulatedSeconds.")
	UPROPERTY( meta = (DeprecatedProperty, DeprecationMessage = "Use FTransfromTrajectorySample instead. See FTransfromTrajectorySample::AccumulatedSeconds."))
	float AccumulatedSeconds = 0.f;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPoseSearchQueryTrajectorySample Lerp(const FPoseSearchQueryTrajectorySample& Other, float Alpha) const;
	void SetTransform(const FTransform& Transform);
	FTransform GetTransform() const { return FTransform(Facing, Position); }
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FPoseSearchQueryTrajectorySample& TrajectorySample);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

USTRUCT(BlueprintType, Category="Pose Search Trajectory")
struct UE_DEPRECATED(5.6, "FPoseSearchQueryTrajectory has been deprecated. Use FTransformTrajectory instead.")
POSESEARCH_API FPoseSearchQueryTrajectory
{
	GENERATED_BODY()

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Note: We need to explicitly disable warnings on these constructors/operators for clang to be happy with deprecated variables
	FPoseSearchQueryTrajectory() = default;
	~FPoseSearchQueryTrajectory() = default;
	FPoseSearchQueryTrajectory(const FPoseSearchQueryTrajectory&) = default;
	FPoseSearchQueryTrajectory(FPoseSearchQueryTrajectory&&) = default;
	FPoseSearchQueryTrajectory& operator=(const FPoseSearchQueryTrajectory&) = default;
	FPoseSearchQueryTrajectory& operator=(FPoseSearchQueryTrajectory&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Implicit conversion constructor in oreder to facilitate deprecations.
	FPoseSearchQueryTrajectory(const FTransformTrajectory& InTrajectory);
	
	// Implicit conversion operator in order to facilitate deprecations.
	operator FTransformTrajectory() const;
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// This contains zero or more history samples, a current sample, and zero or more future predicted samples.
	UE_DEPRECATED(5.6, "Use FTransformTrajectory instead. See FTransfromTrajectory::Samples.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use FTransfromTrajectory instead. See FTransfromTrajectory:Samples."))
	TArray<FPoseSearchQueryTrajectorySample> Samples;
	
	FPoseSearchQueryTrajectorySample GetSampleAtTime(float Time, bool bExtrapolate = false) const;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if ENABLE_ANIM_DEBUG
	void DebugDrawTrajectory(const UWorld* World, const float DebugThickness = 0.f, float HeightOffset = 0.f) const;
	void DebugDrawTrajectory(FAnimInstanceProxy& AnimInstanceProxy, const float DebugThickness = 0.f, float HeightOffset = 0.f, int MaxHistorySamples = -1, int MaxPredictionSamples = -1) const;
	void DebugDrawTrajectory(const UObject* Owner, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, const float DebugThickness = 0.f, float HeightOffset = 0.f) const;
#endif // ENABLE_ANIM_DEBUG
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FPoseSearchQueryTrajectory& Trajectory);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
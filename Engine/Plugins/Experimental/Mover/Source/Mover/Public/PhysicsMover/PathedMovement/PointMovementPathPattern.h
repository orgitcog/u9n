// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PathedMovementPatternBase.h"

#include "PointMovementPathPattern.generated.h"

UENUM()
enum class EPointMovementLocationBasis : uint8
{
	/** The location is relative to the previous point in the path */
	PreviousPoint,
	/** The location is relative to the origin of the path */
	PathOrigin,
	/** The location is relative to the world origin */
	World
};

USTRUCT(BlueprintType)
struct FPointMovementPathPoint
{
	GENERATED_BODY()

public:
	FPointMovementPathPoint() {}

	//@todo DanH: Same options for control as FSplinePathPatternPointData - might want a shared base
	
	/** Basis that the location of this point is relative to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Location)
	EPointMovementLocationBasis Basis = EPointMovementLocationBasis::PreviousPoint;
	
	/** The location to move toward, relative to the path origin (i.e. root component location by default) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Location, meta = (MakeEditWidget = true))
	FVector Location = FVector::Zero();

	/**
	 * The progress along the path that this point corresponds to.
	 * Read-only, calculated automatically based on number of points, distances between them, and movement rules for the point 
	 */
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = Location)
	mutable float Progress = 0.f;
	
	/** Distance from the start of the path to this point. Read-only. */
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = Location)
	mutable float DistanceFromStart = 0.f;

	/** The world  */
	mutable FVector WorldLoc = FVector::Zero();
};

/** Movement pattern that moves between explicitly defined points */
UCLASS()
class UPointMovementPathPattern : public UPathedMovementPatternBase
{
	GENERATED_BODY()

public:
	virtual void InitializePattern() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	virtual bool DebugDrawUsingStepSamples() const override { return false; }
	virtual void AppendDebugDrawElements(UPathedPhysicsDebugDrawComponent& DebugDrawComp, FBoxSphereBounds::Builder& InOutDebugBoundsBuilder) override;
	
protected:
	virtual FTransform CalcUnmaskedTargetRelativeTransform(float PatternProgress, const FTransform& CurTargetTransform) const override;

	//@todo DanH: We need to force a refresh whenever the path origin changes
	void RefreshAssignedPointProgress(bool bForceRefresh = false) const;

	/** Relative point locations to move toward, in sequence from first to last */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point Movement Pattern")
	TArray<FPointMovementPathPoint> PathPoints;

	/** The sum total distance of this path. Read-only for reference only. */
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Point Movement Pattern")
	mutable float TotalPathDistance = 0.f;

	mutable bool bHasAssignedPointProgress = false;
};
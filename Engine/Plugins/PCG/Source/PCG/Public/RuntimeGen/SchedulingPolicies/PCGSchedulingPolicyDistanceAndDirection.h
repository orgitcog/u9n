// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSchedulingPolicyBase.h"

#include "PCGSchedulingPolicyDistanceAndDirection.generated.h"

class IPCGGenSourceBase;

/**
 * SchedulingPolicyDistanceAndDirection uses distance from the generating volume 
 * and alignment with view direction to choose the most important volumes to generate.
 *
 * Distance and Direction are calculated with respect to the Generation Source.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSchedulingPolicyDistanceAndDirection : public UPCGSchedulingPolicyBase
{
	GENERATED_BODY()

public:
	/** Calculate the runtime scheduling priority with respect to a Generation Source. Should return a value in the range [0, 1], where higher values will be scheduled sooner. */
	virtual double CalculatePriority(const IPCGGenSourceBase* InGenSource, const FBox& GenerationBounds, bool bUse2DGrid) const override;

	/** True if the generation source would consider the given bounds for generation. */
	virtual bool ShouldGenerate(const IPCGGenSourceBase* InGenSource, const FBox& GenerationBounds, bool bUse2DGrid) const override;

	/** True if the generation source would cull the given bounds. Only applies to bounds within the cleanup generation radius. */
	virtual bool ShouldCull(const IPCGGenSourceBase* InGenSource, const FBox& GenerationBounds, bool bUse2DGrid) const override;

	/** A SchedulingPolicy is equivalent to another SchedulingPolicy if they are the same (same ptr), or if they have the same type and parameter values. */
	virtual bool IsEquivalent(const UPCGSchedulingPolicyBase* OtherSchedulingPolicy) const;

public:
	/** Toggle whether or not distance is used to calculate the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (EditCondition = "bShouldDisplayProperties", EditConditionHides, HideEditConditionToggle))
	bool bUseDistance = true;

	/** Scalar value used to increase/decrease the impact of distance in the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 0.0f, ClampMax = 1.0f, EditCondition = "bShouldDisplayProperties && bUseDistance", EditConditionHides))
	float DistanceWeight = 1.0f;

	/** Toggle whether or not direction is used to calculate the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (EditCondition = "bShouldDisplayProperties", EditConditionHides, HideEditConditionToggle))
	bool bUseDirection = true;

	/** Scalar value used to increase/decrease the impact of direction in the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 0.0f, ClampMax = 1.0f, EditCondition = "bShouldDisplayProperties && bUseDirection", EditConditionHides))
	float DirectionWeight = 0.0025f;

	/** With frustum culling enabled, only components whose bounds overlap the view frustum will be generated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (EditCondition = "bShouldDisplayProperties", EditConditionHides, HideEditConditionToggle))
	bool bUseFrustumCulling = false;

	/** Multiplier to scale bounds by when comparing against the view frustum for generation. Can help if components on the edge of the frustum are not generating as soon as you would like. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 1.0f, EditCondition = "bShouldDisplayProperties && bUseFrustumCulling", EditConditionHides))
	float GenerateBoundsModifier = 1.0f;

	/** Multiplier to scale bounds by when comparing against the view frustum for clean up. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 1.0f, EditCondition = "bShouldDisplayProperties && bUseFrustumCulling", EditConditionHides))
	float CleanupBoundsModifier = 1.2f;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearchCost.generated.h"

USTRUCT()
struct FPoseSearchCost
{
	GENERATED_BODY()
public:

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Note: We need to explicitly disable warnings on these constructors/operators for clang to be happy with deprecated variables
	// this is a requirement for clang to compile without warnings.
	FPoseSearchCost() = default;
	~FPoseSearchCost() = default;
	FPoseSearchCost(const FPoseSearchCost&) = default;
	FPoseSearchCost(FPoseSearchCost&&) = default;
	FPoseSearchCost& operator=(const FPoseSearchCost&) = default;
	FPoseSearchCost& operator=(FPoseSearchCost&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FPoseSearchCost(float DissimilarityCost, float InNotifyCostAddend, float InContinuingPoseCostAddend, float InContinuingInteractionCostAddend = 0.f)
		: TotalCost(DissimilarityCost + InNotifyCostAddend + InContinuingPoseCostAddend + InContinuingInteractionCostAddend)
#if WITH_EDITORONLY_DATA
		, NotifyCostAddend(InNotifyCostAddend)
		, ContinuingPoseCostAddend(InContinuingPoseCostAddend)
		, ContinuingInteractionCostAddend(InContinuingInteractionCostAddend)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		, CostAddend(InNotifyCostAddend + InContinuingPoseCostAddend + InContinuingInteractionCostAddend)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
	{
	}

	static bool IsCostValid(const float Cost) { return Cost != MAX_flt; }
	bool IsValid() const { return IsCostValid(TotalCost); }
	
	UE_DEPRECATED(5.6, "Use operator float() instead.")
	float GetTotalCost() const { return TotalCost; }

	operator float() const { return TotalCost; }

#if WITH_EDITORONLY_DATA
	float GetCostAddend() const { return GetNotifyCostAddend() + GetContinuingPoseCostAddend() + GetContinuingInteractionCostAddend(); }

	float GetNotifyCostAddend() const { return NotifyCostAddend; }
	float GetContinuingPoseCostAddend() const { return ContinuingPoseCostAddend; }
	float GetContinuingInteractionCostAddend() const { return ContinuingInteractionCostAddend; }
#endif // WITH_EDITORONLY_DATA

protected:
	// TotalCost is the sum of all the Cost contributions (dissimilarity, notifies, continuing pose, continuing interaction costs)
	UPROPERTY()
	float TotalCost = MAX_flt;

#if WITH_EDITORONLY_DATA

	// Notify Cost Bias contribution
	UPROPERTY()
	float NotifyCostAddend = 0.f;
	
	// Continuing Pose Cost Bias contribution
	UPROPERTY()
	float ContinuingPoseCostAddend = 0.f;

	// Experimental, this feature might be removed without warning, not for production use
	// Continuing Interaction Cost Bias contribution
	UPROPERTY()
	float ContinuingInteractionCostAddend = 0.f;

public:

	UE_DEPRECATED(5.6, "Use GetCostAddend instead")
	UPROPERTY()
	float CostAddend = 0.f;
#endif // WITH_EDITORONLY_DATA
};

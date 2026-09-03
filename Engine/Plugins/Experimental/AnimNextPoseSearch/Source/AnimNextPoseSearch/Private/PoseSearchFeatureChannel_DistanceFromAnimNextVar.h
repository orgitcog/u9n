// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel_Distance.h"
#include "PoseSearchFeatureChannel_DistanceFromAnimNextVar.generated.h"

UCLASS(Experimental, EditInlineNew, Blueprintable, meta = (DisplayName = "Distance Channel (AnimNext Variable)"), CollapseCategories)
class UPoseSearchFeatureChannel_DistanceFromAnimNextVar : public UPoseSearchFeatureChannel_Distance
{
	GENERATED_BODY()
	
	UPROPERTY(DisplayName = "Variable", EditAnywhere, Category="Variable")
	FName DistanceVariableName;

	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const override;
};

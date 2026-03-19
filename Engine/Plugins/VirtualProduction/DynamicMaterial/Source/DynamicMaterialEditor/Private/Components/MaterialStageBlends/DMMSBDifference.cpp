// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBDifference.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendDifference"

UDMMaterialStageBlendDifference::UDMMaterialStageBlendDifference()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendDifference", "Difference"),
		LOCTEXT("BlendDifferenceDescription", "Difference outputs the difference in color between the base and blend layers."),
		"DM_Blend_Difference",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Difference.MF_DM_Blend_Difference'")
	)
{
}

#undef LOCTEXT_NAMESPACE

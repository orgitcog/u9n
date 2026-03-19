// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBDarken.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendDarken"

UDMMaterialStageBlendDarken::UDMMaterialStageBlendDarken()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendDarken", "Darken"),
		LOCTEXT("BlendDarkenDescription", "Darken chooses the color with the lowest luminance between the base and blend layer."),
		"DM_Blend_Darken",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Darken.MF_DM_Blend_Darken'")
	)
{
}

#undef LOCTEXT_NAMESPACE

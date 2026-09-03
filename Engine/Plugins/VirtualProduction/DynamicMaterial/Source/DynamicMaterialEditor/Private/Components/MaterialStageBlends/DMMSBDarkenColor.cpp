// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBDarkenColor.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendDarkenColor"

UDMMaterialStageBlendDarkenColor::UDMMaterialStageBlendDarkenColor()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendDarkenColor", "DarkenColor"),
		LOCTEXT("BlendDarkenColorDescription", "Darken Color chooses the lowest value in each color channel between the base and blend layer."),
		"DM_Blend_DarkenColor",
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_DarkenColor.MF_DM_Blend_DarkenColor'")
	)
{
}

#undef LOCTEXT_NAMESPACE

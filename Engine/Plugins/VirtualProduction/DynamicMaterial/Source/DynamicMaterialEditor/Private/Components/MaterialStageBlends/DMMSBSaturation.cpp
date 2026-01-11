// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBSaturation.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendSaturation"

UDMMaterialStageBlendSaturation::UDMMaterialStageBlendSaturation()
	: UDMMaterialStageBlendFunction(
		LOCTEXT("BlendSaturation", "Saturation"),
		LOCTEXT("BlendSaturationDescription", "Saturation combines the blend layer's saturation with the base layer's luminosity and hue."),
		"DM_Blend_Saturation", 
		TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/Blends/MF_DM_Blend_Saturation.MF_DM_Blend_Saturation'")
	)
{
}

#undef LOCTEXT_NAMESPACE

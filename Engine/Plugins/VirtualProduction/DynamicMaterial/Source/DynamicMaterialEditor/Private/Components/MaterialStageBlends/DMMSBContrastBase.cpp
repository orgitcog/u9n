// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageBlends/DMMSBContrastBase.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendContrastBase"

UDMMaterialStageBlendContrastBase::UDMMaterialStageBlendContrastBase()
	: UDMMaterialStageBlendContrastBase(LOCTEXT("BlendContrastBase", "Contrast Base"), FText::GetEmpty())
{
}

UDMMaterialStageBlendContrastBase::UDMMaterialStageBlendContrastBase(const FText& InName, const FText& InDescription)
	: UDMMaterialStageBlend(InName, InDescription)
{
}

#undef LOCTEXT_NAMESPACE

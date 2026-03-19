// Copyright Epic Games, Inc. All Rights Reserved.

#include "MorphTargetEditingToolProperties.h"

TArray<FName> UMorphTargetEditingToolProperties::GetMorphTargetNames()
{
	return MorphTargetNames;
}

FName UMorphTargetEditingToolProperties::GetEditingMorphTargetName()
{
	return Operation == EMorphTargetEditorOperation::New ? NewMorphTargetName : EditMorphTargetName;
}
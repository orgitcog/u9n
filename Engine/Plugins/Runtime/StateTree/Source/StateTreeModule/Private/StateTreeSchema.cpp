// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeSchema.h"
#include "StateTreeTypes.h"
#include "Blueprint/StateTreeNodeBlueprintBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeSchema)

bool UStateTreeSchema::IsChildOfBlueprintBase(const UClass* InClass) const
{
	return InClass->IsChildOf<UStateTreeNodeBlueprintBase>();
}

EStateTreeParameterDataType UStateTreeSchema::GetGlobalParameterDataType() const
{
	return EStateTreeParameterDataType::GlobalParameterData;
}
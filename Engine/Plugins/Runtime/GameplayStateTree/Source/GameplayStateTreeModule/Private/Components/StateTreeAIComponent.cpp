// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StateTreeAIComponent.h"

#include "Components/StateTreeAIComponentSchema.h"
#include "StateTreeExecutionContext.h"

TSubclassOf<UStateTreeSchema> UStateTreeAIComponent::GetSchema() const
{
	return UStateTreeAIComponentSchema::StaticClass();
}

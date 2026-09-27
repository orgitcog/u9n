// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"
#include "SceneStateTransitionGraph.generated.h"

class USceneStateTransitionResultNode;

UCLASS(MinimalAPI)
class USceneStateTransitionGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<USceneStateTransitionResultNode> ResultNode;
};

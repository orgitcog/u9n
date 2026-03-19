// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "EdGraph/EdGraphSchema.h"

class USceneStateBlueprintableTask;

namespace UE::SceneState::Graph
{

/** Adds a new task node with a set blueprint class to the State Machine Graph */
struct FStateMachineAction_NewBlueprintTask : public FEdGraphSchemaAction
{
	FStateMachineAction_NewBlueprintTask() = default;

	explicit FStateMachineAction_NewBlueprintTask(const FAssetData& InBlueprintTaskAsset, int32 InGrouping);

	//~ Begin FEdGraphSchemaAction
	virtual UEdGraphNode* PerformAction(UEdGraph* InParentGraph, UEdGraphPin* InSourcePin, const FVector2f& InLocation, bool bInSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction

	TSubclassOf<USceneStateBlueprintableTask> ResolveBlueprintTaskClass() const;

private:
	FAssetData BlueprintTaskAsset;
};

} // UE::SceneState::Graph

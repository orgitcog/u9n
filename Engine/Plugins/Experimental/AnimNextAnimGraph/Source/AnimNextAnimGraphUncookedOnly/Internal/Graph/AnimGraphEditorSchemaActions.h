// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/GraphEditorSchemaActions.h"
#include "UncookedOnlyUtils.h"
#include "AnimGraphEditorSchemaActions.generated.h"

USTRUCT()
struct ANIMNEXTANIMGRAPHUNCOOKEDONLY_API FAnimNextSchemaAction_AddManifestNode : public FAnimNextSchemaAction
{
	GENERATED_BODY()

	FAnimNextSchemaAction_AddManifestNode() = default;

	FAnimNextSchemaAction_AddManifestNode(const FAnimNextAssetRegistryManifestNode& InManifestNodeData, const FText& InKeywords = FText::GetEmpty());

	// FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2f& Location, bool bSelectNewNode = true) override { return nullptr; }

	TSoftObjectPtr<URigVMGraph> ModelGraph;
	FString NodeName;
};

USTRUCT()
struct ANIMNEXTANIMGRAPHUNCOOKEDONLY_API FAnimNextSchemaAction_NotifyEvent : public FAnimNextSchemaAction
{
	GENERATED_BODY()

	FAnimNextSchemaAction_NotifyEvent();

	// FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2f& Location, bool bSelectNewNode = true) override { return nullptr; }
};

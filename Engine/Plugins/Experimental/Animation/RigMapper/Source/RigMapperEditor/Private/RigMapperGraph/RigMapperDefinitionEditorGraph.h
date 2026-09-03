// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "RigMapperDefinitionEditorGraph.generated.h"

enum class ERigMapperNodeType : uint8;
class URigMapperDefinitionEditorGraphNode;
class URigMapperDefinition;

/**
 * 
 */
UCLASS()
class RIGMAPPEREDITOR_API URigMapperDefinitionEditorGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:
	void Initialize(URigMapperDefinition* InDefinition);
	
	void RebuildGraph();
	void ConstructNodes();
	void RemoveAllNodes();
	void RequestRefreshLayout(bool bInRefreshLayout) { bRefreshLayout = bInRefreshLayout; }
	bool NeedsRefreshLayout() const { return bRefreshLayout; }
	void LayoutNodes() const;
	
	URigMapperDefinition* GetDefinition() const { return WeakDefinition.Get(); };

	TArray<URigMapperDefinitionEditorGraphNode*> GetNodesByName(const TArray<FString>& Inputs, const TArray<FString>& Features, const TArray<FString>& Outputs, const TArray<FString>& NullOutputs) const;

private:
	URigMapperDefinitionEditorGraphNode* CreateGraphNodesRec(URigMapperDefinition* Definition, const FString& InNodeName, bool bIsOutputNode);
	URigMapperDefinitionEditorGraphNode* CreateOutputNode(URigMapperDefinition* Definition, const FString& NodeName);
	URigMapperDefinitionEditorGraphNode* CreateFeatureNode(URigMapperDefinition* Definition, const FString& NodeName);
	static void LinkGraphNodes(URigMapperDefinitionEditorGraphNode* InNode, URigMapperDefinitionEditorGraphNode* OutNode);
	URigMapperDefinitionEditorGraphNode* CreateGraphNode(const FString& NodeName, ERigMapperNodeType NodeType);
	void LayoutNodeRec(URigMapperDefinitionEditorGraphNode* InNode, double InputsWidth, double PosY, TArray<URigMapperDefinitionEditorGraphNode*>& LayedOutNodes) const;
	
private:
	TWeakObjectPtr<URigMapperDefinition> WeakDefinition;
	TMap<FString, URigMapperDefinitionEditorGraphNode*> InputNodes;
	TMap<FString, URigMapperDefinitionEditorGraphNode*> FeatureNodes;
	TMap<FString, URigMapperDefinitionEditorGraphNode*> OutputNodes;
	TMap<FString, URigMapperDefinitionEditorGraphNode*> NullOutputNodes;

	bool bRefreshLayout = false;
};

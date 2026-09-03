// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyBagEditorGraph.h"
#include "EdGraph/EdGraph.h"

#include "PCGGraph.h"

#include "PCGEditorGraph.generated.h"

class FPCGEditor;
class UPCGNode;
class UPCGPin;
class UPCGEditorGraphNodeBase;

namespace PCGEditorGraphUtils
{
	/** Goes up the graph to the first non-reroute node it can, assuming it's properly connected. */
	void GetInspectablePin(const UPCGNode* InNode, const UPCGPin* InPin, const UPCGNode*& OutNode, const UPCGPin*& OutPin);
}

UCLASS()
class UPCGEditorGraph : public UEdGraph, public IPropertyBagEdGraph
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	virtual void BeginDestroy() override;
	// ~End UObject interface

	/** Initialize the editor graph from a PCGGraph */
	void InitFromNodeGraph(UPCGGraph* InPCGGraph);

	/** If the underlying graph changed without UI interaction, use this function to reconstruct the UI elements. */
	void ReconstructGraph();

	/** When the editor is closing */
	void OnClose();

	/** Creates the links for a given node */
	void CreateLinks(UPCGEditorGraphNodeBase* InGraphNode, bool bCreateInbound, bool bCreateOutbound);

	/** To be called everytime we need to replicate our extra nodes to the underlying PCGGraph */
	void ReplicateExtraNodes() const;

	UPCGGraph* GetPCGGraph() const { return PCGGraph; }

	void SetEditor(TWeakPtr<FPCGEditor> InEditor) { PCGEditor = InEditor; }
	TWeakPtr<FPCGEditor> GetEditor() const { return PCGEditor; }

	/** Updates the grid size visualization in the editor. */
	void UpdateVisualizations(UPCGComponent* PCGComponentBeingInspected, const FPCGStack* PCGStackBeingInspected);

	/** Returns the PCG editor graph node corresponding to the given PCG node. */
	const UPCGEditorGraphNodeBase* GetEditorNodeFromPCGNode(const UPCGNode* InPCGNode) const;

	/** Returns the PCG Editor Graph Node that should be spawned from any given PCG Settings. */
	static TSubclassOf<UPCGEditorGraphNodeBase> GetGraphNodeClassFromPCGSettings(const UPCGSettings* Settings);

protected:
	void CreateLinks(UPCGEditorGraphNodeBase* InGraphNode, bool bCreateInbound, bool bCreateOutbound, const TMap<UPCGNode*, UPCGEditorGraphNodeBase*>& InPCGNodeToPCGEditorNodeMap);

	void OnGraphUserParametersChanged(UPCGGraphInterface* InGraph, EPCGGraphParameterEvent ChangeType, FName ChangedPropertyName);

	// ~Begin IPropertyBagEdGraph interface
	virtual bool CanReceivePropertyBagDetailsDropOnGraphPin(const UEdGraphPin* Pin) const override;
	virtual bool CanReceivePropertyBagDetailsDropOnGraphNode(const UEdGraphNode* Node) const override;
	virtual bool CanReceivePropertyBagDetailsDropOnGraph(const UEdGraph* Graph) const override;
	virtual FReply OnPropertyBagDetailsDropOnGraphPin(const FPropertyBagPropertyDesc& PropertyDesc, UEdGraphPin* Pin, const FVector2f& GraphPosition) const override;
	virtual FReply OnPropertyBagDetailsDropOnGraph(const FPropertyBagPropertyDesc& PropertyDesc, UEdGraph* Graph, const FVector2f& GraphPosition) const override;
	// ~End IPropertyBagEdGraph interface

private:
	UPROPERTY()
	TObjectPtr<UPCGGraph> PCGGraph = nullptr;

	TWeakPtr<FPCGEditor> PCGEditor = nullptr;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "UObject/Object.h"
#include "DataLinkGraph.generated.h"

class UDataLinkNode;
struct FConstStructView;
struct FDataLinkPinReference;

#if WITH_EDITOR
class UEdGraph;
#endif

UCLASS(MinimalAPI, BlueprintType, DisplayName="Motion Design Data Link Graph")
class UDataLinkGraph : public UObject
{
	GENERATED_BODY()

	friend class FDataLinkGraphCompiler;
	friend class UDataLinkGraphFactory;

public:
	/** Counts the number of input pins within the input node list that are not linked to other nodes */
	DATALINK_API int32 GetInputPinCount() const;

	/** Retrieves the Input Pins from the input nodes that are not linked to other nodes */
	DATALINK_API TArray<FDataLinkPinReference> GetInputPins() const;

	/** Iterates the input pins from the input nodes that are not linked to other nodes. These serve as the 'input pin' for the graph */
	DATALINK_API bool ForEachInputPin(TFunctionRef<bool(FDataLinkPinReference)> InFunction) const;

	TConstArrayView<const UDataLinkNode*> GetInputNodes() const
	{
		return InputNodes;
	}

	const UDataLinkNode* GetOutputNode() const
	{
		return OutputNode;
	}

#if WITH_EDITOR
	UEdGraph* GetEdGraph() const
	{
		return EdGraph;
	}

	static TMulticastDelegateRegistration<void(UDataLinkGraph*)>& OnGraphCompiled()
	{
		return OnGraphCompiledDelegate;
	}
#endif

private:
#if WITH_EDITOR
	DATALINK_API static TMulticastDelegate<void(UDataLinkGraph*)> OnGraphCompiledDelegate;
#endif

	/** All the compiled nodes present in this graph */
	UPROPERTY()
	TArray<TObjectPtr<UDataLinkNode>> Nodes;

	/** The nodes that the graph starts off with */
	UPROPERTY()
	TArray<TObjectPtr<UDataLinkNode>> InputNodes;

	/** The node that provides the result data */
	UPROPERTY()
	TObjectPtr<UDataLinkNode> OutputNode;

#if WITH_EDITORONLY_DATA
	/** EdGraph used to compile the nodes in this graph */
	UPROPERTY()
	TObjectPtr<UEdGraph> EdGraph;
#endif
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"
#include "K2Node_DataLinkRequest.generated.h"

class UDataLinkGraph;

UCLASS(MinimalAPI)
class UK2Node_DataLinkRequest : public UK2Node
{
	GENERATED_BODY()

public:
	static const FLazyName PN_DataLinkInstance;
	static const FLazyName PN_ExecutionContext;
	static const FLazyName PN_DataLinkSinkProvider;
	static const FLazyName PN_Success;
	static const FLazyName PN_Failure;

	UEdGraphPin* GetDataLinkInstancePin() const;
	UEdGraphPin* GetExecutionContextPin() const;
	UEdGraphPin* GetDataLinkSinkProviderPin() const;
	UEdGraphPin* GetOutputPin() const;

	//~ Begin UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* InTargetGraph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type InTitleType) const override;
	//~ End UEdGraphNode

	//~ Begin UK2Node
	virtual void ExpandNode(FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End UK2Node
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node_SceneStateEventBase.h"
#include "K2Node_SceneStatePushEvent.generated.h"

UCLASS()
class UK2Node_SceneStatePushEvent : public UK2Node_SceneStateEventBase
{
	GENERATED_BODY()

public:
	UK2Node_SceneStatePushEvent();

protected:
	//~ Begin UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type InTitleType) const override;
	//~ End UEdGraphNode

	//~ Begin UK2Node
	virtual void ExpandNode(FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph) override;
	//~ End UK2Node
};

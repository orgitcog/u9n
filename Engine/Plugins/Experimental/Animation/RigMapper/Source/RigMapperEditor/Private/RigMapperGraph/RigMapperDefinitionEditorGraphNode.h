// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "EdGraph/EdGraphNode.h"

#include "RigMapperDefinitionEditorGraphNode.generated.h"

class URigMapperDefinition;

UENUM()
enum class ERigMapperNodeType : uint8
{
	Input,
	WeightedSum,
	SDK,
	Multiply,
	Output,
	NullOutput,
	Invalid
};

/**
 * 
 */
UCLASS()
class RIGMAPPEREDITOR_API URigMapperDefinitionEditorGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	class NodeFactory : public FGraphPanelNodeFactory
	{
		virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override;
	};

	
	// UEdGraphNode implementation
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return NodeTitle; }
	virtual FLinearColor GetNodeBodyTintColor() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// !UEdGraphNode implementation

	void SetupNode(URigMapperDefinition* InDefinition, const FString& InNodeName, ERigMapperNodeType InNodeType);
	const FString& GetNodeName() const { return NodeName; }
	
	void SetNodeType(ERigMapperNodeType InNodeType);
	ERigMapperNodeType GetNodeType() const { return NodeType; }
	
	const FText& GetSubtitle() const { return NodeSubtitle; }
	
	UEdGraphPin* CreateInputPin();
	UEdGraphPin* CreateOutputPin();
	const TArray<UEdGraphPin*>& GetInputPins() const { return InputPins; }
	UEdGraphPin* GetOutputPin() const { return OutputPins.IsEmpty() ? nullptr : OutputPins[0]; };

	void SetDimensions(const FVector2D& InDimensions) { Dimensions = InDimensions; }
	const FVector2D& GetDimensions() const { return Dimensions; }

	void SetMargin(const FVector2D& InDMargin) { Margin = InDMargin; }
	const FVector2D& GetMargin() const { return Margin; }

	void GetRect(FVector2D& TopLeft, FVector2D& BottomRight) const;

protected:
	/** Cached title for the node */
	FText NodeTitle;

	/** Cached title for the node */
	FText NodeSubtitle;

	/** Our one input pin */
	TArray<UEdGraphPin*> InputPins;

	/** Our one output pin */
	TArray<UEdGraphPin*> OutputPins;

	/** Cached dimensions of this node (used for layout) */
	FVector2D Dimensions = { 300, 50 };

	/** Cached dimensions of this node (used for layout) */
	FVector2D Margin = { 0, 0 };
	
	TWeakObjectPtr<URigMapperDefinition> Definition;

	FString NodeName;

	ERigMapperNodeType NodeType = ERigMapperNodeType::Invalid;
};

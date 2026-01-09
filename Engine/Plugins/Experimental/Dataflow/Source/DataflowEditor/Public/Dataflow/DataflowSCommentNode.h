// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"
#include "SGraphNode.h"
#include "SGraphNodeComment.h"

#include "DataflowSCommentNode.generated.h"

//
// SDataflowEdNodeComment
//

class DATAFLOWEDITOR_API SDataflowEdNodeComment : public SGraphNodeComment
{
	typedef SGraphNodeComment Super;

public:
	SLATE_BEGIN_ARGS(SDataflowEdNodeComment) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode_Comment* InNode) { Super::Construct(SGraphNodeComment::FArguments(), InNode); }
};

//
// Action to add a node to the graph
//
USTRUCT()
struct DATAFLOWEDITOR_API FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

public:
	FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode(const TSharedPtr<SGraphEditor>& InGraphEditor) : FEdGraphSchemaAction()
		, GraphEditor(InGraphEditor)
		{}

	FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode() {}

	static TSharedPtr<FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode> CreateAction(UEdGraph* ParentGraph, const TSharedPtr<SGraphEditor>& GraphEditor);

	using FEdGraphSchemaAction::PerformAction; // Prevent hiding of deprecated base class function with FVector2D
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2f& Location, bool bSelectNewNode = true) override;
	//virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	TSharedPtr<SGraphEditor> GraphEditor;
};

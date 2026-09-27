// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphEditor.h"
#include "Widgets/SCompoundWidget.h"

class URigMapperDefinitionEditorGraphNode;
class URigMapperDefinition;
class URigMapperDefinitionEditorGraph;

/**
 * 
 */
class RIGMAPPEREDITOR_API SRigMapperDefinitionGraphEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRigMapperDefinitionGraphEditor)
		{
		}

	SLATE_END_ARGS()

	virtual ~SRigMapperDefinitionGraphEditor() override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, URigMapperDefinition* InDefinition);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SelectNodes(const TArray<FString>& Inputs, const TArray<FString>& Features, const TArray<FString>& Outputs, const TArray<FString>& NullOutputs);
	void RebuildGraph();

private:
	void ZoomToFitNodes(const TArray<URigMapperDefinitionEditorGraphNode*>& SelectedNodes) const;
	static void GetAllLinkedNodes(const URigMapperDefinitionEditorGraphNode* BaseNode, TArray<URigMapperDefinitionEditorGraphNode*>& LinkedNodes, bool bDescend);
	void HandleSelectionChanged(const TSet<UObject*>& Nodes);

public:
	SGraphEditor::FOnSelectionChanged OnSelectionChanged;
	
private:
	TSharedPtr<SGraphEditor> GraphEditor;
	TObjectPtr<URigMapperDefinitionEditorGraph> GraphObj;

	bool bSelectingNodes = false;

	bool bFocusLinkedNodes = true;

	bool bDeferZoom = false;
};

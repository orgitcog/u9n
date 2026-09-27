// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

class URigMapperDefinitionEditorGraphNode;

/**
 * 
 */
class RIGMAPPEREDITOR_API SRigMapperDefinitionGraphEditorNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SRigMapperDefinitionGraphEditorNode)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, URigMapperDefinitionEditorGraphNode* InNode);

protected:
	FSlateColor GetNodeColor() const;
	FText GetNodeTitle() const;
	FText GetNodeSubtitle() const;
	EVisibility GetShowNodeSubtitle() const { return GetNodeSubtitle().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; };
	
	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
};

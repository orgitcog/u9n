// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRigMapperDefinitionGraphEditorNode.h"

#include "RigMapperDefinitionEditorGraphNode.h"
#include "SlateOptMacros.h"
#include "Components/HorizontalBox.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRigMapperDefinitionGraphEditorNode::Construct(const FArguments& InArgs, URigMapperDefinitionEditorGraphNode* InNode)
{
	GraphNode = InNode;
	SetCursor(EMouseCursor::CardinalCross);

	// if (!ContentWidget.IsValid())
	// {
	// 	ContentWidget = SNullWidget::NullWidget;
	// }
	
	UpdateGraphNode();
}

FSlateColor SRigMapperDefinitionGraphEditorNode::GetNodeColor() const
{
	if (GraphNode)
	{
		return FSlateColor(GraphNode->GetNodeTitleColor());
	}
	return FSlateColor(FLinearColor::Red);
}

FText SRigMapperDefinitionGraphEditorNode::GetNodeTitle() const
{
	if (GraphNode)
	{
		return GraphNode->GetNodeTitle(ENodeTitleType::FullTitle);
	}
	return FText::FromString("Invalid");
}

FText SRigMapperDefinitionGraphEditorNode::GetNodeSubtitle() const
{
	if (const URigMapperDefinitionEditorGraphNode* Node = Cast<URigMapperDefinitionEditorGraphNode>(GraphNode))
	{
		return Node->GetSubtitle();
	}
	return FText::GetEmpty();
}


void SRigMapperDefinitionGraphEditorNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );

	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::GetBrush("PhysicsAssetEditor.Graph.NodeBody") )
			.BorderBackgroundColor(this, &SRigMapperDefinitionGraphEditorNode::GetNodeColor)
			.Padding(0)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SAssignNew(LeftNodeBox, SVerticalBox)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(4.0f)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(8, 0)
					[
						SNew(STextBlock)
						.TextStyle( FAppStyle::Get(), "Graph.Node.NodeTitle" )
						.ColorAndOpacity(FSlateColor(FLinearColor::Black))
						.Text(this, &SRigMapperDefinitionGraphEditorNode::GetNodeTitle)
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "NormalText")
						.ColorAndOpacity(FSlateColor(FLinearColor::Black))
						.Text(this, &SRigMapperDefinitionGraphEditorNode::GetNodeSubtitle)
						.Visibility(this, &SRigMapperDefinitionGraphEditorNode::GetShowNodeSubtitle)
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		];

	CreatePinWidgets();
}

const FSlateBrush* SRigMapperDefinitionGraphEditorNode::GetShadowBrush(bool bSelected) const
{
	return bSelected ? FAppStyle::GetBrush(TEXT("PhysicsAssetEditor.Graph.Node.ShadowSelected")) : FAppStyle::GetBrush(TEXT("PhysicsAssetEditor.Graph.Node.Shadow"));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

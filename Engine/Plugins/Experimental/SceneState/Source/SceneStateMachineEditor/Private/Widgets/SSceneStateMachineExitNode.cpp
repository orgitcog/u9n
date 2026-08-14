// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSceneStateMachineExitNode.h"
#include "Nodes/SceneStateMachineExitNode.h"
#include "SGraphPin.h"
#include "SSceneStateMachineEntryPin.h"
#include "SceneStateMachineEditorStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

namespace UE::SceneState::Editor
{

void SStateMachineExitNode::Construct(const FArguments& InArgs, USceneStateMachineExitNode* InNode)
{
	GraphNode = InNode;
	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

void SStateMachineExitNode::UpdateGraphNode()
{
	Super::UpdateGraphNode();

	const FStateMachineEditorStyle& Style = FStateMachineEditorStyle::Get();

	GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(Style.GetBrush("EntryNode.OuterBorder"))
			.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f))
			.Padding(0)
			[
				SNew(SOverlay)
				// Pin Area
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(10.0f)
				[
					SNew(SBorder)
					.BorderImage(Style.GetBrush("EntryNode.InnerBorder"))
					.BorderBackgroundColor(Style.GetColor("NodeColor.Exit"))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(5, 2)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetWidgetStyle<FTextBlockStyle>("Graph.StateNode.NodeTitle").Font)
						.Text(GraphNode->GetNodeTitle(ENodeTitleType::FullTitle))
					]
				]
			]			
		];

	CreatePinWidgets();
}

const FSlateBrush* SStateMachineExitNode::GetShadowBrush(bool bInSelected) const
{
	const FStateMachineEditorStyle& Style = FStateMachineEditorStyle::Get();

	return bInSelected
		? Style.GetBrush("EntryNode.ShadowSelected")
		: Style.GetBrush("EntryNode.Shadow");
}

} // UE::SceneState::Editor

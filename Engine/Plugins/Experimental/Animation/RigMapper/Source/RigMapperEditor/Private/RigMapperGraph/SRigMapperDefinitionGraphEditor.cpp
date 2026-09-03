// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRigMapperDefinitionGraphEditor.h"

#include "RigMapperDefinitionEditorGraph.h"
#include "RigMapperDefinitionEditorGraphNode.h"
#include "RigMapperDefinitionEditorGraphSchema.h"

#include "SGraphPanel.h"
#include "SlateOptMacros.h"
#include "Components/VerticalBox.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

#define LOCTEXT_NAMESPACE "SRigMapperDefinitionGraphEditor"

SRigMapperDefinitionGraphEditor::~SRigMapperDefinitionGraphEditor()
{
	if (!GExitPurge)
	{
		if (ensure(GraphObj))
		{
			GraphObj->RemoveFromRoot();
		}		
	}
}

void SRigMapperDefinitionGraphEditor::Construct(const FArguments& InArgs, URigMapperDefinition* InDefinition)
{
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("GraphEditorRigMapperDefinition", "Rig Mapper Definition");

	SGraphEditor::FGraphEditorEvents GraphEvents;
	// GraphEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &SPhysicsAssetGraph::OnCreateGraphActionMenu);
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SRigMapperDefinitionGraphEditor::HandleSelectionChanged);
	// GraphEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &SPhysicsAssetGraph::HandleNodeDoubleClicked);

	GraphObj = NewObject<URigMapperDefinitionEditorGraph>();
	GraphObj->AddToRoot();
	GraphObj->Initialize(InDefinition);
	GraphObj->RebuildGraph();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(GraphEditor, SGraphEditor)
			.GraphToEdit(GraphObj)
			.GraphEvents(GraphEvents)
			.Appearance(AppearanceInfo)
			.ShowGraphStateOverlay(false)
			.IsEditable(false)
			.AutoExpandActionMenu(false)
			.DisplayAsReadOnly(false)
		]
	];

}

void SRigMapperDefinitionGraphEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (GraphObj->NeedsRefreshLayout())
	{		
		GraphObj->LayoutNodes();

		GraphObj->RequestRefreshLayout(false);

		const FGraphPanelSelectionSet& Selection = GraphEditor->GetSelectedNodes();
		
		TArray<URigMapperDefinitionEditorGraphNode*> SelectedNodes;
		SelectedNodes.Reserve(Selection.Num());
		
		for (UObject* Obj : Selection)
		{
			if (URigMapperDefinitionEditorGraphNode* Node = Cast<URigMapperDefinitionEditorGraphNode>(Obj))
			{
				SelectedNodes.Add(Node);
			}
		}
		
		ZoomToFitNodes(SelectedNodes);
	}
}

void SRigMapperDefinitionGraphEditor::SelectNodes(const TArray<FString>& Inputs, const TArray<FString>& Features, const TArray<FString>& Outputs, const TArray<FString>& NullOutputs)
{
	if (!bSelectingNodes)
	{
		bSelectingNodes = true;
		
		GraphEditor->ClearSelectionSet();

		const TArray<URigMapperDefinitionEditorGraphNode*>& SelectedNodes = GraphObj->GetNodesByName(Inputs, Features, Outputs, NullOutputs); 
		
		for (URigMapperDefinitionEditorGraphNode* Node : SelectedNodes)
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
		ZoomToFitNodes(SelectedNodes);
		
		bSelectingNodes = false;
	}
}

void SRigMapperDefinitionGraphEditor::RebuildGraph()
{
	GraphObj->RebuildGraph();
}

void SRigMapperDefinitionGraphEditor::ZoomToFitNodes(const TArray<URigMapperDefinitionEditorGraphNode*>& SelectedNodes) const
{
	if (GraphObj->NeedsRefreshLayout())
	{
		return;
	}
	if (!bFocusLinkedNodes)
	{
		GraphEditor->ZoomToFit(true);
	}
	else
	{
		// todo: Allow option to show only selected graph/tree nodes
		// todo  GraphEditor->GetGraphPanel()->GetNodeWidgetFromGuid(Node->NodeGuid)->SetVisibility()
		
		if (!SelectedNodes.IsEmpty() && !GraphEditor->GetGraphPanel()->HasDeferredZoomDestination())
		{
			FVector2D MinCorner(MAX_FLT, MAX_FLT);
			FVector2D MaxCorner(-MAX_FLT, -MAX_FLT);

			const FVector2D MaxLinkedNodeOffset(600, 400);
			
			for (UObject* Obj : SelectedNodes)
			{
				URigMapperDefinitionEditorGraphNode* Node = Cast<URigMapperDefinitionEditorGraphNode>(Obj);

				FVector2D NodeTopLeft;
				FVector2D NodeBottomRight;
				Node->GetRect(NodeTopLeft, NodeBottomRight);
				
				TArray<URigMapperDefinitionEditorGraphNode*> LinkedNodes = { Node };
				GetAllLinkedNodes(Node, LinkedNodes, true);
				GetAllLinkedNodes(Node, LinkedNodes, false);

				for (URigMapperDefinitionEditorGraphNode* LinkedNode : LinkedNodes)
				{
					FVector2D LinkedNodeTopLeft;
					FVector2D LinkedNodeBottomRight;
					LinkedNode->GetRect(LinkedNodeTopLeft, LinkedNodeBottomRight);
					
					MinCorner.X = FMath::Min(MinCorner.X, FMath::Max(LinkedNodeTopLeft.X, NodeTopLeft.X - MaxLinkedNodeOffset.X));
					MinCorner.Y = FMath::Min(MinCorner.Y, FMath::Max(LinkedNodeTopLeft.Y, NodeTopLeft.Y - MaxLinkedNodeOffset.Y));
					MaxCorner.X = FMath::Max(MaxCorner.X, FMath::Min(LinkedNodeBottomRight.X, NodeBottomRight.X + MaxLinkedNodeOffset.X));
					MaxCorner.Y = FMath::Max(MaxCorner.Y, FMath::Min(LinkedNodeBottomRight.Y, NodeBottomRight.Y + MaxLinkedNodeOffset.Y));			
				}
			}
			GraphEditor->GetGraphPanel()->JumpToRect(MinCorner, MaxCorner);
		}
	}
}

void SRigMapperDefinitionGraphEditor::GetAllLinkedNodes(const URigMapperDefinitionEditorGraphNode* BaseNode, TArray<URigMapperDefinitionEditorGraphNode*>& LinkedNodes, bool bDescend)
{
	
	TArray<UEdGraphPin*> Pins;
	if (bDescend)
	{
		Pins = BaseNode->GetInputPins();
	}
	else
	{
		Pins = { BaseNode->GetOutputPin() };
	}
	
	for (const UEdGraphPin* PinA : Pins)
	{
		if (PinA)
		{
			for (const UEdGraphPin* PinB : PinA->LinkedTo)
			{
				if (PinB)
				{
					URigMapperDefinitionEditorGraphNode* LinkedNode = Cast<URigMapperDefinitionEditorGraphNode>(PinB->GetOwningNode());

					if (LinkedNode && !LinkedNodes.Contains(LinkedNode))
					{
						LinkedNodes.Add(LinkedNode);
						GetAllLinkedNodes(LinkedNode, LinkedNodes, bDescend);
					}
				}
			}
		}
	}
}

void SRigMapperDefinitionGraphEditor::HandleSelectionChanged(const TSet<UObject*>& Nodes)
{
	if (!bSelectingNodes)
	{
		bSelectingNodes = true;
		
		if (OnSelectionChanged.IsBound())
		{
			OnSelectionChanged.Execute(Nodes);
		}

		TArray<URigMapperDefinitionEditorGraphNode*> SelectedNodes;

		for (UObject* Obj : Nodes)
		{
			if (URigMapperDefinitionEditorGraphNode* Node = Cast<URigMapperDefinitionEditorGraphNode>(Obj))
			{
				SelectedNodes.Add(Node);
			}
		}
		
		ZoomToFitNodes(SelectedNodes);
		
		bSelectingNodes = false;
	}
}


#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

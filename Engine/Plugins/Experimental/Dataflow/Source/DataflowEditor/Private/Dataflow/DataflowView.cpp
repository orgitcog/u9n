// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowView.h"

#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowSelection.h"
#include "Templates/EnableIf.h"

#define LOCTEXT_NAMESPACE "DataflowView"

FDataflowNodeView::FDataflowNodeView(TObjectPtr<UDataflowBaseContent> InContent)
	: FGCObject()
	, EditorContent(InContent)
{
}

TObjectPtr<UDataflowBaseContent> FDataflowNodeView::GetEditorContent()
{
	if (ensure(EditorContent))
	{
		return EditorContent;
	}
	return nullptr;
}

bool FDataflowNodeView::SelectedNodeHaveSupportedOutputTypes(UDataflowEdNode* InNode)
{
	SetSupportedOutputTypes();

	if (InNode->IsBound())
	{
		if (TSharedPtr<FDataflowNode> DataflowNode = InNode->DataflowGraph->FindBaseNode(InNode->DataflowNodeGuid))
		{
			TArray<FDataflowOutput*> Outputs = DataflowNode->GetOutputs();

			for (FDataflowOutput* Output : Outputs)
			{
				for (const FString& OutputType : SupportedOutputTypes)
				{
					if (Output->GetType() == FName(*OutputType))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void FDataflowNodeView::OnConstructionViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements)
{
	ConstructionViewSelectionChanged(SelectedComponents, SelectedElements);
}

void FDataflowNodeView::OnSimulationViewSelectionChanged(const TArray<UPrimitiveComponent*>& SelectedComponents, const TArray<FDataflowBaseElement*>& SelectedElements)
{
	SimulationViewSelectionChanged(SelectedComponents, SelectedElements);
}

void FDataflowNodeView::OnSelectedNodeChanged(UDataflowEdNode* InNode)
{
	if (!bIsPinnedDown)
	{
		SelectedNode = nullptr;

		if (InNode)  // nullptr is valid
		{
			if (SelectedNodeHaveSupportedOutputTypes(InNode))
			{
				SelectedNode = InNode;
			}
		}

		UpdateViewData();
	}
}

void FDataflowNodeView::RefreshView()
{
	if (!bIsRefreshLocked && SelectedNode)
	{
		UpdateViewData();
	}
}

void FDataflowNodeView::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SelectedNode);
	if (EditorContent)
	{
		Collector.AddReferencedObject(EditorContent);
	}
}


#undef LOCTEXT_NAMESPACE

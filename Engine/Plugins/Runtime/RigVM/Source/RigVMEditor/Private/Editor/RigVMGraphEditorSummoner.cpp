// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMGraphEditorSummoner.h"
#include "Editor/RigVMNewEditor.h"
#include "GraphEditor.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "RigVMGraphEditorSummoner"

FText FRigVMLocalKismetCallbacks::GetGraphDisplayName(const UEdGraph* Graph)
{
	if (Graph)
	{
		if (const UEdGraphSchema* Schema = Graph->GetSchema())
		{
			FGraphDisplayInfo Info;
			Schema->GetGraphDisplayInformation(*Graph, /*out*/ Info);

			return Info.DisplayName;
		}
		else
		{
			// if we don't have a schema, we're dealing with a malformed (or incomplete graph)...
			// possibly in the midst of some transaction - here we return the object's outer path 
			// so we can at least get some context as to which graph we're referring
			return FText::FromString(Graph->GetPathName());
		}
	}

	return LOCTEXT("UnknownGraphName", "UNKNOWN");
}

FRigVMGraphEditorSummoner::FRigVMGraphEditorSummoner(TSharedPtr<FRigVMNewEditor> InEditorPtr, FOnCreateGraphEditorWidget CreateGraphEditorWidgetCallback)
: FDocumentTabFactoryForObjects<UEdGraph>(TabID, InEditorPtr)
, BlueprintEditorPtr(InEditorPtr)
, OnCreateGraphEditorWidget(CreateGraphEditorWidgetCallback)
{
}

void FRigVMGraphEditorSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	BlueprintEditorPtr.Pin()->OnGraphEditorFocused(GraphEditor);
}

void FRigVMGraphEditorSummoner::OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	BlueprintEditorPtr.Pin()->OnGraphEditorBackgrounded(GraphEditor);
}

TSharedRef<SWidget> FRigVMGraphEditorSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	check(Info.TabInfo.IsValid());
	return OnCreateGraphEditorWidget.Execute(Info.TabInfo.ToSharedRef(), DocumentID);
}

const FSlateBrush* FRigVMGraphEditorSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	return FRigVMNewEditor::GetGlyphForGraph(DocumentID, false);
}

#undef LOCTEXT_NAMESPACE
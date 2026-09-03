// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "RigVMGraphEditorSummoner"

class UEdGraphSchema;
struct FGraphDisplayInfo;
class SGraphEditor;

struct FRigVMLocalKismetCallbacks
{
	static FText GetGraphDisplayName(const UEdGraph* Graph);
};

struct FRigVMGraphEditorSummoner : public FDocumentTabFactoryForObjects<UEdGraph>
{
	static inline const FLazyName TabID = FLazyName(TEXT("RigVM Graph Editor"));
	
public:
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<SGraphEditor>, FOnCreateGraphEditorWidget, TSharedRef<FTabInfo>, UEdGraph*);

	FRigVMGraphEditorSummoner(TSharedPtr<class FRigVMNewEditor> InEditorPtr, FOnCreateGraphEditorWidget CreateGraphEditorWidgetCallback);

	virtual void OnTabActivated(TSharedPtr<SDockTab> Tab) const override;
	
	virtual void OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const override;

	protected:
	virtual TAttribute<FText> ConstructTabNameForObject(UEdGraph* DocumentID) const override
	{
		return TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&FRigVMLocalKismetCallbacks::GetGraphDisplayName, (const UEdGraph*)DocumentID));
	}

	virtual TSharedRef<SWidget> CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;

	virtual const FSlateBrush* GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;

	protected:
	TWeakPtr<class FRigVMNewEditor> BlueprintEditorPtr;
	FOnCreateGraphEditorWidget OnCreateGraphEditorWidget;
};

#undef LOCTEXT_NAMESPACE
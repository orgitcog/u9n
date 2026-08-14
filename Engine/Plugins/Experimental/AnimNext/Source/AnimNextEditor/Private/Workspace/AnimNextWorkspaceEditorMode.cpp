// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceEditorMode.h"

#include "AnimNextEditorContext.h"
#include "AnimNextEditorModule.h"
#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "AnimNextScopedCompilerResults.h"
#include "AnimNextWorkspaceSchema.h"
#include "ContextObjectStore.h"
#include "IAssetCompilationHandler.h"
#include "InteractiveToolManager.h"
#include "IWorkspaceEditor.h"
#include "RigVMCommands.h"
#include "UncookedOnlyUtils.h"
#include "Logging/MessageLog.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/BaseToolkit.h"

#define LOCTEXT_NAMESPACE "AnimNextWorkspaceEditorMode"

const FEditorModeID UAnimNextWorkspaceEditorMode::EM_AnimNextWorkspace("AnimNextWorkspace");

UAnimNextWorkspaceEditorMode::UAnimNextWorkspaceEditorMode()
{
	Info = FEditorModeInfo(UAnimNextWorkspaceEditorMode::EM_AnimNextWorkspace,
		LOCTEXT("AnimNextWorkspaceEditorModeName", "AnimNextWorkspaceEditorMode"),
		FSlateIcon(),
		false);
}

void UAnimNextWorkspaceEditorMode::Enter()
{
	Super::Enter();

	TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = GetWorkspaceEditor();
	if(!WorkspaceEditor.IsValid())
	{
		return;
	}
	
	WorkspaceEditor->OnFocussedDocumentChanged().AddUObject(this, &UAnimNextWorkspaceEditorMode::HandleFocussedDocumentChanged);

	TArray<UObject*> Assets;
	WorkspaceEditor->GetOpenedAssets<UAnimNextRigVMAsset>(Assets);
	for(UObject* Asset : Assets)
	{
		UAnimNextRigVMAsset* AnimNextRigVMAsset = static_cast<UAnimNextRigVMAsset*>(Asset);
		UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);

		EditorData->RigVMCompiledEvent.RemoveAll(this);
		EditorData->RigVMCompiledEvent.AddUObject(this, &UAnimNextWorkspaceEditorMode::HandleRigVMCompiledEvent);
		EditorData->RigVMGraphModifiedEvent.RemoveAll(this);
		EditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextWorkspaceEditorMode::HandleRigVMModifiedEvent);

		WeakAssets.Add(Asset);
	}

	UpdateCompileStatus();
}

void UAnimNextWorkspaceEditorMode::Exit()
{
	Super::Exit();

	for (TPair<FObjectKey, TSharedRef<UE::AnimNext::Editor::IAssetCompilationHandler>>& AssetCompiler : AssetCompilers)
	{
		TSharedRef<UE::AnimNext::Editor::IAssetCompilationHandler> AssetCompilerRef = AssetCompiler.Value;
		AssetCompilerRef->OnCompileStatusChanged().Unbind();
	}

	AssetCompilers.Reset();

	for(TObjectKey<UObject> Asset : WeakAssets)
	{
		UAnimNextRigVMAsset* AnimNextRigVMAsset = Cast<UAnimNextRigVMAsset>(Asset.ResolveObjectPtr());
		if(AnimNextRigVMAsset == nullptr)
		{
			continue;;
		}
		
		UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);

		EditorData->RigVMCompiledEvent.RemoveAll(this);
		EditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	}

	WeakAssets.Reset();
}

void UAnimNextWorkspaceEditorMode::BindCommands()
{
	Super::BindCommands();

	TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = GetWorkspaceEditor();
	if(!WorkspaceEditor.IsValid())
	{
		return;
	}

	TSharedRef<FUICommandList> ToolkitCommands = WorkspaceEditor->GetToolkitCommands();

	const UE::AnimNext::FRigVMCommands& RigVMCommands = UE::AnimNext::FRigVMCommands::Get();
	ToolkitCommands->MapAction(RigVMCommands.Compile,
		FExecuteAction::CreateUObject(this, &UAnimNextWorkspaceEditorMode::HandleCompile));

	ToolkitCommands->MapAction(RigVMCommands.AutoCompile,
		FExecuteAction::CreateUObject(this, &UAnimNextWorkspaceEditorMode::HandleAutoCompile),
		FCanExecuteAction(),
		FIsActionChecked::CreateUObject(this, &UAnimNextWorkspaceEditorMode::IsAutoCompileChecked));

	ToolkitCommands->MapAction(RigVMCommands.CompileWholeWorkspace,
		FExecuteAction::CreateUObject(this, &UAnimNextWorkspaceEditorMode::HandleCompileWholeWorkspace),
		FCanExecuteAction(),
		FIsActionChecked::CreateUObject(this, &UAnimNextWorkspaceEditorMode::IsCompileWholeWorkspaceChecked));

	ToolkitCommands->MapAction(RigVMCommands.CompileDirtyFiles,
		FExecuteAction::CreateUObject(this, &UAnimNextWorkspaceEditorMode::HandleCompileDirtyFiles),
		FCanExecuteAction(),
		FIsActionChecked::CreateUObject(this, &UAnimNextWorkspaceEditorMode::IsCompileDirtyFilesChecked));
}

void UAnimNextWorkspaceEditorMode::HandleCompile()
{
	TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = GetWorkspaceEditor();
	if(!WorkspaceEditor.IsValid())
	{
		return;
	}

	TArray<UObject*> Assets;

	UObject* Asset = nullptr;
	FText JobName = LOCTEXT("DefaultJobName", "Job");
	if(IsCompileWholeWorkspaceChecked())
	{
		WorkspaceEditor->GetOpenedAssets<UObject>(Assets);
		if(Assets.Num() == 1)
		{
			Asset = Assets[0];
			JobName = FText::FromName(Asset->GetFName());
		}
		else
		{
			Asset = WorkspaceEditor->GetWorkspaceAsset();
			JobName = FText::FromName(Asset->GetFName());
		}
	}
	else if(UObject* FocussedDocument = WorkspaceEditor->GetFocussedDocument())
	{
		// Find the outer asset for this document
		Asset = FocussedDocument;
		
		while(Asset && (!Asset->IsAsset() || Asset->IsPackageExternal()))
		{
			Asset = Asset->GetOuter();
		}

		if(Asset)
		{
			JobName = FText::FromName(Asset->GetFName());
			Assets.Add(Asset);
		}
	}

	if(IsCompileDirtyFilesChecked())
	{
		for(int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
		{
			UObject* AssetToCheck = Assets[AssetIndex];
			if(TSharedPtr<UE::AnimNext::Editor::IAssetCompilationHandler> FoundCompiler = GetAssetCompiler(AssetToCheck))
			{
				UE::AnimNext::Editor::ECompileStatus AssetCompileStatus = FoundCompiler->GetCompileStatus(WorkspaceEditor.ToSharedRef(), AssetToCheck);
				if( AssetCompileStatus != UE::AnimNext::Editor::ECompileStatus::Dirty &&
					AssetCompileStatus != UE::AnimNext::Editor::ECompileStatus::Error)
				{
					Assets.RemoveAtSwap(AssetIndex);
					AssetIndex--;
				}
			}
		}
	}

	if(Assets.Num() == 0)
	{
		return;
	}

	// Start a batch compile scope
	UE::AnimNext::UncookedOnly::FScopedCompilerResults CompileResults(JobName, Asset, Assets);

	UE::AnimNext::Editor::FAnimNextEditorModule& AnimNextEditorModule = FModuleManager::LoadModuleChecked<UE::AnimNext::Editor::FAnimNextEditorModule>("AnimNextEditor");
	for(UObject* AssetToCompile : Assets)
	{
		if(TSharedPtr<UE::AnimNext::Editor::IAssetCompilationHandler> FoundCompiler = GetAssetCompiler(AssetToCompile))
		{
			FoundCompiler->Compile(WorkspaceEditor.ToSharedRef(), AssetToCompile);
		}
	}
}

void UAnimNextWorkspaceEditorMode::HandleAutoCompile()
{
	State.bAutoCompile = !State.bAutoCompile;

	TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = GetWorkspaceEditor();
	if(WorkspaceEditor.IsValid())
	{
		PropagateAutoCompile(WorkspaceEditor.ToSharedRef(), State.bAutoCompile);
	}
}

void UAnimNextWorkspaceEditorMode::PropagateAutoCompile(TSharedRef<UE::Workspace::IWorkspaceEditor> InWorkspaceEditor, bool bInAutoCompile)
{
	TArray<UObject*> Assets;
	InWorkspaceEditor->GetOpenedAssets(Assets);
	UE::AnimNext::Editor::FAnimNextEditorModule& AnimNextEditorModule = FModuleManager::LoadModuleChecked<UE::AnimNext::Editor::FAnimNextEditorModule>("AnimNextEditor");
	for(UObject* Asset : Assets)
	{
		if(TSharedPtr<UE::AnimNext::Editor::IAssetCompilationHandler> FoundCompiler = GetAssetCompiler(Asset))
		{
			FoundCompiler->SetAutoCompile(InWorkspaceEditor, Asset, bInAutoCompile);
		}
	}
}

bool UAnimNextWorkspaceEditorMode::IsAutoCompileChecked() const
{
	return State.bAutoCompile;
}

void UAnimNextWorkspaceEditorMode::HandleCompileWholeWorkspace()
{
	State.bCompileWholeWorkspace = !State.bCompileWholeWorkspace;
}

bool UAnimNextWorkspaceEditorMode::IsCompileWholeWorkspaceChecked() const
{
	return State.bCompileWholeWorkspace;
}

void UAnimNextWorkspaceEditorMode::HandleCompileDirtyFiles()
{
	State.bCompileDirtyFiles = !State.bCompileDirtyFiles;
}

bool UAnimNextWorkspaceEditorMode::IsCompileDirtyFilesChecked() const
{
	return State.bCompileDirtyFiles;
}

void UAnimNextWorkspaceEditorMode::HandleFocussedDocumentChanged(TObjectPtr<UObject> InObject)
{
	if(InObject == nullptr)
	{
		return;
	}

	UAnimNextRigVMAsset* AnimNextRigVMAsset = Cast<UAnimNextRigVMAsset>(InObject);
	if(AnimNextRigVMAsset == nullptr)
	{
		AnimNextRigVMAsset = InObject->GetTypedOuter<UAnimNextRigVMAsset>();
	}
	
	if(AnimNextRigVMAsset == nullptr)
	{
		return;
	}

	// Ensure auto-compilation is propagated for newly opened asset 
	UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(AnimNextRigVMAsset);
	EditorData->SetAutoVMRecompile(State.bAutoCompile);

	// Subscribe to asset compilation/modification
	EditorData->RigVMCompiledEvent.RemoveAll(this);
	EditorData->RigVMCompiledEvent.AddUObject(this, &UAnimNextWorkspaceEditorMode::HandleRigVMCompiledEvent);
	EditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	EditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextWorkspaceEditorMode::HandleRigVMModifiedEvent);

	WeakAssets.Add(AnimNextRigVMAsset);
}

void UAnimNextWorkspaceEditorMode::UpdateCompileStatus()
{
	bUpdateCompileStatus = false;

	TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = GetWorkspaceEditor();
	if(!WorkspaceEditor.IsValid())
	{
		return;
	}

	int32 NumAssetsWithWarnings = 0;
	int32 NumAssetsWithErrors = 0;
	int32 NumAssetsDirty = 0;

	TArray<UObject*> Assets;
	WorkspaceEditor->GetOpenedAssets<UAnimNextRigVMAsset>(Assets);

	for(UObject* Asset : Assets)
	{
		if(TSharedPtr<UE::AnimNext::Editor::IAssetCompilationHandler> FoundCompiler = GetAssetCompiler(Asset))
		{
			UE::AnimNext::Editor::ECompileStatus AssetCompileStatus = FoundCompiler->GetCompileStatus(WorkspaceEditor.ToSharedRef(), Asset);
			if(AssetCompileStatus == UE::AnimNext::Editor::ECompileStatus::Dirty)
			{
				NumAssetsDirty++;
			}

			if(AssetCompileStatus == UE::AnimNext::Editor::ECompileStatus::Error)
			{
				NumAssetsWithErrors++;
			}

			if(AssetCompileStatus == UE::AnimNext::Editor::ECompileStatus::Warning)
			{
				NumAssetsWithWarnings++;
			}
		}
	}

	if(NumAssetsWithErrors > 0)
	{
		CompileStatus = UE::AnimNext::Editor::ECompileStatus::Error;
	}
	else if(NumAssetsWithWarnings > 0)
	{
		CompileStatus = UE::AnimNext::Editor::ECompileStatus::Warning;
	}
	else if(NumAssetsDirty > 0)
	{
		CompileStatus = UE::AnimNext::Editor::ECompileStatus::Dirty;
	}
	else
	{
		CompileStatus = UE::AnimNext::Editor::ECompileStatus::UpToDate;
	}
}

void UAnimNextWorkspaceEditorMode::HandleRigVMCompiledEvent(UObject* InAsset, URigVM* InVM, FRigVMExtendedExecuteContext& InExtendedExecuteContext)
{
	UpdateCompileStatus();
}

void UAnimNextWorkspaceEditorMode::HandleRigVMModifiedEvent(ERigVMGraphNotifType InType, URigVMGraph* InGraph, UObject* InSubject)
{
	if(InGraph == nullptr)
	{
		return;
	}

	UAnimNextRigVMAssetEditorData* EditorData = InGraph->GetTypedOuter<UAnimNextRigVMAssetEditorData>();
	if(EditorData->IsDirtyForRecompilation())
	{
		CompileStatus = UE::AnimNext::Editor::ECompileStatus::Dirty;
	}
}

TSharedPtr<UE::Workspace::IWorkspaceEditor> UAnimNextWorkspaceEditorMode::GetWorkspaceEditor()
{
	if (const UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UAnimNextEditorContext* Context = ContextObjectStore->FindContext<UAnimNextEditorContext>())
		{
			return Context->WeakWorkspaceEditor.Pin();
		}
	}

	return nullptr;
}

TSharedPtr<UE::AnimNext::Editor::IAssetCompilationHandler> UAnimNextWorkspaceEditorMode::GetAssetCompiler(UObject* InAsset)
{
	if(TSharedRef<UE::AnimNext::Editor::IAssetCompilationHandler>* FoundCompiler = AssetCompilers.Find(InAsset))
	{
		return *FoundCompiler;
	}

	UE::AnimNext::Editor::FAnimNextEditorModule& AnimNextEditorModule = FModuleManager::LoadModuleChecked<UE::AnimNext::Editor::FAnimNextEditorModule>("AnimNextEditor");
	if(const UE::AnimNext::Editor::FAssetCompilationHandlerFactoryDelegate* FoundFactory = AnimNextEditorModule.FindAssetCompilationHandlerFactory(InAsset->GetClass()))
	{
		TSharedRef<UE::AnimNext::Editor::IAssetCompilationHandler> NewCompiler = AssetCompilers.Add(InAsset, FoundFactory->Execute(InAsset));
		NewCompiler->OnCompileStatusChanged().BindUObject(this, &UAnimNextWorkspaceEditorMode::UpdateCompileStatus);
		return NewCompiler;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
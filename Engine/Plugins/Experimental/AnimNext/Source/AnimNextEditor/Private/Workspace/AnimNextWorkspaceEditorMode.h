// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextWorkspaceState.h"
#include "EdMode.h"
#include "IAssetCompilationHandler.h"
#include "AnimNextWorkspaceEditorMode.generated.h"

class UAnimNextWorkspaceSchema;
class URigVM;
class URigVMGraph;
struct FRigVMExtendedExecuteContext;
enum class ERigVMGraphNotifType : uint8;

namespace UE::Workspace
{
	class IWorkspaceEditor;
}

UCLASS(Transient)
class UAnimNextWorkspaceEditorMode : public UEdMode
{
	GENERATED_BODY()

public:
	const static FEditorModeID EM_AnimNextWorkspace;

	UAnimNextWorkspaceEditorMode();

	// Get the current compilation status
	UE::AnimNext::Editor::ECompileStatus GetLatestCompileStatus() const { return CompileStatus; }

	// Get the currently stored workspace state
	const FAnimNextWorkspaceState& GetState() { return State; } 

private:
	// UEdMode interface
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void BindCommands() override;
	virtual bool UsesToolkits() const override { return false; }

	void HandleCompile();
	void HandleAutoCompile();
	bool IsAutoCompileChecked() const;
	void HandleCompileWholeWorkspace();
	bool IsCompileWholeWorkspaceChecked() const;
	void HandleCompileDirtyFiles();
	bool IsCompileDirtyFilesChecked() const;

	// Updates the compile status. Scans all opened assets in the workspace.
	void UpdateCompileStatus();

	// Handle an asset in the workspace getting compiled 
	void HandleRigVMCompiledEvent(UObject* InAsset, URigVM* InVM, FRigVMExtendedExecuteContext& InExtendedExecuteContext);

	// Handle an asset getting modified
	void HandleRigVMModifiedEvent(ERigVMGraphNotifType InType, URigVMGraph* InGraph, UObject* InSubject);

	// Handle the document focus changing
	void HandleFocussedDocumentChanged(TObjectPtr<UObject> InAsset); 

	// Forward auto-compilation to all assets in the workspace
	void PropagateAutoCompile(TSharedRef<UE::Workspace::IWorkspaceEditor> InWorkspaceEditor, bool bInAutoCompile);

	TSharedPtr<UE::Workspace::IWorkspaceEditor> GetWorkspaceEditor();

	// Lazily construct an asset compiler for the supplied asset
	TSharedPtr<UE::AnimNext::Editor::IAssetCompilationHandler> GetAssetCompiler(UObject* InAsset);

	friend UAnimNextWorkspaceSchema;

	// Custom state, persisted via UAnimNextWorkspaceSchema
	FAnimNextWorkspaceState State;

	// Asset compilers for all current assets 
	TMap<FObjectKey, TSharedRef<UE::AnimNext::Editor::IAssetCompilationHandler>> AssetCompilers;

	// All assets that we are currently tracking for compilation status
	TSet<TObjectKey<UObject>> WeakAssets;

	// Current compilation status
	UE::AnimNext::Editor::ECompileStatus CompileStatus = UE::AnimNext::Editor::ECompileStatus::Unknown;

	// Flag to update the latest compile status
	bool bUpdateCompileStatus = false;
};
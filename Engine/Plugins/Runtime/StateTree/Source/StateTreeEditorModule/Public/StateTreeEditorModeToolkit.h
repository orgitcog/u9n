// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeEditorMode.h"
#include "IStateTreeEditorHost.h"
#include "Toolkits/BaseToolkit.h"

class UStateTreeEditorMode;
struct FPropertyBindingBindingCollection;
namespace UE::StateTreeEditor
{
	struct FSpawnedWorkspaceTab;
}

class STATETREEEDITORMODULE_API FStateTreeEditorModeToolkit : public FModeToolkit
{
public:

	FStateTreeEditorModeToolkit(UStateTreeEditorMode* InEditorMode);
	
	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual void InvokeUI() override;
	virtual void RequestModeUITabs() override;
	
	virtual void ExtendSecondaryModeToolbar(UToolMenu* InModeToolbarMenu) override;
		
	void OnStateTreeChanged();

protected:
	FSlateIcon GetCompileStatusImage() const;

	static FSlateIcon GetNewTaskButtonImage();
	TSharedRef<SWidget> GenerateTaskBPBaseClassesMenu() const;

	static FSlateIcon GetNewConditionButtonImage();
	TSharedRef<SWidget> GenerateConditionBPBaseClassesMenu() const;
    
	static FSlateIcon GetNewConsiderationButtonImage();
	TSharedRef<SWidget> GenerateConsiderationBPBaseClassesMenu() const;

	void OnNodeBPBaseClassPicked(UClass* NodeClass) const;
	
	FText GetStatisticsText() const;
	const FPropertyBindingBindingCollection* GetBindingCollection() const;

	void UpdateStateTreeOutliner();

	void HandleTabSpawned(UE::StateTreeEditor::FSpawnedWorkspaceTab SpawnedTab);
	void HandleTabClosed(UE::StateTreeEditor::FSpawnedWorkspaceTab SpawnedTab);

protected:
	TWeakObjectPtr<UStateTreeEditorMode> WeakEditorMode;
	TSharedPtr<IStateTreeEditorHost> EditorHost;

	/** Tree Outliner */
	TSharedPtr<SWidget> StateTreeOutliner = nullptr;
	TWeakPtr<SDockTab> WeakOutlinerTab = nullptr;

#if WITH_STATETREE_TRACE_DEBUGGER
	void UpdateDebuggerView();
	TWeakPtr<SDockTab> WeakDebuggerTab = nullptr;
#endif // WITH_STATETREE_TRACE_DEBUGGER

};



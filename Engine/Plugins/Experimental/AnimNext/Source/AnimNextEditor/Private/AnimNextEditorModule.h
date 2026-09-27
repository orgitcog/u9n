// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAnimNextEditorModule.h"
#include "Module/ModuleEventPropertyCustomization.h"
#include "UObject/TopLevelAssetPath.h"

class UAnimNextWorkspaceSchema;
class FAnimNextGraphPanelNodeFactory;
class UAnimNextWorkspaceEditorMode;
class FAnimNextGraphPanelPinFactory;

namespace UE::AnimNext::Editor
{
	class FParamNamePropertyTypeIdentifier;
	class FLocatorContext;
}

namespace UE::Workspace
{
	class IWorkspaceEditorModule;
}

namespace UE::AnimNext::Editor
{

class FAnimNextEditorModule : public IAnimNextEditorModule
{
	friend UAnimNextWorkspaceSchema;
	friend UAnimNextWorkspaceEditorMode;

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IAnimNextEditorModule interface
	virtual void RegisterLocatorFragmentEditorType(FName InLocatorFragmentEditorName) override;
	virtual void UnregisterLocatorFragmentEditorType(FName InLocatorFragmentEditorName) override;
	virtual void AddWorkspaceSupportedAssetClass(const FTopLevelAssetPath& InClassAssetPath) override;
	virtual void RemoveWorkspaceSupportedAssetClass(const FTopLevelAssetPath& InClassAssetPath) override;
	virtual FDelegateHandle RegisterGraphMenuActionsProvider(const FOnCollectGraphMenuActionsDelegate& InCollectDelegate) override;
	virtual void UnregisterGraphMenuActionsProvider(const FDelegateHandle& InDelegateHandle) override;
	virtual void RegisterAssetCompilationHandler(const FTopLevelAssetPath& InClassPath, FAssetCompilationHandlerFactoryDelegate InAssetCompilationHandlerFactory) override;
	virtual void UnregisterAssetCompilationHandler(const FTopLevelAssetPath& InClassPath) override;
	virtual FDelegateHandle RegisterNodeDblClickHandler(const FNodeDblClickNotificationDelegate& InNodeDblClickNotificationDelegate) override;
	virtual void UnregisterNodeDblClickHandler(const FDelegateHandle& InDelegateHandle) override;

private:
	void CollectGraphMenuActions(const TWeakPtr<UE::Workspace::IWorkspaceEditor>& WorkspaceEditor, FGraphContextMenuBuilder& InContexMenuBuilder, const FActionMenuContextData& InActionMenuContextData);

	void RegisterWorkspaceDocumentTypes(Workspace::IWorkspaceEditorModule& WorkspaceEditorModule);
	void UnregisterWorkspaceDocumentTypes();

	const FAssetCompilationHandlerFactoryDelegate* FindAssetCompilationHandlerFactory(UClass* InAssetClass) const;

private:
	/** Type identifier for parameter names */
	TSharedPtr<FParamNamePropertyTypeIdentifier> Identifier;

	/** Registered names for locator fragments */
	TSet<FName> LocatorFragmentEditorNames;

	TArray<FTopLevelAssetPath> SupportedAssetClasses;

	FCollectGraphMenuActionsMulticast OnCollectGraphMenuActionsDelegateImpl;
	FNodeDblClickNotificationMulticast OnNodeDblClickHandlerMulticast;

	TMap<FTopLevelAssetPath, FAssetCompilationHandlerFactoryDelegate> AssetCompilationHandlerFactories;

	TSharedPtr<FAnimNextGraphPanelPinFactory> GraphPanelPinFactory;
	TSharedPtr<FModuleEventPropertyTypeIdentifier> ModuleEventPropertyTypeIdentifier;

	friend class FLocatorContext;
};

}

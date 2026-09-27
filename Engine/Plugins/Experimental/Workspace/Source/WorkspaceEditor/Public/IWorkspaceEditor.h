// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/BaseAssetToolkit.h"

class UWorkspaceAssetEditor;
class UWorkspaceSchema;
class UWorkspace;
struct FWorkspaceOutlinerItemExport;

namespace UE::Workspace
{

typedef TWeakPtr<SWidget> FGlobalSelectionId;
using FOnClearGlobalSelection = FSimpleDelegate;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFocussedDocumentChanged, TObjectPtr<UObject>);

// RAII helper allowing for a multi-widget selection scope within a WorkspaceEditor instance
struct WORKSPACEEDITOR_API FWorkspaceEditorSelectionScope
{
	FWorkspaceEditorSelectionScope(const TSharedPtr<class IWorkspaceEditor>& InWorkspaceEditor);	
	~FWorkspaceEditorSelectionScope();

	TWeakPtr<class IWorkspaceEditor> WeakWorkspaceEditor; 
};

class IWorkspaceEditor : public FBaseAssetToolkit
{
public:
	IWorkspaceEditor(UAssetEditor* InOwningAssetEditor) : FBaseAssetToolkit(InOwningAssetEditor) {}

	// Open the supplied assets for editing within the workspace editor
	virtual void OpenAssets(TConstArrayView<FAssetData> InAssets) = 0;

	// Open the supplied exports for editing within the workspace editor
	virtual void OpenExports(TConstArrayView<FWorkspaceOutlinerItemExport> InExports) = 0;

	// Open the supplied objects for editing within the workspace editor
	virtual void OpenObjects(TConstArrayView<UObject*> InObjects) = 0;

	// Get the current set of opened (loaded) assets of the specified class
	virtual void GetOpenedAssetsOfClass(TSubclassOf<UObject> InClass, TArray<UObject*>& OutObjects) const = 0;

	// Get the current set of opened (loaded) assets 
	void GetOpenedAssets(TArray<UObject*>& OutObjects) const { GetOpenedAssetsOfClass(UObject::StaticClass(), OutObjects); }

	// Get the current set of opened (loaded) assets
	template<typename AssetClass>
	void GetOpenedAssets(TArray<UObject*>& OutObjects) const { GetOpenedAssetsOfClass(AssetClass::StaticClass(), OutObjects); }

	// Get the current set of assets in this workspace editor 
	virtual void GetAssets(TArray<FAssetData>& OutAssets) const = 0;

	// Close the supplied objects if they are open for editing within the workspace editor
	virtual void CloseObjects(TConstArrayView<UObject*> InObjects) = 0;

	// Show the supplied objects in the workspace editor details panel
	virtual void SetDetailsObjects(const TArray<UObject*>& InObjects) = 0;

	// Refresh the workspace editor details panel
	virtual void RefreshDetails() = 0;

	// Exposes the editor WorkspaceSchema
	virtual UWorkspaceSchema* GetSchema() const = 0;

	// Set the _current_ global selection (last SWidget with selection set) with delegate to clear it selection on next SetGlobalSelection()
	virtual void SetGlobalSelection(FGlobalSelectionId SelectionId, FOnClearGlobalSelection OnClearSelectionDelegate) = 0;

	// Get the currently focussed document. @return nullptr if the class does not match or no document is focussed
	virtual const TObjectPtr<UObject> GetFocussedDocumentOfClass(const TObjectPtr<UClass> InClass) const = 0;

	// Get the currently focussed document. @return nullptr if the class does not match or no document is focussed
	template<typename AssetClass>
	TObjectPtr<AssetClass> GetFocussedDocument() const
	{
		return Cast<AssetClass>(GetFocussedDocumentOfClass(AssetClass::StaticClass()));
	}

	// Get the currently focussed document. @return nullptr if no document is focussed
	TObjectPtr<UObject> GetFocussedDocument() const
	{
		return GetFocussedDocumentOfClass(UObject::StaticClass());
	}

	// Multicast delegate broadcast whenever the document focussed inside of the WorkspaceEditor changes
	virtual FOnFocussedDocumentChanged& OnFocussedDocumentChanged() = 0;

	// Get the current single selection of the outliner.
	// @return true if a single selection is active
	virtual bool GetOutlinerSelection(TArray<FWorkspaceOutlinerItemExport>& OutExports) const = 0;

	// Delegate fired when selection changes in the workspace outliner
	using FOnOutlinerSelectionChanged = TMulticastDelegate<void(TConstArrayView<FWorkspaceOutlinerItemExport> InExports)>;
	virtual FOnOutlinerSelectionChanged& OnOutlinerSelectionChanged() = 0;

	// Retrieves the common DetailsView widget
	virtual TSharedPtr<IDetailsView> GetDetailsView() = 0;

	// Returns the workspace asset
	virtual UObject* GetWorkspaceAsset() const = 0;

	// Returns the name of the package where the workspace is located
	virtual FString GetPackageName() const = 0;
};

}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanParameterMappingTable.h"
#include "MetaHumanCharacterPipeline.h"

#include "PreviewScene.h"
#include "Tools/BaseAssetToolkit.h"

class AActor;
enum class EMetaHumanCharacterAssemblyResult : uint8;
struct FMetaHumanCharacterPaletteItem;
class FPreviewScene;
class IDetailsView;
class UMetaHumanCharacterPaletteAssetEditor;
class UMetaHumanCharacterPaletteItemWrapper;
class SCharacterPartsView;

/**
 * The core class of the Palette editor
 */
class FMetaHumanCharacterPaletteEditorToolkit : public FBaseAssetToolkit
{
public:
	FMetaHumanCharacterPaletteEditorToolkit(UMetaHumanCharacterPaletteAssetEditor* InOwningAssetEditor);

	UMetaHumanCharacterPaletteAssetEditor* GetCharacterEditor();

protected:
	// Begin FBaseAssetToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void PostInitAssetEditor() override;
	// End FBaseAssetToolkit interface

private:
	TSharedRef<SDockTab> SpawnTab_PartsView(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_ItemDetails(const FSpawnTabArgs& Args);
	void OnPartsViewSelectionChanged(TSharedPtr<FMetaHumanCharacterPaletteItem> NewSelectedItem, ESelectInfo::Type SelectInfo);
	void OnPartsViewDoubleClick(TSharedPtr<FMetaHumanCharacterPaletteItem> Item);
	void OnFinishedChangingItemProperties(const FPropertyChangedEvent& Event);
	
	void Build();
	void UpdatePreviewActor();
	void OnMetaHumanCharacterAssembled(EMetaHumanCharacterAssemblyResult Status);

	// The preview scene displayed in the viewport of the asset editor.
	TUniquePtr<FPreviewScene> PreviewScene;

	// The actor spawned in the world of the preview scene
	//
	// This is a weak pointer because the PreviewScene should hold a strong reference to the actor,
	// so the actor won't be deleted while the scene is alive, but we don't want this to become a
	// dangling pointer after the scene is cleaned up.
	TWeakObjectPtr<AActor> PreviewActor;

	TArray<TSharedPtr<FMetaHumanCharacterPaletteItem>> ListItems;

	TSharedPtr<IDetailsView> ItemDetailsView;
	TSharedPtr<SCharacterPartsView> PartsViewWidget;
	TSharedPtr<FMetaHumanCharacterPaletteItem> CurrentlySelectedItem;
	FMetaHumanPaletteItemKey CurrentlySelectedItemKey;

	TStrongObjectPtr<UMetaHumanCharacterPaletteItemWrapper> ItemWrapper;
};

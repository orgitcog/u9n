// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorModeUILayer.h"

#include "MetaHumanCharacterEditorUILayer.generated.h"

/** Interchange layer to manage built in tab locations within the editor's layout. **/
UCLASS()
class UMetaHumanCharacterEditorUISubsystem : public UAssetEditorUISubsystem
{
	GENERATED_BODY()

public:

	//~Begin UAssetEditorUISubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	virtual void Deinitialize() override;
	virtual void RegisterLayoutExtensions(FLayoutExtender& InExtender) override;
	//~End UAssetEditorUISubsystem interface
};

/** Handles the hosting of additional toolkits, such as the mode toolkit, within the MetaHumanCharacterEditor's toolkit. **/
class FMetaHumanCharacterEditorModeUILayer : public FAssetEditorModeUILayer
{
public:
	FMetaHumanCharacterEditorModeUILayer(const IToolkitHost* InToolkitHost);

	//~Begin FAssetEditorModeUILayer interface
	virtual void OnToolkitHostingStarted(const TSharedRef<IToolkit>& InToolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<IToolkit>& InToolkit) override;
	TSharedPtr<FWorkspaceItem> GetModeMenuCategory() const override;
	//~End FAssetEditorModeUILayer interface

	void SetModeMenuCategory(TSharedPtr<FWorkspaceItem> InMenuCategory);

protected:

	// The Menu Category to be used to add new entries for MetaHuman tabs
	TSharedPtr<FWorkspaceItem> MetaHumanCharacterEditorMenuCategory;
};
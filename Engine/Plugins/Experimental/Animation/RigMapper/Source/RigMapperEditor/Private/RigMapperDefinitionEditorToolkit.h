// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "RigMapperDefinition.h"

#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"

enum class ERigMapperNodeType : uint8;
class SRigMapperDefinitionStructureView;
class SRigMapperDefinitionGraphEditor;

/**
 * The toolkit for the URigMapperDefinition asset editor
 */
class RIGMAPPEREDITOR_API FRigMapperDefinitionEditorToolkit : public FAssetEditorToolkit, 
	public FSelfRegisteringEditorUndoClient
{
public:
	void Initialize(URigMapperDefinition* InDefinition, EToolkitMode::Type InMode, TSharedPtr<IToolkitHost> InToolkitHost);

	//~ FAssetEditorToolkit Interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	//~ IToolkit interface

	class FDetailsViewCustomization : public IDetailCustomization
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it */
		static TSharedRef<IDetailCustomization> MakeInstance();

		FDetailsViewCustomization()
		{}

		// IDetailCustomization interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	};
	
private:
	TSharedRef<SDockTab> SpawnGraphTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SDockTab> SpawnStructureTab(const FSpawnTabArgs& SpawnTabArgs);

	bool HandleIsPropertyVisible(const FPropertyAndParent& PropertyAndParent);
	void HandleFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
	void HandleRigMapperDefinitionLoaded();
	void HandleGraphSelectionChanged(const TSet<UObject*>& Nodes);
	
	void HandleStructureSelectionChanged(ESelectInfo::Type SelectInfo, TArray<FString> SelectedInputs, TArray<FString> SelectedFeatures, TArray<FString> SelectedOutputs, TArray<FString> SelectedNullOutputs);

private:
	static const FName DefinitionEditorGraphTabId;
	static const FName DefinitionEditorStructureTabId;
	static const FName DefinitionEditorDetailsTabId;

	static const TMap<FName, ERigMapperNodeType> PropertyNameToNodeTypeMapping;
	
	TObjectPtr<URigMapperDefinition> Definition;

	TSharedPtr<SRigMapperDefinitionGraphEditor> GraphEditor;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SRigMapperDefinitionStructureView> StructureView;
};

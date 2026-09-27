// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/BaseAssetToolkit.h"

enum class EMetaHumanCharacterEnvironment : uint8;
enum class EMetaHumanQualityLevel : uint8;
enum class ERequestTextureResolution : int32;

namespace UE::MetaHuman
{
	enum class ERigType;
}

class FMetaHumanCharacterEditorToolkit : public FBaseAssetToolkit
{
public:

	FMetaHumanCharacterEditorToolkit(class UMetaHumanCharacterAssetEditor* InOwningAssetEditor);
	virtual ~FMetaHumanCharacterEditorToolkit();

	//~Begin FBaseAssetToolkit interface
	virtual FName GetToolkitFName() const;
	virtual FText GetBaseToolkitName() const;
	virtual void CreateEditorModeManager() override;
	virtual void SaveAsset_Execute() override;
	virtual void InitToolMenuContext(struct FToolMenuContext& InMenuContext) override;
	virtual void OnClose() override;
	//~End FBaseAssetToolkit interface

protected:

	//~Begin FBaseAssetToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void PostInitAssetEditor() override;
	virtual void OnToolkitHostingStarted(const TSharedRef<IToolkit>& InToolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<IToolkit>& InToolkit) override;
	//~End FBaseAssetToolkit interface

private:

	/**/
	static const FName MetaHumanCharacterPreviewTabID;
	static const FName MetaHumanCharcterAnimationPanelID;

	/** Utility to get the MetaHumanCharacterEditorMode */
	TNotNull<class UMetaHumanCharacterEditorMode*> GetMetaHumanCharacterEditorMode() const;

	/** Returns false if there is a current active tool */
	bool HasActiveTool() const;

	/** Returns true if the Character can request higher resolution face textures from the service */
	bool CanRequestHighResolutionTextures() const;

	/** Start a service request for face textures */
	void RequestHighResolutionTextures(ERequestTextureResolution InRes);

	/** Returns true if it's safe to call AutoRigFace */
	bool CanAutoRigFace() const;

	/** Function which triggers the call to AutoRigService */
	void AutoRigFace(const UE::MetaHuman::ERigType InRigType);

	/** Returns true if the is a rig to remove */
	bool CanRemoveFaceRig() const;

	/** Function which triggers removal of the face rig */
	void RemoveFaceRig();

	/** Check if editor can create combined skel mesh for the given character */
	bool CanExportCombinedSkelMesh() const;
	
	/** Entry point for creating combined skel mesh */
	void ExportCombinedSkelMesh();

	/** Generic function for loading a level*/
	class ULevelStreaming* LoadLevelInWorld(const FSoftObjectPath& LevelPath);

	/**This function is called on PostInitEditor when we need to load all of the lighting scenarios that we use in viewport*/
	void LoadLightingScenariosInWorld(const TArray<FSoftObjectPath>& LevelPaths);

	/**This function is called on PostInitEditor when we need to load all of the PostProcess scenarios that we use in viewport*/
	void LoadPostProcessScenariosInWorld(const FSoftObjectPath& BaseLevelPath, const FSoftObjectPath& TonemapperLevelPath);
	
	/**Changes lighting environment by streaming lighting scenario in the world*/
	void OnLightingStudioEnvironmentChanged(const EMetaHumanCharacterEnvironment NewStudioEnvironment);

	/** Called when the light rotation changes so the world can be updated */
	void OnLightRotationChanged(float InRotation);

	/** Called when the background color changes */
	void OnBackgroundColorChanged(const FLinearColor& InBackgroundColor);

	/** Called when PostProcess volume option changes*/
	void OnTonemapperEnvironmentChanged(const bool InTonemapperEnabled);

	/** Re-runs the preview build pipeline to regenerate the content used by the preview actor */
	void RefreshPreview();

	/** Extend the editor's toolbar with custom entries */
	void ExtendToolbar();

	/** Extend the editor's main menu with custom entries */
	void ExtendMenu();

	void BindCommands();

private:
	// Functions to assist debugging

	bool CanExportPreviewSkelMeshes() const;	// Returns true if the character can export a skeletal mesh asset of the face or body
	bool CanSaveStates() const;					// Returns true if the character has body or face state
	bool CanSaveFaceDNA() const;				// Returns true if the character has face DNA
	bool CanSaveBodyDNA() const;				// Returns true if the character has body DNA
	bool CanSaveTextures() const;				// Returns true if the character has synthesized textures
	bool CanSaveEyePreset() const;

	void ExportFaceSkelMesh();		// Exports a skeletal mesh of the edited face in the user project
	void ExportBodySkelMesh();		// Exports a skeletal mesh of the edited body in the user project
	void SaveFaceState();			// Save the identity state of the face to a file
	void SaveFaceStateToDNA();		// Save the face state as part of a DNA, uses either the MH asset DNA or the preview one
	void DumpFaceStateDataForAR();	// Save the face state debug data to a folder
	void SaveBodyState();			// Save the identity state of the body to a file
	void SaveFaceDNA();				// Save the DNA for the face to a file
	void SaveBodyDNA();				// Save the DNA for the body to a file
	void SaveFaceTextures();		// Save all synthesized textures of the edited character as images files
	void SaveEyePreset();			// Save the current eye as a preset
	void TakeHighResScreenshot();	// Take a high resolution screenshot of the asset editor

private:

	// The preview scene displayed in the viewport of the asset editor. It holds the world and all the components that operate on the world
	// The advanced preview scene is used because of the post processing settings needed in editor
	TUniquePtr<class FPreviewScene> PreviewScene;

	// The actor spawned in the world of the preview scene. It is used to hold any components required to render the MetaHuman in the preview world
	TScriptInterface<class IMetaHumanCharacterEditorActorInterface> PreviewActor;

	// Handles the hosting of mode toolkits. Builds the UI from the toolkit being hosted
	TSharedPtr<class FMetaHumanCharacterEditorModeUILayer> ModeUILayer;

	// This is set in ModeUILayer to be the menu category where new tabs are registered to be enabled by the user
	TSharedPtr<FWorkspaceItem> MetaHumanCharacterEditorMenuCategory;

	/** Base and Tonemapper post process levels*/
	TArray<class ULevelStreaming*> PostProcessLevels;

	// The preview settings widget where different options for preview can be adjusted and the tab where it's docked
	TSharedPtr<SWidget> PreviewSettingsWidget;
	
	TSharedRef<SDockTab> SpawnTab_AnimationBar(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnTab_PreviewSceneDetails(const FSpawnTabArgs& Args);

	void InitPreviewSceneDetails();
};
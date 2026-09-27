// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedPreviewScene.h"
#include "Dataflow/DataflowDebugDraw.h"
#include "Dataflow/DataflowDebugDrawObject.h"
#include "Elements/Common/TypedElementCommonTypes.h"

#include "DataflowEditorPreviewSceneBase.generated.h"

struct FDataflowBaseElement;
class UDataflowBaseContent;
class UDataflowEditor;
class FAssetEditorModeManager;
class USelection;

/**
 * TEDS tag added to any object that belongs to dataflow construction scene
 */
USTRUCT(meta = (DisplayName = "Dataflow construction object"))
struct FDataflowConstructionObjectTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

/**
 * TEDS tag added to any object that belongs to dataflow simulation scene
 */
USTRUCT(meta = (DisplayName = "Dataflow simulation object"))
struct FDataflowSimulationObjectTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

/**
 * TEDS tag added to any object that belongs to dataflow scene
 */
USTRUCT(meta = (DisplayName = "Dataflow scene object", EditorDataStorage_DynamicColumnTemplate))
struct FDataflowSceneObjectTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()

	struct EditorDataStorage_DynamicColumnTemplate{};
};

/**
 * TEDS tag added to any struct that belongs to dataflow scene
 */
USTRUCT(meta = (DisplayName = "Dataflow scene struct", EditorDataStorage_DynamicColumnTemplate))
struct FDataflowSceneStructTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()

	struct EditorDataStorage_DynamicColumnTemplate{};
};

/**
 * TEDS column added to any struct/scene to display the type (construction/simulation/evaluation)
 */
USTRUCT(meta = (DisplayName = "Dataflow scene type"))
struct FDataflowSceneTypeColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

public:

	int32 SceneType = 0;
};


/** Dataflow focus request delegate */
DECLARE_MULTICAST_DELEGATE_OneParam(FDataflowFocusRequestDelegate, const FBox&)

/**
 * Dataflow preview scene base
 * @brief the scene is holding all the objects that will be
 * visible and potentially editable within the viewport
 */
class DATAFLOWEDITOR_API FDataflowPreviewSceneBase : public FAdvancedPreviewScene
{
public:

	FDataflowPreviewSceneBase(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* Editor, const FName& InActorName);
	virtual ~FDataflowPreviewSceneBase();
	
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** Dataflow editor content accessors */
	TObjectPtr<UDataflowBaseContent>& GetEditorContent();
	const TObjectPtr<UDataflowBaseContent>& GetEditorContent() const;

	/** Dataflow terminal contents accessors */
	TArray<TObjectPtr<UDataflowBaseContent>>& GetTerminalContents();
	const TArray<TObjectPtr<UDataflowBaseContent>>& GetTerminalContents() const;

	/** Root scene actor accessors */
	TObjectPtr<AActor> GetRootActor() { return RootSceneActor; }
	const TObjectPtr<AActor> GetRootActor() const { return RootSceneActor; }

	/** Dataflow mode manager accessors */
	TSharedPtr<FAssetEditorModeManager>& GetDataflowModeManager() { return DataflowModeManager; }
	const TSharedPtr<FAssetEditorModeManager>& GetDataflowModeManager() const { return DataflowModeManager; }
	
	/** Build the scene bounding box */
	virtual FBox GetBoundingBox() const;

	/** Get const scene elements */
	const IDataflowDebugDrawInterface::FDataflowElementsType& GetSceneElements() const {return SceneElements;}

	/** Get non const scene elements */
	IDataflowDebugDrawInterface::FDataflowElementsType& ModifySceneElements() {return SceneElements;}

	/** Get const debug draw component */
	const TObjectPtr<class UDataflowDebugDrawComponent>& GetDebugDrawComponent() const {return DebugDrawComponent;}

	/** Get non const debug draw component */
	TObjectPtr<class UDataflowDebugDrawComponent>& ModifyDebugDrawComponent() {return DebugDrawComponent;}

	/** Tick data flow scene */
	virtual void TickDataflowScene(const float DeltaSeconds) {}

	/** Check if a primitive component is selected */
	bool IsComponentSelected(const UPrimitiveComponent* InComponent) const;

	/** Check if the preview scene can run simulation */
	virtual bool CanRunSimulation() const {return false;}

	/** Update the currently selected scene profile */
	void SetCurrentProfileIndex(int32 NewProfileIndex);

	/** Return true if the preview scene is dirty */
	bool IsSceneDirty() const {return bPreviewSceneDirty;}

	/** Reset the dirty flag */
	void ResetDirtyFlag() {bPreviewSceneDirty = false;}

	/** Event triggered when an object is focused in the scene (double-click in the scene outliner)*/
	FDataflowFocusRequestDelegate& OnFocusRequest() { return FocusRequestDelegate; }

	/** Register all the scene elements to TEDs */
	void RegisterSceneElements(const bool bIsConstruction);

	/** Unregister all the scene elements to TEDs */
	void UnregisterSceneElements();

	/** Return the scene selected components */
	static USelection* GetSelectedComponents(FDataflowPreviewSceneBase* DataflowScene);

	/** Get the dataflow editor */
	const UDataflowEditor* GetDataflowEditor() const { return DataflowEditor; }
	
protected:

	/** Store Scene object into the TEDS database */
	void AddSceneObject(UObject* SceneObject, const bool bIsConstruction) const;

	/** Store Scene struct into the TEDS database */
	void AddSceneStruct(void* SceneStruct, const TWeakObjectPtr<const UScriptStruct> TypeInfo, const bool bIsConstruction) const;

	/** Remove Scene object from the TEDS database */
	void RemoveSceneObject(UObject* SceneObject) const;

	/** Remove Scene struct from the TEDS database */
	void RemoveSceneStruct(void* SceneStruct) const;

	/** Respond to changes in the scene profile settings */
	void OnAssetViewerSettingsRefresh(const FName& InPropertyName);

	/** Root scene actor */
	TObjectPtr<AActor> RootSceneActor = nullptr; 

	/** Dataflow editor linked to that preview scene */
	UDataflowEditor* DataflowEditor = nullptr;

	/** Mode Manager for selection */
	TSharedPtr<FAssetEditorModeManager> DataflowModeManager;

	/** Boolean to check if the preview scene is dirty or not */
	bool bPreviewSceneDirty = false;

	/** Delegate to focus the viewport */
	FDataflowFocusRequestDelegate FocusRequestDelegate;

	/** List of scene elements that could be used in the editor (outliner/viewport...) */
	IDataflowDebugDrawInterface::FDataflowElementsType SceneElements;
	
	/** Persistent component used for debug drawing */
	TObjectPtr<class UDataflowDebugDrawComponent> DebugDrawComponent;
};


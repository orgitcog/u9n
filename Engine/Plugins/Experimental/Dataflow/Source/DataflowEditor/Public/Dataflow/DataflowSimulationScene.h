// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowSimulationGenerator.h"
#include "Chaos/CacheCollection.h"

#include "DataflowSimulationScene.generated.h"

class UDataflowEditor;
class UDeformableTetrahedralComponent;
class USkeletalMeshComponent;
class UGeometryCache;
class UFleshDynamicAsset;
class FFleshCollection;

DECLARE_EVENT(UDataflowSimulationSceneDescription, FDataflowSimulationSceneDescriptionChanged)

UCLASS()
class DATAFLOWEDITOR_API UDataflowSimulationSceneDescription : public UObject
{
public:
	GENERATED_BODY()

	FDataflowSimulationSceneDescriptionChanged DataflowSimulationSceneDescriptionChanged;

	UDataflowSimulationSceneDescription()
	{
		SetFlags(RF_Transactional);
	}

	/** Set the simulation scene */
	void SetSimulationScene(class FDataflowSimulationScene* SimulationScene);

	/** Caching blueprint actor class to spawn */
	UPROPERTY(EditAnywhere, Category = "Scene")
	TSubclassOf<AActor> BlueprintClass = nullptr;

	/** Blueprint actor transform */
	UPROPERTY(EditAnywhere, Category = "Scene")
	FTransform BlueprintTransform = FTransform::Identity;

	/** Caching asset to be used to record the simulation  */
	UPROPERTY(EditAnywhere, Category="Caching", DisplayName="Cache Collection")
	TObjectPtr<UChaosCacheCollection> CacheAsset = nullptr;

	/** Caching params used to record the simulation */
	UPROPERTY(EditAnywhere, Category="Caching")
	FDataflowPreviewCacheParams CacheParams;

	/** Geometry cache asset used to extract skeletal mesh results from simulation */
	UPROPERTY(EditAnywhere, Category = "Geometry Cache", DisplayName="Geometry Cache", meta=(EditCondition = "CacheAsset != nullptr"))
	TObjectPtr<UGeometryCache> GeometryCacheAsset = nullptr;

	/** Skeletal mesh interpolated from simulation. This should match the SkeletalMesh used in GenerateSurfaceBindings node */
	UPROPERTY(EditAnywhere, Category = "Geometry Cache", meta = (EditCondition = "CacheAsset != nullptr && EmbeddedStaticMesh == nullptr"))
	TObjectPtr<USkeletalMesh> EmbeddedSkeletalMesh = nullptr;

	/** Static mesh interpolated from simulation. This should match the Static mesh used in GenerateSurfaceBindings node */
	UPROPERTY(EditAnywhere, Category = "Geometry Cache", meta = (EditCondition = "CacheAsset != nullptr && EmbeddedSkeletalMesh == nullptr"))
	TObjectPtr<UStaticMesh> EmbeddedStaticMesh = nullptr;

	/** Interpolates and saves geometry cache from Chaos cache */
	UFUNCTION(CallInEditor, Category = "Geometry Cache")
	void GenerateGeometryCache();

	/** Creates a new geometry cache file */
	UFUNCTION(CallInEditor, Category = "Geometry Cache")
	void NewGeometryCache();

	/** Visibility of the skeletal mesh */
	UPROPERTY(EditAnywhere, Category = "Skeletal Mesh")
	bool bSkeletalMeshVisibility = true;
private:

	//~ UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	
	/** Simulation scene linked to that descriptor */
	class FDataflowSimulationScene* SimulationScene;

	/** Render geometry positions from interpolation */
	TArray<TArray<FVector3f>> RenderPositions;
};

/**
 * Dataflow simulation scene holding all the dataflow content components
 */
class DATAFLOWEDITOR_API FDataflowSimulationScene : public FDataflowPreviewSceneBase
{
public:

	FDataflowSimulationScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* Editor);
	
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	
	virtual ~FDataflowSimulationScene();

	/** Functions that will be triggered when objects will be reinstanced (BP compilation) */
	void OnObjectsReinstanced(const TMap<UObject*, UObject*>& ObjectsMap);

	/** Tick data flow scene */
	virtual void TickDataflowScene(const float DeltaSeconds) override;
	
	/** Check if the preview scene can run simulation */
	virtual bool CanRunSimulation() const { return true; }

	/** Get the bounding box for the selected components or the entire sim scene */
	virtual FBox GetBoundingBox() const override;

	/** Get the scene description used in the preview scene widget */
	UDataflowSimulationSceneDescription* GetPreviewSceneDescription() const { return SceneDescription; }

	/** Create all the simulation world components and instances */
	void CreateSimulationScene();

	/** Reset all the simulation world components and instances */
	void ResetSimulationScene();

	/** Pause the simulation */
	void PauseSimulationScene() const;

	/** Start the simulation */
	void StartSimulationScene() const;

	/** Step the simulation */
	void StepSimulationScene() const;
	
	/** Gets whether the simulation is running */
	bool IsSimulationEnabled() const;

	/** Rebuild the simulation scene */
	void RebuildSimulationScene(const bool bIsSimulationEnabled);

	/** Check if there is something to render */
	bool HasRenderableGeometry() { return true; }

	/** Update Scene in response to the SceneDescription changing */
	void SceneDescriptionPropertyChanged(const FName& PropertyName);

	/** Record the simulation cache */
	void RecordSimulationCache();

	/** Get the simulation time range */
	const FVector2f& GetTimeRange() const {return TimeRange;}

	/** Get the number of frames */
	const int32& GetNumFrames() const {return NumFrames;}

	/** Get the frame rate */
	const int32 GetFrameRate() const { return SceneDescription->CacheParams.FrameRate; }

	/** Get the subframe rate */
	const int32 GetSubframeRate() const { return SceneDescription->CacheParams.SubframeRate; }

	/** Get delta time */
	float GetDeltaTime() const { return DeltaTime; }

	/** Simulation time used to drive the cache loading */
	float SimulationTime;
	
	/** Preview actor accessors */
	TObjectPtr<AActor> GetPreviewActor() { return PreviewActor; }
	const TObjectPtr<AActor> GetPreviewActor() const { return PreviewActor; }

	/** LOD for preview actor components */
	void SetPreviewLOD(int32 InLOD);
	int32 GetPreviewLOD() const;

private:

	/** Update the simulation cache at some point in time (loading/recording)*/
	void UpdateSimulationCache(float& DeltaSeconds);

	/** Bind the scene selection to the components */
	void BindSceneSelection();

	/** Unbind the scene selection from the components */
	void UnbindSceneSelection();
	
	/** Simulation scene description */
	TObjectPtr<UDataflowSimulationSceneDescription> SceneDescription;

	/** Simulation generator to record the simulation result */
	TSharedPtr<UE::Dataflow::FDataflowSimulationGenerator> SimulationGenerator;

	/** Cache time range in seconds */
	FVector2f TimeRange;

	/** Number of cache frames */
	int32 NumFrames;

	/** Delta time (1/fps) */
	float DeltaTime;

	/** Last context time stamp for which we regenerated the world */
	UE::Dataflow::FTimestamp LastTimeStamp = UE::Dataflow::FTimestamp::Invalid;

	/** Preview actor that will will be used to visualize the result of the simulation graph */
	TObjectPtr<AActor> PreviewActor;

	/** Handle for the delegate */
	FDelegateHandle OnObjectsReinstancedHandle;

	/** Preview LOD used in the simulation viewport */
	int32 CurrentPreviewLOD = INDEX_NONE;

	/** Boolean to check if we are recording the cache or not */
	bool bIsRecordingCache = false;
};





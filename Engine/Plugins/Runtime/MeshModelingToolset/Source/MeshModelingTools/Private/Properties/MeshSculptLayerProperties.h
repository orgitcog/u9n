// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "GeometryBase.h"

#include "MeshSculptLayerProperties.generated.h"

PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);
PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMeshSculptLayers);
class IModelingToolExternalDynamicMeshUpdateAPI;

UCLASS(MinimalAPI)
class UMeshSculptLayerProperties : public UObject
{
	GENERATED_BODY()

public:

	/** Set the active sculpt layer */
	UPROPERTY(EditAnywhere, Category = SculptLayers, meta = (ClampMin = 0, HideEditConditionToggle, EditCondition = bCanEditLayers, ModelingQuickSettings))
	int32 ActiveLayer = 0;

	/** Set the sculpt layer weights */
	UPROPERTY(EditAnywhere, EditFixedSize, NoClear, Category = SculptLayers, meta = (NoResetToDefault, HideEditConditionToggle, EditCondition = bCanEditLayers, ModelingQuickSettings))
	TArray<double> LayerWeights;

	UPROPERTY(meta = (TransientToolProperty))
	bool bCanEditLayers = false;

	UFUNCTION(CallInEditor, Category = SculptLayers)
	void AddLayer();

	UFUNCTION(CallInEditor, Category = SculptLayers)
	void RemoveLayer();

	MESHMODELINGTOOLS_API void Init(IModelingToolExternalDynamicMeshUpdateAPI* InTool, int32 InNumLockedBaseLayers);

#if WITH_EDITOR
	MESHMODELINGTOOLS_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	IModelingToolExternalDynamicMeshUpdateAPI* Tool;

	int32 NumLockedBaseLayers = 0;

	// Mesh before layer change has been applied. Used for tracking mesh changes that occur over multiple frames (i.e., from an interactive drag)
	// TODO: Add/update a mesh FChange to track sculpt layer changes, and use that instead of saving an entire mesh here
	TSharedPtr<FDynamicMesh3> InitialMesh;

	// Helper to set sculpt layers from the current LayerWeights property (accounting for the NumLockedBaseLayers)
	void SetLayerWeights(FDynamicMeshSculptLayers* SculptLayers) const;

	// Update the ActiveLayer and LayerWeights settings from the current sculpt layers
	void UpdateSettingsFromMesh(const FDynamicMeshSculptLayers* SculptLayers);

	// Helper to apply edits to the current sculpt layers if possible, with associated book keeping
	// @param EditFn The edit to apply if possible
	// @param bEmitChange Whether to emit a change object along with the edit
	void EditSculptLayers(TFunctionRef<void(UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::FDynamicMeshSculptLayers* SculptLayers)> EditFn, bool bEmitChange);
};



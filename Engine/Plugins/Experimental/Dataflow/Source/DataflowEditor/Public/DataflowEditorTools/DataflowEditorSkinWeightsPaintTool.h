// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "DataflowEditorTools/DataflowEditorToolBuilder.h"
#include "SkeletalMesh/SkinWeightsPaintTool.h"

#include "DataflowEditorSkinWeightsPaintTool.generated.h"

class UDataflowContextObject;
class UDataflowEditorMode;

/**
 * Dataflow skin weights tool builder
 */
UCLASS()
class DATAFLOWEDITOR_API UDataflowEditorSkinWeightsPaintToolBuilder : public USkinWeightsPaintToolBuilder, public IDataflowEditorToolBuilder
{
	GENERATED_BODY()

private:
	//~ Begin IDataflowEditorToolBuilder interface
	virtual void GetSupportedConstructionViewModes(const UDataflowContextObject& ContextObject, TArray<const UE::Dataflow::IDataflowConstructionViewMode*>& Modes) const override;
	virtual bool CanSetConstructionViewWireframeActive() const override { return false; }
	//~ End IDataflowEditorToolBuilder interface

	//~ Begin USkinWeightsPaintToolBuilder interface
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
	//~ End USkinWeightsPaintToolBuilder interface

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};


/**
 * Dataflow skin weights painting tool
 */
UCLASS()
class DATAFLOWEDITOR_API UDataflowEditorSkinWeightsPaintTool : public USkinWeightsPaintTool
{
	GENERATED_BODY()
	
	//~ Begin USkinWeightsPaintTool interface
	virtual FEditorViewportClient* GetViewportClient() const override {return nullptr;}
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;
	//~ End USkinWeightsPaintTool interface

	friend class UDataflowEditorSkinWeightsPaintToolBuilder;

private:

	/** Set the dataflow context object */
	void SetDataflowEditorContextObject(TObjectPtr<UDataflowContextObject> InDataflowEditorContextObject)
	{
		DataflowEditorContextObject = InDataflowEditorContextObject;
	}

	/** Extract the skin weights from the node */
	bool ExtractSkinWeights(TArray<TArray<int32>>& CurrentIndices, TArray<TArray<float>>& CurrentWeights);
	
	/** Get the skeletal mesh vertex offset in the node collection */
	int32 GetVertexOffset() const;

	/** Get the LOD description given a  LOD index */
	FMeshDescription* GetCurrentDescription(const int32 LODIndex) const;
	
	/** Skin weight node associated with that tool */
	struct FDataflowCollectionEditSkinWeightsNode* SkinWeightNode = nullptr;

	/** Setup weights used to store the initial values */
	TArray<TArray<float>> SetupWeights;

	/** Setup indices used to store the initial values */
	TArray<TArray<int32>> SetupIndices;

	/** Dataflow context object to be used to retrieve the node context data*/
	TObjectPtr<UDataflowContextObject> DataflowEditorContextObject;
};



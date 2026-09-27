// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMorphTargetEditingToolInterface.h"
#include "MeshVertexSculptTool.h"
#include "SkeletalMesh/SkeletalMeshEditionInterface.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "MorphTargetEditingToolProperties.h"
#include "SKMMorphTargetBackedTarget.h"
#include "MeshDescription.h"

#include "MorphTargetVertexSculptTool.generated.h"

/**
 * MorphTarget Vertex Sculpt Tool Builder
 */
UCLASS()
class UMorphTargetVertexSculptToolBuilder : public UMeshVertexSculptToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};


/**
* MorphTarget Editor Tool
*/
UCLASS()
class UMorphTargetVertexSculptTool : 
	public UMeshVertexSculptTool,
	public ISkeletalMeshEditingInterface,
	public IMorphTargetEditingToolInterface
{
	GENERATED_BODY()

public:	
	// UMeshVertexSculptTool overrides
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual void OnEndStroke() override;

	
protected:
	friend class FMorphTargetVertexSculptNonSymmetricChange;
	virtual int32 FindHitSculptMeshTriangle(const FRay3d& LocalRay) override;

	virtual void RegisterBrushes() override;

	void OnToolMeshChanged(UDynamicMeshComponent* Component, const FMeshRegionChangeBase* Change, bool bRevert);

	// IMorphTargetEditingTool overrides
	virtual void SetupCommonProperties(const TFunction<void(UMorphTargetEditingToolProperties*)>& InSetupFunction) override;
	
	// ISkeletalMeshEditingInterface
	virtual void HandleSkeletalMeshModified(const TArray<FName>& Payload, const ESkeletalMeshNotifyType InNotifyType) override;

	void InitializeCache();
	void UpdateCacheIfNeeded();

	void PoseToolMesh();

	UPROPERTY()
	TObjectPtr<class UMorphTargetEditingToolProperties> EditorToolProperties;

	TWeakInterfacePtr<ISkeletalMeshMorphTargetBackedTarget> MorphTargetBackedTarget; 

	TFunction<const FDynamicMesh3*()> GetMeshWithoutCurrentMorphFunc;
	FDynamicMesh3 MeshWithoutCurrentMorph;
	
	bool bCached = false;
	bool bHasValidData = false;

	bool bPoseOpInitialized = false;
	TArray<FTransform> PreviousPoseComponentSpace;
	TMap<FName, float> PreviousMorphWeights;

	TSharedPtr<FDynamicMesh3> MeshBeforePosing;
	bool bPoseChangedLastTick = false;

	FDelegateHandle OnToolMeshChangedDelegate;

	static constexpr int32 LodIndex0 = 0;

	FMeshDescription ToolMeshDescription;
	FName ToolMorphTargetName;

	bool bPosingSculptMesh = false;
};






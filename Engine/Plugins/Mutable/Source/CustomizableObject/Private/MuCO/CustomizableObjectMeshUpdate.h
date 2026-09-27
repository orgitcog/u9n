// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
CustomizableObjectMeshUpdate.h: Helpers to stream in skeletal mesh LODs.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "Streaming/SkeletalMeshUpdate.h"

#include "MuR/System.h"
#include "MuR/Mesh.h"

struct FModelStreamableBulkData;
class UCustomizableObjectSkeletalMesh;

namespace mu 
{ 
	class FParameters;
}

struct FMutableMeshOperationData
{
	TSharedPtr<mu::FSystem> System;
	TSharedPtr<mu::FModel, ESPMode::ThreadSafe> Model;
	TSharedPtr<const mu::FParameters> Parameters;

	TSharedPtr<FModelStreamableBulkData> ModelStreamableBulkData;
	
	int32 State = -1;

	// IDs of the meshes to generate per LOD.
	TArray<mu::FResourceID> MeshIDs;

	// Meshes generated per LOD
	TArray<TSharedPtr<const mu::FMesh>> Meshes;

	/** Serialized data per block. */
	TMap<uint32, FMorphTargetMeshData> MorphTargetMeshData;

	/** Serialized data per block. */
	TMap<uint32, FClothingMeshData> ClothingMeshData;

	UE::Tasks::FTaskEvent Event = UE::Tasks::FTaskEvent(TEXT("Task_Mutable_UpdateMesh_End"));

	/** SkeletalMesh */
	/** The resident first LOD resource index.With domain = [0, ResourceState.NumLODs[.NOT THE ASSET LOD INDEX! */ 
	int32 CurrentFirstLODIdx = INDEX_NONE;

	/** The requested first LOD resource index. With domain = [0, ResourceState.NumLODs[. NOT THE ASSET LOD INDEX! */
	int32 PendingFirstLODIdx = INDEX_NONE;

	int32 AssetLODBias = 0;
};

extern template class TRenderAssetUpdate<FSkelMeshUpdateContext>;

/** 
* This class provides a framework for loading the LODs of CustomizableObject skeletal meshes.
* Each thread essentially calls Tick() until the job is done.
* The object can be safely deleted when IsCompleted() returns true.
*/
class FCustomizableObjectMeshStreamIn : public FSkeletalMeshStreamIn
{
public:
	FCustomizableObjectMeshStreamIn(
		const UCustomizableObjectSkeletalMesh* InMesh,
		EThreadType CreateResourcesThread,
		const TSharedPtr<FModelStreamableBulkData>& ModelStreamableBulkData);

	void OnUpdateMeshFinished();

	// Decrement TaskSynchronization counter and cancel pending low-priority tasks.
	virtual void Abort() override;

private:

	void DoInitiate(const FContext& Context);

	void DoConvertResources(const FContext& Context);

	void DoCreateBuffers(const FContext& Context);

	void DoCancelMeshUpdate(const FContext& Context);

	/** Creates a MeshUpdate task to generate the meshes for the LODs to stream in. */
	void RequestMeshUpdate(const FContext& Context);

	/** Cancels the MeshUpdate task. */
	void CancelMeshUpdate(const FContext& Context);

	/** Converts from mu::FMesh to FSkeletalMeshLODRenderData. */
	void ConvertMesh(const FContext& Context, bool& bOutMarkRenderStateDirty);

	void MarkRenderStateDirty(const FContext& Context);

	/** Context of the MeshUpdate. */
	TSharedPtr<FMutableMeshOperationData> OperationData;

	/** MeshUpdate task Id to cancel the task if the stream in task is aborted. */
	uint32 MutableTaskId = 0;
};

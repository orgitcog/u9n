// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectSkeletalMesh.h"

#include "MuCO/CustomizableObjectSystemPrivate.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectMeshUpdate.h"

#include "Rendering/SkeletalMeshRenderData.h"
#include "Animation/SkinWeightProfile.h"




void UCustomizableObjectSkeletalMesh::InitMutableStreamingData(const TSharedRef<FUpdateContextPrivate>& InOperationData, const FName& ComponentName, const int32 InstanceUpdateFirstLOD, const int32 LODCount)
{
	UCustomizableObject& CustomizableObject = *InOperationData->Object;

	ModelStreamableBulkData = CustomizableObject.GetPrivate()->GetModelStreamableBulkData();
	
	// Debug info
	CustomizableObjectPathName = GetNameSafe(&CustomizableObject);

	// Init properties
	Model = CustomizableObject.GetPrivate()->GetModel();

	Parameters = InOperationData->Parameters;
	State = InOperationData->GetCapturedDescriptor().GetState();

	MeshIDs.Init(MAX_uint64, MAX_MESH_LOD_COUNT);
	SurfaceIDs.SetNum(MAX_MESH_LOD_COUNT);

	const int32 FirstLOD = InOperationData->bStreamMeshLODs ?
		InOperationData->FirstLODAvailable[ComponentName] :
		InOperationData->GetFirstRequestedLOD()[ComponentName];

	for (int32 LODIndex = FirstLOD; LODIndex < LODCount; ++LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = InOperationData->InstanceUpdateData.LODs[InstanceUpdateFirstLOD + LODIndex];
		MeshIDs[LODIndex] = LOD.MeshID;

		SurfaceIDs[LODIndex].SetNum(LOD.SurfaceCount);
		check(LOD.Mesh && LOD.Mesh->GetSurfaceCount() == LOD.SurfaceCount);

		for (int32 SurfaceIndex = 0; SurfaceIndex < LOD.SurfaceCount; ++SurfaceIndex)
		{
			SurfaceIDs[LODIndex][SurfaceIndex] = LOD.Mesh->GetSurfaceId(SurfaceIndex);
		}
	}

	const UModelResources& ModelResources = CustomizableObject.GetPrivate()->GetModelResourcesChecked();

	for (const FSkinWeightProfileInfo& SkinWeightProfile : GetSkinWeightProfiles())
	{
		const FMutableSkinWeightProfileInfo* ProfileInfo = ModelResources.SkinWeightProfilesInfo.FindByPredicate(
			[&SkinWeightProfile](const FMutableSkinWeightProfileInfo& P) { return P.Name == SkinWeightProfile.Name; });

		if (ensure(ProfileInfo))
		{
			SkinWeightProfileIDs.Add({ ProfileInfo->NameId, ProfileInfo->Name });
		}
	}
}


bool UCustomizableObjectSkeletalMesh::StreamIn(int32 NewMipCount, bool bHighPrio)
{
	check(IsInGameThread());

	FSkeletalMeshRenderData* RenderData = GetResourceForRendering();
	if (!RenderData || !RenderData->IsInitialized())
	{
		return false;
	}

	if (!HasPendingInitOrStreaming() && CachedSRRState.StreamIn(NewMipCount))
	{
		FRenderAssetUpdate::EThreadType CreateResourcesThread = GRHISupportsAsyncTextureCreation
			? FRenderAssetUpdate::TT_Async
			: FRenderAssetUpdate::TT_Render;

		PendingUpdate = new FCustomizableObjectMeshStreamIn(this, CreateResourcesThread, ModelStreamableBulkData);

		return !PendingUpdate->IsCancelled();
	}
	return false;
}

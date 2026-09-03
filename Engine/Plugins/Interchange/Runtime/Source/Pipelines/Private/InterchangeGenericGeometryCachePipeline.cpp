// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeGenericMeshPipeline.h"

#include "InterchangeGeometryCacheFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"

void UInterchangeGenericMeshPipeline::ExecutePreImportPipelineGeometryCache()
{
	check(CommonMeshesProperties.IsValid());

	if (bImportGeometryCaches && CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_None)
	{
		// If there's an animated mesh, combine all meshes into a single Geometry Cache
		TArray<FString> MeshInstanceUids;
		PipelineMeshesUtilities->GetAllGeometryCacheInstance(MeshInstanceUids);

		TArray<FString> MeshUids;
		for (const FString& MeshUid : MeshInstanceUids)
		{
			const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);

			// Only look at lod 0 since GeometryCache don't support LODs
			const int32 LodIndex = 0;
			FInterchangeLodSceneNodeContainer EmptySceneNodeContainer;
			const FInterchangeLodSceneNodeContainer& SceneNodeContainer = MeshInstance.SceneNodePerLodIndex.Contains(LodIndex) ?
				MeshInstance.SceneNodePerLodIndex.FindChecked(LodIndex) : EmptySceneNodeContainer;

			for (const UInterchangeSceneNode* SceneNode : SceneNodeContainer.SceneNodes)
			{
				MeshUids.Add(SceneNode->GetUniqueID());
			}
		}

		// If we got some instances, create a geometry cache factory node
		if (MeshUids.Num() > 0)
		{
			UInterchangeGeometryCacheFactoryNode* GeometryCacheFactoryNode = CreateGeometryCacheFactoryNode(MeshUids);
			GeometryCacheFactoryNodes.Add(GeometryCacheFactoryNode);
		}
	}
}

UInterchangeGeometryCacheFactoryNode* UInterchangeGenericMeshPipeline::CreateGeometryCacheFactoryNode(const TArray<FString>& MeshUids)
{
	check(CommonMeshesProperties.IsValid());
	if (MeshUids.Num() == 0)
	{
		return nullptr;
	}

	// Name the geometry cache node according to the first non-root node
	FString GeometryCacheUid;
	FString DisplayLabel;
	{
		// Starting from the first mesh, we go up through its ancestors until we find the first node after the root
		const UInterchangeBaseNode* Node = BaseNodeContainer->GetNode(MeshUids[0]);
		if (const UInterchangeSceneNode* SceneNode = Cast<const UInterchangeSceneNode>(Node))
		{
			const UInterchangeSceneNode* ParentSceneNode = Cast<const UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
			if (!ParentSceneNode || ParentSceneNode->GetParentUid() == UInterchangeBaseNode::InvalidNodeUid())
			{
				// The SceneNode itself or its parent is the root so just get the SceneNode's info
				DisplayLabel = SceneNode->GetDisplayLabel();
				GeometryCacheUid = SceneNode->GetUniqueID();
			}
			else
			{
				while (SceneNode && ParentSceneNode)
				{
					SceneNode = Cast<const UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
					ParentSceneNode = Cast<const UInterchangeSceneNode>(BaseNodeContainer->GetNode(SceneNode->GetParentUid()));
					if (!ParentSceneNode || ParentSceneNode->GetParentUid() == UInterchangeBaseNode::InvalidNodeUid())
					{
						// The root has no parent so retrieve the needed info and break 
						DisplayLabel = SceneNode->GetDisplayLabel();
						GeometryCacheUid = SceneNode->GetUniqueID();
						break;
					}
				}
			}
		}
	}

	const FString StaticMeshUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(GeometryCacheUid);
	UInterchangeGeometryCacheFactoryNode* GeometryCacheFactoryNode = NewObject<UInterchangeGeometryCacheFactoryNode>(BaseNodeContainer, NAME_None);
	if (!ensure(GeometryCacheFactoryNode))
	{
		return nullptr;
	}

	GeometryCacheFactoryNode->InitializeGeometryCacheNode(StaticMeshUid, DisplayLabel, UGeometryCache::StaticClass()->GetName(), BaseNodeContainer);

	// For now, keep all the mesh sections separate since each section will be in its own track in the GeometryCache
	GeometryCacheFactoryNode->SetCustomKeepSectionsSeparate(true);

	AddMeshesToGeometryCache(GeometryCacheFactoryNode, MeshUids);

	// #ueent_todo: Support the common meshes build options (not all are supported)
	GeometryCacheFactoryNode->SetCustomRecomputeNormals(CommonMeshesProperties->bRecomputeNormals);
	GeometryCacheFactoryNode->SetCustomRecomputeTangents(CommonMeshesProperties->bRecomputeTangents);
	GeometryCacheFactoryNode->SetCustomUseMikkTSpace(CommonMeshesProperties->bUseMikkTSpace);
	GeometryCacheFactoryNode->SetCustomComputeWeightedNormals(CommonMeshesProperties->bComputeWeightedNormals);
	GeometryCacheFactoryNode->SetCustomUseHighPrecisionTangentBasis(CommonMeshesProperties->bUseHighPrecisionTangentBasis);
	GeometryCacheFactoryNode->SetCustomUseFullPrecisionUVs(CommonMeshesProperties->bUseFullPrecisionUVs);
	GeometryCacheFactoryNode->SetCustomUseBackwardsCompatibleF16TruncUVs(CommonMeshesProperties->bUseBackwardsCompatibleF16TruncUVs);
	GeometryCacheFactoryNode->SetCustomRemoveDegenerates(CommonMeshesProperties->bRemoveDegenerates);

	GeometryCacheFactoryNode->SetCustomFlattenTracks(bFlattenTracks);
	GeometryCacheFactoryNode->SetCustomPositionPrecision(CompressedPositionPrecision);
	GeometryCacheFactoryNode->SetCustomNumBitsForUVs(CompressedTextureCoordinatesNumberOfBits);

	if (bOverrideTimeRange)
	{
		GeometryCacheFactoryNode->SetCustomStartFrame(FrameStart);
		GeometryCacheFactoryNode->SetCustomEndFrame(FrameEnd);
	}

	GeometryCacheFactoryNode->SetCustomMotionVectorsImport(MotionVectors);
	GeometryCacheFactoryNode->SetCustomApplyConstantTopologyOptimization(bApplyConstantTopologyOptimizations);
	GeometryCacheFactoryNode->SetCustomStoreImportedVertexNumbers(bStoreImportedVertexNumbers);
	GeometryCacheFactoryNode->SetCustomOptimizeIndexBuffers(bOptimizeIndexBuffers);

	return GeometryCacheFactoryNode;
}

void UInterchangeGenericMeshPipeline::AddMeshesToGeometryCache(UInterchangeGeometryCacheFactoryNode* GeometryCacheFactoryNode, const TArray<FString>& NodeUids)
{
	TMap<FString, FString> ExistingLodSlotMaterialDependencies;
	constexpr bool bAddSourceNodeName = true;
	for (const FString& NodeUid : NodeUids)
	{
		TMap<FString, FString> SlotMaterialDependencies;
		if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(NodeUid)))
		{
			FString MeshDependency;
			SceneNode->GetCustomAssetInstanceUid(MeshDependency);
			if (const UInterchangeMeshNode* MeshDependencyNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(MeshDependency)))
			{
				UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshDependencyNode, GeometryCacheFactoryNode, bAddSourceNodeName);

				// Note here we add the SceneNode because we'll bake its transform into the mesh
				GeometryCacheFactoryNode->AddTargetNodeUid(NodeUid);
				MeshDependencyNode->AddTargetNodeUid(GeometryCacheFactoryNode->GetUniqueID());

				MeshDependencyNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
			}
			else
			{
				SceneNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
			}

			UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(SceneNode, GeometryCacheFactoryNode, bAddSourceNodeName);
		}
		else if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(NodeUid)))
		{
			UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshNode, GeometryCacheFactoryNode, bAddSourceNodeName);
			GeometryCacheFactoryNode->AddTargetNodeUid(NodeUid);
			MeshNode->AddTargetNodeUid(GeometryCacheFactoryNode->GetUniqueID());

			MeshNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
		}

		UE::Interchange::MeshesUtilities::ApplySlotMaterialDependencies(*GeometryCacheFactoryNode, SlotMaterialDependencies, *BaseNodeContainer, &ExistingLodSlotMaterialDependencies);
	}
}

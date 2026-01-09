// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeGenericMeshPipeline.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "CoreMinimal.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeStaticMeshLodDataNode.h"
#include "InterchangeSourceData.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "Tasks/Task.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"


namespace UE::Interchange::Private
{
	FString GetNodeName(TObjectPtr<UInterchangePipelineMeshesUtilities> PipelineMeshesUtilities, const UInterchangeBaseNodeContainer& NodeContainer, const FString& NodeUid)
	{
		const UInterchangeBaseNode* BaseNode = NodeContainer.GetNode(NodeUid);
		if (BaseNode)
		{
			FString NodeName = BaseNode->GetDisplayLabel();
			if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNode))
			{
				if (SceneNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetLodGroupSpecializeTypeString()))
				{
					TArray<FString> LodGroupChildrens = NodeContainer.GetNodeChildrenUids(SceneNode->GetUniqueID());
					if (LodGroupChildrens.Num() > 0)
					{
						return GetNodeName(PipelineMeshesUtilities, NodeContainer, LodGroupChildrens[0]);
					}
				}
			}
			else if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(BaseNode))
			{
				//If this mesh is reference by only one scene node that do not have any children, use the scene node display label
				const FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->GetMeshGeometryByUid(NodeUid);
				if (MeshGeometry.ReferencingMeshInstanceUids.Num() == 1
					&& NodeContainer.GetNodeChildrenCount(MeshGeometry.ReferencingMeshInstanceUids[0]) == 0)
				{
					if(const UInterchangeBaseNode* InstanceMeshNode = NodeContainer.GetNode(MeshGeometry.ReferencingMeshInstanceUids[0]))
					{
						return InstanceMeshNode->GetDisplayLabel();
					}
				}
			}

			return NodeName;
		}
		else
		{
			return FString();
		}
	}
}

static TTuple<EInterchangeMeshCollision, FString> GetCollisionMeshType(
	TObjectPtr<UInterchangePipelineMeshesUtilities> PipelineMeshesUtilities,
	const UInterchangeBaseNodeContainer& NodeContainer,
	const FString& NodeUid,
	const TArray<FString>& AllNodeUids,
	bool bImportCollisionAccordingToMeshName
)
{
	EInterchangeMeshCollision CollisionType = EInterchangeMeshCollision::None;

	// Try to find the mesh node we're actually talking about
	const UInterchangeBaseNode* BaseNode = NodeContainer.GetNode(NodeUid);
	const UInterchangeMeshNode* MeshNode = Cast<const UInterchangeMeshNode>(BaseNode);
	if (!MeshNode)
	{
		if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNode))
		{
			FString MeshDependency;
			if (SceneNode->GetCustomAssetInstanceUid(MeshDependency))
			{
				MeshNode = Cast<const UInterchangeMeshNode>(NodeContainer.GetNode(MeshDependency));
			}
		}
	}

	// If we have an explicit collision type on the mesh, we know the mesh node is a collision mesh itself
	if (MeshNode)
	{
		if (MeshNode->GetCustomCollisionType(CollisionType) && CollisionType != EInterchangeMeshCollision::None)
		{
			return {CollisionType, NodeUid};
		}
	}

	if (bImportCollisionAccordingToMeshName)
	{
		FString MeshName = UE::Interchange::Private::GetNodeName(PipelineMeshesUtilities, NodeContainer, NodeUid);

		// Determine if the mesh name is a potential collision mesh

		if (MeshName.StartsWith(TEXT("UBX_")))
		{
			CollisionType = EInterchangeMeshCollision::Box;
		}
		else if (MeshName.StartsWith(TEXT("UCX_")) || MeshName.StartsWith(TEXT("MCDCX_")))
		{
			CollisionType = EInterchangeMeshCollision::Convex18DOP;
		}
		else if (MeshName.StartsWith(TEXT("USP_")))
		{
			CollisionType = EInterchangeMeshCollision::Sphere;
		}
		else if (MeshName.StartsWith(TEXT("UCP_")))
		{
			CollisionType = EInterchangeMeshCollision::Capsule;
		}
		else
		{
			return { EInterchangeMeshCollision::None, FString() };
		}

		// We have a mesh name with a collision type suffix.
		// However it should only be treated as a collision mesh if its body name corresponds to one of the other meshes.
		// If we get here, we know there is at least one underscore, so we never expect either of these character searches to fail.

		int32 FirstUnderscore = INDEX_NONE;
		verify(MeshName.FindChar(TEXT('_'), FirstUnderscore));

		int32 LastUnderscore = INDEX_NONE;
		verify(MeshName.FindLastChar(TEXT('_'), LastUnderscore));

		auto MatchPredicate = [&PipelineMeshesUtilities , &NodeContainer](FString Body)
		{
			// Generate a predicate to be used by the below Finds.
			return [Body, &PipelineMeshesUtilities, &NodeContainer](const FString& ToCompare) { return Body == UE::Interchange::Private::GetNodeName(PipelineMeshesUtilities, NodeContainer, ToCompare); };
		};

		// If we find a mesh named the same as the collision mesh (following the collision prefix), we have a match
		// e.g. this will match 'UBX_House' with a mesh called 'House'
		{
			const FString RenderMeshName = MeshName.RightChop(FirstUnderscore + 1);
			const FString* CorrespondingMeshUid = AllNodeUids.FindByPredicate(MatchPredicate(RenderMeshName));
			if (CorrespondingMeshUid)
			{
				return { CollisionType, *CorrespondingMeshUid };
			}
		}

		// Otherwise strip the final underscore suffix from the collision mesh name and look again
		// e.g. this will match 'UBX_House_01' with a mesh called 'House'
		if (FirstUnderscore != LastUnderscore)
		{
			const FString RenderMeshName = MeshName.Mid(FirstUnderscore + 1, LastUnderscore - FirstUnderscore - 1);
			const FString* CorrespondingMeshUid = AllNodeUids.FindByPredicate(MatchPredicate(RenderMeshName));
			if (CorrespondingMeshUid)
			{
				return { CollisionType, *CorrespondingMeshUid };
			}
		}

		// It is possible our target mesh has been renamed away by a name sanitizing process (see UE-248981 and
		// FFbxParser::EnsureNodeNameAreValid). Since it is unusual to have a prefixed collision mesh and no target, let's 
		// assume we actually have a render mesh somewhere and try stripping any nameclash-looking suffixes from mesh names 
		// to see if we have a match
		{
			// Like above start by just stripping the prefix (e.g. 'UBX_House_01' --> 'House_01')
			FString RenderMeshName = MeshName.RightChop(FirstUnderscore + 1);

			auto NameCollisionSearchPredicate = [&RenderMeshName, &PipelineMeshesUtilities, &NodeContainer](const FString& Other)
			{
				FString MeshName = UE::Interchange::Private::GetNodeName(PipelineMeshesUtilities, NodeContainer, Other);
				if (!MeshName.StartsWith(RenderMeshName))
				{
					return false;
				}

				FString MeshNameSuffix = MeshName.RightChop(RenderMeshName.Len());

				// Check if everything after the render mesh name is just some combination of "ncl" (nameclash), numbers and/or underscores
				MeshNameSuffix = MeshNameSuffix.Replace(TEXT("ncl"), TEXT(""));

				FString LastChar = MeshNameSuffix.Right(1);
				while ((LastChar.IsNumeric() || LastChar == TEXT("_")) && MeshNameSuffix.Len() > 0)
				{
					MeshNameSuffix.LeftChopInline(1, EAllowShrinking::No);
					LastChar = MeshNameSuffix.Right(1);
				}

				// If there's nothing left, everything after the RenderMeshName was nameclash stuff, so this is likely our mesh
				return MeshNameSuffix.Len() == 0;
			};

			const FString* CorrespondingMeshUid = AllNodeUids.FindByPredicate(NameCollisionSearchPredicate);
			if (CorrespondingMeshUid)
			{
				return {CollisionType, *CorrespondingMeshUid};
			}

			// If we still have nothing, try also stripping numbered suffixes from the render mesh name and repeating the search
			// (e.g. 'UBX_House_01' --> 'House')
			RenderMeshName = MeshName.Mid(FirstUnderscore + 1, LastUnderscore - FirstUnderscore - 1);
			CorrespondingMeshUid = AllNodeUids.FindByPredicate(NameCollisionSearchPredicate);
			if (CorrespondingMeshUid)
			{
				return {CollisionType, *CorrespondingMeshUid};
			}
		}
	}

	return { EInterchangeMeshCollision::None, FString() };
}

// Returns true if MeshUid describes a Mesh node that is purely the collision mesh of some other mesh
static bool IsJustCollisionMesh(TObjectPtr<UInterchangePipelineMeshesUtilities> PipelineMeshesUtilities, const UInterchangeBaseNodeContainer& NodeContainer, const FString& MeshUid, const TArray<FString>& MeshUids, bool bImportCollisionAccordingToMeshName)
{
	TTuple<EInterchangeMeshCollision, FString> Tuple = GetCollisionMeshType(PipelineMeshesUtilities, NodeContainer, MeshUid, MeshUids, bImportCollisionAccordingToMeshName);

	// If MeshUid is a collision mesh but *of itself*, then it is both a collision and a render mesh, and we should return false
	return Tuple.Key != EInterchangeMeshCollision::None && Tuple.Value != MeshUid;
}


static void BuildMeshToCollisionMeshMap(TObjectPtr<UInterchangePipelineMeshesUtilities> PipelineMeshesUtilities, const UInterchangeBaseNodeContainer& NodeContainer, const TArray<FString>& MeshUids, TMap<FString, TArray<FString>>& MeshToCollisionMeshMap, bool bImportCollisionAccordingToMeshName)
{
	for (const FString& MeshUid : MeshUids)
	{
		TTuple<EInterchangeMeshCollision, FString> CollisionType = GetCollisionMeshType(PipelineMeshesUtilities, NodeContainer, MeshUid, MeshUids, bImportCollisionAccordingToMeshName);
		if (CollisionType.Get<0>() != EInterchangeMeshCollision::None)
		{
			MeshToCollisionMeshMap.FindOrAdd(FString(CollisionType.Get<1>())).Emplace(MeshUid);
		}
	}
}


void UInterchangeGenericMeshPipeline::ExecutePreImportPipelineStaticMesh()
{
	check(CommonMeshesProperties.IsValid());

#if WITH_EDITOR
	//Make sure the generic pipeline will cover all staticmesh build settings when we import
	Async(EAsyncExecution::TaskGraphMainThread, []()
		{
			static bool bVerifyBuildProperties = false;
			if (!bVerifyBuildProperties)
			{
				bVerifyBuildProperties = true;
				TArray<const UClass*> Classes;
				Classes.Add(UInterchangeGenericCommonMeshesProperties::StaticClass());
				Classes.Add(UInterchangeGenericMeshPipeline::StaticClass());
				if (!DoClassesIncludeAllEditableStructProperties(Classes, FMeshBuildSettings::StaticStruct()))
				{
					UE_LOG(LogInterchangePipeline, Log, TEXT("UInterchangeGenericMeshPipeline: The generic pipeline does not cover all static mesh build options."));
				}
			}
		});
#endif

	if (bImportStaticMeshes && (CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_None || CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_StaticMesh))
	{
		if (bCombineStaticMeshes)
		{
			// Combine all the static meshes

			bool bFoundMeshes = false;
			{
				// If baking transforms, get all the static mesh instance nodes, and group them by LOD
				TArray<FString> MeshUids;
				PipelineMeshesUtilities->GetAllStaticMeshInstance(MeshUids);

				TMap<int32, TArray<FString>> MeshUidsPerLodIndex;

				for (const FString& MeshUid : MeshUids)
				{
					const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);
					for (const auto& LodIndexAndSceneNodeContainer : MeshInstance.SceneNodePerLodIndex)
					{
						const int32 LodIndex = LodIndexAndSceneNodeContainer.Key;
						const FInterchangeLodSceneNodeContainer& SceneNodeContainer = LodIndexAndSceneNodeContainer.Value;

						TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
						for (const UInterchangeSceneNode* SceneNode : SceneNodeContainer.SceneNodes)
						{
							TranslatedNodes.Add(SceneNode->GetUniqueID());
						}
					}
				}

				//We must add all nodes that are not part of any lod to all lods upper then 0
				if (MeshUidsPerLodIndex.Num() > 1)
				{
					constexpr int32 LodIndexZero = 0;
					for (const FString& MeshUid : MeshUids)
					{
						const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);
						if (MeshInstance.LodGroupNode == nullptr
							&& MeshInstance.SceneNodePerLodIndex.Num() == 1
							&& MeshInstance.SceneNodePerLodIndex.Contains(LodIndexZero)
							&& MeshInstance.SceneNodePerLodIndex[LodIndexZero].SceneNodes.Num() == 1)
						{
							const UInterchangeSceneNode* SceneNode = MeshInstance.SceneNodePerLodIndex[LodIndexZero].SceneNodes[0];
							//This mesh must be added to all existing LODs over 0
							for (TPair<int32, TArray<FString>>& MeshUidsPerLodIndexPair : MeshUidsPerLodIndex)
							{
								if (MeshUidsPerLodIndexPair.Key > LodIndexZero)
								{
									TArray<FString>& TranslatedNodes = MeshUidsPerLodIndexPair.Value;
									TranslatedNodes.AddUnique(SceneNode->GetUniqueID());
								}
							}
						}
					}
				}

				// If we got some instances, create a static mesh factory node
				if (MeshUidsPerLodIndex.Num() > 0)
				{
					UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = CreateStaticMeshFactoryNode(MeshUidsPerLodIndex);
					StaticMeshFactoryNodes.Add(StaticMeshFactoryNode);
					bFoundMeshes = true;
				}
			}

			if (!bFoundMeshes)
			{
				// If we haven't yet managed to build a factory node, look at static mesh geometry directly.
				TArray<FString> MeshUids;
				PipelineMeshesUtilities->GetAllStaticMeshGeometry(MeshUids);

				TMap<int32, TArray<FString>> MeshUidsPerLodIndex;

				for (const FString& MeshUid : MeshUids)
				{
					// MeshGeometry cannot have Lod since LODs are defined in the scene node
					const FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->GetMeshGeometryByUid(MeshUid);
					const int32 LodIndex = 0;
					TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
					TranslatedNodes.Add(MeshGeometry.MeshUid);
				}

				if (MeshUidsPerLodIndex.Num() > 0)
				{
					UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = CreateStaticMeshFactoryNode(MeshUidsPerLodIndex);
					StaticMeshFactoryNodes.Add(StaticMeshFactoryNode);
				}
			}

		}
		else
		{
			// Do not combine static meshes

			auto CreateMeshFactoryNodeUnCombined = [this](const TArray<FString>& MeshUids, const bool bInstancedMesh)->bool
				{
					constexpr int32 LodIndexZero = 0;
					bool bFoundMeshes = false;

					// Work out which meshes are collision meshes which correspond to another mesh
					TMap<FString, TArray<FString>> MeshToCollisionMeshMap;
					BuildMeshToCollisionMeshMap(PipelineMeshesUtilities, *BaseNodeContainer, MeshUids, MeshToCollisionMeshMap, bImportCollisionAccordingToMeshName);

					// Now iterate through each mesh UID, creating a new factory for each one
					for (const FString& MeshUid : MeshUids)
					{
						// If this is just a collision mesh, don't add a factory node; it will be added as part of another factory node
						if (IsJustCollisionMesh(PipelineMeshesUtilities, *BaseNodeContainer, MeshUid, MeshUids, bImportCollisionAccordingToMeshName))
						{
							continue;
						}

						TMap<int32, TArray<FString>> MeshUidsPerLodIndex;
						if (bInstancedMesh)
						{
							//Instanced geometry can have lods
							const FInterchangeMeshInstance& MeshInstance = PipelineMeshesUtilities->GetMeshInstanceByUid(MeshUid);
							for (const auto& LodIndexAndSceneNodeContainer : MeshInstance.SceneNodePerLodIndex)
							{
								const int32 LodIndex = LodIndexAndSceneNodeContainer.Key;
								const FInterchangeLodSceneNodeContainer& SceneNodeContainer = LodIndexAndSceneNodeContainer.Value;

								TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndex);
								for (const UInterchangeSceneNode* SceneNode : SceneNodeContainer.SceneNodes)
								{
									TranslatedNodes.Add(SceneNode->GetUniqueID());
								}
							}
						}
						else
						{
							//Non instanced geometry cannot have lods
							const FInterchangeMeshGeometry& MeshGeometry = PipelineMeshesUtilities->GetMeshGeometryByUid(MeshUid);
							TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndexZero);
							TranslatedNodes.Add(MeshGeometry.MeshUid);
						}

						if (MeshUidsPerLodIndex.Num() > 0)
						{
							if (bCollision)
							{
								if (const TArray<FString>* CorrespondingCollisionMeshes = MeshToCollisionMeshMap.Find(MeshUid))
								{
									TArray<FString>& TranslatedNodes = MeshUidsPerLodIndex.FindOrAdd(LodIndexZero);
									for (const FString& CollisionMesh : *CorrespondingCollisionMeshes)
									{
										// AddUnique here because now we support meshes being both collision AND render meshes at the
										// same time we may already have these entries in MeshUidsPerLodIndex
										TranslatedNodes.AddUnique(CollisionMesh);
									}
								}
							}

							UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = CreateStaticMeshFactoryNode(MeshUidsPerLodIndex);
							StaticMeshFactoryNodes.Add(StaticMeshFactoryNode);
							bFoundMeshes = true;
						}
					}
					return bFoundMeshes;
				};

			TArray<FString> MeshUids;
			PipelineMeshesUtilities->GetAllStaticMeshInstance(MeshUids);
			if (!CreateMeshFactoryNodeUnCombined(MeshUids, true/*bInstancedMesh*/))
			{
				MeshUids.Reset();
				PipelineMeshesUtilities->GetAllStaticMeshGeometry(MeshUids);
				CreateMeshFactoryNodeUnCombined(MeshUids, false/*bInstancedMesh*/);
			}
		}
	}
}

bool UInterchangeGenericMeshPipeline::MakeMeshFactoryNodeUidAndDisplayLabel(const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex, int32 LodIndex, FString& NewNodeUid, FString& DisplayLabel)
{
	int32 SceneNodeCount = 0;

	if (!ensure(LodIndex >= 0 && MeshUidsPerLodIndex.Num() > LodIndex))
	{
		return false;
	}

	for (const TPair<int32, TArray<FString>>& LodIndexAndMeshUids : MeshUidsPerLodIndex)
	{
		if (LodIndex == LodIndexAndMeshUids.Key)
		{
			const TArray<FString>& Uids = LodIndexAndMeshUids.Value;
			if (Uids.Num() > 0)
			{
				const FString& Uid = Uids[0];
				const UInterchangeBaseNode* Node = BaseNodeContainer->GetNode(Uid);

				if (const UInterchangeMeshNode* MeshNode = Cast<const UInterchangeMeshNode>(Node))
				{
					DisplayLabel = Node->GetDisplayLabel();
					NewNodeUid = Uid;
					return true;
				}

				if (const UInterchangeSceneNode* SceneNode = Cast<const UInterchangeSceneNode>(Node))
				{
					FString RefMeshUid;
					if (SceneNode->GetCustomAssetInstanceUid(RefMeshUid))
					{
						const UInterchangeBaseNode* MeshNode = BaseNodeContainer->GetNode(RefMeshUid);
						if (MeshNode)
						{
							DisplayLabel = MeshNode->GetDisplayLabel();
							if (Uids.Num() == 1)
							{
								// If we are instancing one scene node, we want to use it to name the mesh
								DisplayLabel = SceneNode->GetDisplayLabel();
							}

							NewNodeUid = RefMeshUid;
							return true;
						}
					}
				}
			}

			// We found the lod but there is no valid Mesh node to return the Uid
			break;
		}
	}

	return false;
}

UInterchangeStaticMeshFactoryNode* UInterchangeGenericMeshPipeline::CreateStaticMeshFactoryNode(const TMap<int32, TArray<FString>>& MeshUidsPerLodIndex)
{
	check(CommonMeshesProperties.IsValid());
	if (MeshUidsPerLodIndex.Num() == 0)
	{
		return nullptr;
	}

	// Create the static mesh factory node, name it according to the first mesh node compositing the meshes
	FString StaticMeshUid_MeshNamePart;
	FString DisplayLabel;
	const int32 BaseLodIndex = 0;
	if (!MakeMeshFactoryNodeUidAndDisplayLabel(MeshUidsPerLodIndex, BaseLodIndex, StaticMeshUid_MeshNamePart, DisplayLabel))
	{
		// Log an error
		return nullptr;
	}

	const FString StaticMeshUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(StaticMeshUid_MeshNamePart);
	UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode = NewObject<UInterchangeStaticMeshFactoryNode>(BaseNodeContainer, NAME_None);
	if (!ensure(StaticMeshFactoryNode))
	{
		return nullptr;
	}

	StaticMeshFactoryNode->InitializeStaticMeshNode(StaticMeshUid, DisplayLabel, UStaticMesh::StaticClass()->GetName(), BaseNodeContainer);

	//Set the pipeline import sockets property
	StaticMeshFactoryNode->SetCustomImportSockets(CommonMeshesProperties->bImportSockets);

	if (CommonMeshesProperties->bKeepSectionsSeparate)
	{
		StaticMeshFactoryNode->SetCustomKeepSectionsSeparate(CommonMeshesProperties->bKeepSectionsSeparate);
	}

	AddLodDataToStaticMesh(StaticMeshFactoryNode, MeshUidsPerLodIndex);

	switch (CommonMeshesProperties->VertexColorImportOption)
	{
		case EInterchangeVertexColorImportOption::IVCIO_Replace:
		{
			StaticMeshFactoryNode->SetCustomVertexColorReplace(true);
		}
		break;
		case EInterchangeVertexColorImportOption::IVCIO_Ignore:
		{
			StaticMeshFactoryNode->SetCustomVertexColorIgnore(true);
		}
		break;
		case EInterchangeVertexColorImportOption::IVCIO_Override:
		{
			StaticMeshFactoryNode->SetCustomVertexColorOverride(CommonMeshesProperties->VertexOverrideColor);
		}
		break;
	}

	StaticMeshFactoryNode->SetCustomLODGroup(LodGroup);

	//Common meshes build options
	StaticMeshFactoryNode->SetCustomRecomputeNormals(CommonMeshesProperties->bRecomputeNormals);
	StaticMeshFactoryNode->SetCustomRecomputeTangents(CommonMeshesProperties->bRecomputeTangents);
	StaticMeshFactoryNode->SetCustomUseMikkTSpace(CommonMeshesProperties->bUseMikkTSpace);
	StaticMeshFactoryNode->SetCustomComputeWeightedNormals(CommonMeshesProperties->bComputeWeightedNormals);
	StaticMeshFactoryNode->SetCustomUseHighPrecisionTangentBasis(CommonMeshesProperties->bUseHighPrecisionTangentBasis);
	StaticMeshFactoryNode->SetCustomUseFullPrecisionUVs(CommonMeshesProperties->bUseFullPrecisionUVs);
	StaticMeshFactoryNode->SetCustomUseBackwardsCompatibleF16TruncUVs(CommonMeshesProperties->bUseBackwardsCompatibleF16TruncUVs);
	StaticMeshFactoryNode->SetCustomRemoveDegenerates(CommonMeshesProperties->bRemoveDegenerates);

	//Static meshes build options
	StaticMeshFactoryNode->SetCustomBuildReversedIndexBuffer(bBuildReversedIndexBuffer);
	StaticMeshFactoryNode->SetCustomGenerateLightmapUVs(bGenerateLightmapUVs);
	StaticMeshFactoryNode->SetCustomGenerateDistanceFieldAsIfTwoSided(bGenerateDistanceFieldAsIfTwoSided);
	StaticMeshFactoryNode->SetCustomSupportFaceRemap(bSupportFaceRemap);
	StaticMeshFactoryNode->SetCustomMinLightmapResolution(MinLightmapResolution);
	StaticMeshFactoryNode->SetCustomSrcLightmapIndex(SrcLightmapIndex);
	StaticMeshFactoryNode->SetCustomDstLightmapIndex(DstLightmapIndex);
	StaticMeshFactoryNode->SetCustomBuildScale3D(BuildScale3D);
	StaticMeshFactoryNode->SetCustomDistanceFieldResolutionScale(DistanceFieldResolutionScale);
	StaticMeshFactoryNode->SetCustomDistanceFieldReplacementMesh(DistanceFieldReplacementMesh.Get());
	StaticMeshFactoryNode->SetCustomMaxLumenMeshCards(MaxLumenMeshCards);
	StaticMeshFactoryNode->SetCustomBuildNanite(bBuildNanite);
	StaticMeshFactoryNode->SetCustomAutoComputeLODScreenSizes(bAutoComputeLODScreenSizes);
	StaticMeshFactoryNode->SetLODScreenSizes(LODScreenSizes);

	return StaticMeshFactoryNode;
}


UInterchangeStaticMeshLodDataNode* UInterchangeGenericMeshPipeline::CreateStaticMeshLodDataNode(const FString& NodeName, const FString& NodeUniqueID)
{
	FString DisplayLabel(NodeName);
	FString NodeUID(NodeUniqueID);
	UInterchangeStaticMeshLodDataNode* StaticMeshLodDataNode = NewObject<UInterchangeStaticMeshLodDataNode>(BaseNodeContainer, NAME_None);
	if (!ensure(StaticMeshLodDataNode))
	{
		// @TODO: log error
		return nullptr;
	}

	BaseNodeContainer->SetupNode(StaticMeshLodDataNode, NodeUID, DisplayLabel, EInterchangeNodeContainerType::FactoryData);

	StaticMeshLodDataNode->SetOneConvexHullPerUCX(bOneConvexHullPerUCX);
	StaticMeshLodDataNode->SetImportCollision(bCollision);
	StaticMeshLodDataNode->SetImportCollisionType(Collision);
	StaticMeshLodDataNode->SetForceCollisionPrimitiveGeneration(bForceCollisionPrimitiveGeneration);

	return StaticMeshLodDataNode;
}


void UInterchangeGenericMeshPipeline::AddLodDataToStaticMesh(UInterchangeStaticMeshFactoryNode* StaticMeshFactoryNode, const TMap<int32, TArray<FString>>& NodeUidsPerLodIndex)
{
	check(CommonMeshesProperties.IsValid());
	const FString StaticMeshFactoryUid = StaticMeshFactoryNode->GetUniqueID();

	int32 MaxLodIndex = 0;
	for (const TPair<int32, TArray<FString>>& LodIndexAndNodeUids : NodeUidsPerLodIndex)
	{
		MaxLodIndex = FMath::Max(MaxLodIndex, LodIndexAndNodeUids.Key);
	}

	TArray<FString> EmptyLodData;
	const int32 LodCount = MaxLodIndex+1;
	for(int32 LodIndex = 0; LodIndex < LodCount; ++LodIndex)
	{
		if (!CommonMeshesProperties->bImportLods && LodIndex > 0)
		{
			// If the pipeline should not import lods, skip any lod over base lod
			continue;
		}

		const TArray<FString>& NodeUids = NodeUidsPerLodIndex.Contains(LodIndex) ? NodeUidsPerLodIndex.FindChecked(LodIndex) : EmptyLodData;
		// Create a lod data node with all the meshes for this LOD
		const FString StaticMeshLodDataName = TEXT("LodData") + FString::FromInt(LodIndex);
		const FString LODDataPrefix = TEXT("\\LodData") + (LodIndex > 0 ? FString::FromInt(LodIndex) : TEXT(""));
		const FString StaticMeshLodDataUniqueID = LODDataPrefix + StaticMeshFactoryUid;

		// Create LodData node if it doesn't already exist
		UInterchangeStaticMeshLodDataNode* LodDataNode = Cast<UInterchangeStaticMeshLodDataNode>(BaseNodeContainer->GetFactoryNode(StaticMeshLodDataUniqueID));
		if (!LodDataNode)
		{
			// Add the data for the LOD (all the mesh node fbx path, so we can find them when we will create the payload data)
			LodDataNode = CreateStaticMeshLodDataNode(StaticMeshLodDataName, StaticMeshLodDataUniqueID);
			BaseNodeContainer->SetNodeParentUid(StaticMeshLodDataUniqueID, StaticMeshFactoryUid);
			StaticMeshFactoryNode->AddLodDataUniqueId(StaticMeshLodDataUniqueID);
		}
		TMap<FString, FString> ExistingLodSlotMaterialDependencies;
		constexpr bool bAddSourceNodeName = true;
		for (const FString& NodeUid : NodeUids)
		{
			TMap<FString, FString> SlotMaterialDependencies;
			if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(NodeUid)))
			{
				FString MeshDependency;
				SceneNode->GetCustomAssetInstanceUid(MeshDependency);
				if (BaseNodeContainer->IsNodeUidValid(MeshDependency))
				{
					const UInterchangeMeshNode* MeshDependencyNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(MeshDependency));
					UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshDependencyNode, StaticMeshFactoryNode, bAddSourceNodeName);
					StaticMeshFactoryNode->AddTargetNodeUid(MeshDependency);
					StaticMeshFactoryNode->AddSocketUids(PipelineMeshesUtilities->GetMeshGeometryByUid(MeshDependency).AttachedSocketUids);
					MeshDependencyNode->AddTargetNodeUid(StaticMeshFactoryNode->GetUniqueID());

					MeshDependencyNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
				}
				else
				{
					SceneNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
				}

				UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(SceneNode, StaticMeshFactoryNode, bAddSourceNodeName);
			}
			else if (const UInterchangeMeshNode* MeshNode = Cast<UInterchangeMeshNode>(BaseNodeContainer->GetNode(NodeUid)))
			{
				UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(MeshNode, StaticMeshFactoryNode, bAddSourceNodeName);
				StaticMeshFactoryNode->AddTargetNodeUid(NodeUid);
				StaticMeshFactoryNode->AddSocketUids(PipelineMeshesUtilities->GetMeshGeometryByUid(NodeUid).AttachedSocketUids);
				MeshNode->AddTargetNodeUid(StaticMeshFactoryNode->GetUniqueID());

				MeshNode->GetSlotMaterialDependencies(SlotMaterialDependencies);
			}

			UE::Interchange::MeshesUtilities::ApplySlotMaterialDependencies(*StaticMeshFactoryNode, SlotMaterialDependencies, *BaseNodeContainer, &ExistingLodSlotMaterialDependencies);

			if (bImportCollisionAccordingToMeshName)
			{
				TTuple<EInterchangeMeshCollision, FString> MeshType = GetCollisionMeshType(PipelineMeshesUtilities, *BaseNodeContainer, NodeUid, NodeUids, bImportCollisionAccordingToMeshName);
				EInterchangeMeshCollision CollisionType = MeshType.Key;
				const FString& RenderMeshUid = MeshType.Value;
				const FString& ColliderMeshUid = NodeUid;

				switch (CollisionType)
				{
				case EInterchangeMeshCollision::None:
					LodDataNode->AddMeshUid(NodeUid);
					break;

				case EInterchangeMeshCollision::Box:
					LodDataNode->AddBoxCollisionMeshUids(ColliderMeshUid, RenderMeshUid);
					break;

				case EInterchangeMeshCollision::Sphere:
					LodDataNode->AddSphereCollisionMeshUids(ColliderMeshUid, RenderMeshUid);
					break;

				case EInterchangeMeshCollision::Capsule:
					LodDataNode->AddCapsuleCollisionMeshUids(ColliderMeshUid, RenderMeshUid);
					break;

				case EInterchangeMeshCollision::Convex10DOP_X:
				case EInterchangeMeshCollision::Convex10DOP_Y:
				case EInterchangeMeshCollision::Convex10DOP_Z:
				case EInterchangeMeshCollision::Convex18DOP:
				case EInterchangeMeshCollision::Convex26DOP:
					LodDataNode->AddConvexCollisionMeshUids(ColliderMeshUid, RenderMeshUid);
					break;
				}

				// If the Mesh is both render AND collision mesh let's also add it as a render mesh,
				// as the above switch will have only added the collision mesh to the LodDataNode
				if (RenderMeshUid == NodeUid && CollisionType != EInterchangeMeshCollision::None)
				{
					LodDataNode->AddMeshUid(NodeUid);
				}
			}
			else
			{
				LodDataNode->AddMeshUid(NodeUid);
			}
		}
		UE::Interchange::MeshesUtilities::ReorderSlotMaterialDependencies(*StaticMeshFactoryNode, *BaseNodeContainer);
	}
}

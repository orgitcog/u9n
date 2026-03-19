// Copyright Epic Games, Inc. All Rights Reserved. 

#include "MetaHumanInterchangeDnaTranslator.h"
#include "MetaHumanDNAImportColorMap.h"

#include "DNACommon.h"
#include "DNAUtils.h"
#include "StaticMeshOperations.h"
#include "SkeletalMeshAttributes.h"
#include "BoneWeights.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"

#include "InterchangeManager.h"
#include "InterchangeMeshDefinitions.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"

DEFINE_LOG_CATEGORY(InterchangeDNATranslator);
#define LOCTEXT_NAMESPACE "InterchangeDNATranslator"

namespace UE::MetaHuman
{
	static const TArray<FString> DNAMissingJoints = { "root", "pelvis", "spine_01", "spine_02", "spine_03" };
	static const TMap<FString, FString> MaterialSlotsMapping = {
		/* LOD0 meshes */
		{"head_lod0_mesh", "head_shader_shader"},
		{"teeth_lod0_mesh", "teeth_shader_shader"},
		{"saliva_lod0_mesh", "saliva_shader_shader"},
		{"eyeLeft_lod0_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod0_mesh", "eyeRight_shader_shader"},
		{"eyeshell_lod0_mesh", "eyeshell_shader_shader"},
		{"eyelashes_lod0_mesh", "eyelashes_shader_shader"},
		{"eyeEdge_lod0_mesh", "eyeEdge_shader_shader"},
		{"cartilage_lod0_mesh", "cartilage_shader_shader"},
		/* LOD1 meshes */
		{"head_lod1_mesh", "head_LOD1_shader_shader"},
		{"teeth_lod1_mesh", "teeth_shader_shader"},
		{"saliva_lod1_mesh", "saliva_shader_shader"},
		{"eyeLeft_lod1_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod1_mesh", "eyeRight_shader_shader"},
		{"eyeshell_lod1_mesh", "eyeshell_shader_shader"},
		{"eyelashes_lod1_mesh", "eyelashes_HiLOD_shader_shader"},
		{"eyeEdge_lod1_mesh", "eyeEdge_shader_shader"},
		{"cartilage_lod1_mesh", "cartilage_shader_shader"},
		/* LOD2 meshes */
		{"head_lod2_mesh", "head_LOD2_shader_shader"},
		{"teeth_lod2_mesh", "teeth_shader_shader"},
		{"saliva_lod2_mesh", "saliva_shader_shader"},
		{"eyeLeft_lod2_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod2_mesh", "eyeRight_shader_shader"},
		{"eyeshell_lod2_mesh", "eyeshell_shader_shader"},
		{"eyelashes_lod2_mesh", "eyelashes_HiLOD_shader_shader"},
		{"eyeEdge_lod2_mesh", "eyeEdge_shader_shader"},
		/* LOD3 meshes */
		{"head_lod3_mesh", "head_LOD3_shader_shader"},
		{"teeth_lod3_mesh", "teeth_shader_shader"},
		{"eyeLeft_lod3_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod3_mesh", "eyeRight_shader_shader"},
		{"eyeshell_lod3_mesh", "eyeshell_shader_shader"},
		{"eyelashes_lod3_mesh", "eyelashes_HiLOD_shader_shader"},
		{"eyeEdge_lod3_mesh", "eyeEdge_shader_shader"},
		/* LOD4 meshes */
		{"head_lod4_mesh", "head_LOD4_shader_shader"},
		{"teeth_lod4_mesh", "teeth_shader_shader"},
		{"eyeLeft_lod4_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod4_mesh", "eyeRight_shader_shader"},
		{"eyeshell_lod4_mesh", "eyeshell_shader_shader"},
		/* LOD5 meshes */
		{"head_lod5_mesh", "head_LOD57_shader_shader"},
		{"teeth_lod5_mesh", "teeth_shader_shader"},
		{"eyeLeft_lod5_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod5_mesh", "eyeRight_shader_shader"},
		/* LOD6 meshes */
		{"head_lod6_mesh", "head_LOD57_shader_shader"},
		{"teeth_lod6_mesh", "teeth_shader_shader"},
		{"eyeLeft_lod6_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod6_mesh", "eyeRight_shader_shader"},
		/* LOD7 meshes */
		{"head_lod7_mesh", "head_LOD57_shader_shader"},
		{"teeth_lod7_mesh", "teeth_shader_shader"},
		{"eyeLeft_lod7_mesh", "eyeLeft_shader_shader"},
		{"eyeRight_lod7_mesh", "eyeRight_shader_shader"},
		/* body meshes */
		{"body_lod0_mesh", "body_shader_shader"},
		{"body_lod1_mesh", "body_shader_shader"},
		{"body_lod2_mesh", "body_shader_shader"},
		{"body_lod3_mesh", "body_shader_shader"},
		{"combined_lod0_mesh", "body_shader_shader"},
		{"combined_lod1_mesh", "body_shader_shader"},
		{"combined_lod2_mesh", "body_shader_shader"},
		{"combined_lod3_mesh", "body_shader_shader"}
	};
}

UMetaHumanInterchangeDnaTranslator::UMetaHumanInterchangeDnaTranslator()
{
}

void UMetaHumanInterchangeDnaTranslator::ReleaseSource()
{
}

void UMetaHumanInterchangeDnaTranslator::ImportFinish()
{
}

bool UMetaHumanInterchangeDnaTranslator::IsThreadSafe() const
{
	// This translator is not using dispatcher to translate and return payloads
	return false;
}

EInterchangeTranslatorType UMetaHumanInterchangeDnaTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Assets;
}

EInterchangeTranslatorAssetType UMetaHumanInterchangeDnaTranslator::GetSupportedAssetTypes() const
{
	//DNA translator supports only Meshes
	return EInterchangeTranslatorAssetType::Meshes;
}

TArray<FString> UMetaHumanInterchangeDnaTranslator::GetSupportedFormats() const
{
	TArray<FString> DnaExtensions;

	//TODO: Remove the dna import convenience function when actual dna importer is implemented
	DnaExtensions.Add(TEXT("dna;MetaHuman DNA format"));

	return DnaExtensions;
}

bool UMetaHumanInterchangeDnaTranslator::Translate(UInterchangeBaseNodeContainer& NodeContainer) const
{
	// Interchange handles the source file upload from the temporary DNA file.
	FString FilePath = GetSourceData()->GetFilename();
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(InterchangeDNATranslator, Error, TEXT("Temporary DNA file %s does not exist."), *FilePath);
		return false;
	}

	TArray<uint8> DNADataAsBuffer;
	if (FFileHelper::LoadFileToArray(DNADataAsBuffer, *FilePath))
	{
		const_cast<UMetaHumanInterchangeDnaTranslator*>(this)->DNAReader = ReadDNAFromBuffer(&DNADataAsBuffer, EDNADataLayer::All);
	}

	if (DNAReader == nullptr)
	{
		UE_LOG(InterchangeDNATranslator, Error, TEXT("Failed to load temporary DNA file at %s."), *FilePath);
		return false;
	}

	uint16 MeshCount = DNAReader->GetMeshCount();

	//Create one material node per mesh.
	TArray<TPair<FString, FString>> MaterialSlots;
	MaterialSlots.Reserve(MeshCount);
	FString MaterialName;
	for (int16 MaterialIndex = 0; MaterialIndex < MeshCount; MaterialIndex++)
	{
		// Remap material slots to face archetype configuration.
		const FString MeshName = DNAReader->GetMeshName(MaterialIndex);
		if (UE::MetaHuman::MaterialSlotsMapping.Contains(MeshName))
		{
			MaterialName = UE::MetaHuman::MaterialSlotsMapping[MeshName];
		}
		else
		{
			MaterialName = DNAReader->GetMeshName(MaterialIndex) + TEXT("_shader");
		}
		FString NodeUid = TEXT("\\Material\\") + MaterialName;
		MaterialSlots.Add({ MaterialName, NodeUid });
	}

	int32 LODCount = DNAReader->GetLODCount();
	for (int32 LODIndex = 0; LODIndex < LODCount; LODIndex++)
	{
		TArrayView<const uint16> LODMeshIndices = DNAReader->GetMeshIndicesForLOD(LODIndex);
		for (int16 LODMeshIndex = 0; LODMeshIndex < LODMeshIndices.Num(); LODMeshIndex++)
		{
			int32 MeshIndex = LODMeshIndices[LODMeshIndex];
			// Create Mesh node per LOD0 mesh in DNA.
			const FString MeshName = DNAReader->GetMeshName(MeshIndex);
			const FString MeshUniqueId = TEXT("\\Mesh\\") + MeshName;

			const UInterchangeMeshNode* ExistingMeshNode = Cast<UInterchangeMeshNode>(NodeContainer.GetNode(MeshUniqueId));
			UInterchangeMeshNode* MeshNode = nullptr;
			if (ExistingMeshNode)
			{
				//This mesh node was already created.
				continue;
			}

			MeshNode = NewObject<UInterchangeMeshNode>(&NodeContainer, NAME_None);
			if (!ensure(MeshNode))
			{
				UE_LOG(InterchangeDNATranslator, Error, TEXT("Mesh node allocation failed when importing DNA."));
				return false;
			}
			// Creating a SkinnedMessNode.
			NodeContainer.SetupNode(MeshNode, MeshUniqueId, MeshName, EInterchangeNodeContainerType::TranslatedAsset);
			MeshNode->SetSkinnedMesh(true); // Designate mesh as a skeletal mesh.

			// Add joint dependencies for every mesh by looking at the skin weights.
			const int32 MeshVertexCount = DNAReader->GetVertexPositionCount(MeshIndex);
			if (MeshVertexCount > 0)
			{
				TArray<FString> JointNodeUniqueIDs;
				for (int32 VertexIndex = 0; VertexIndex < MeshVertexCount; VertexIndex++)
				{
					TArrayView<const uint16> SkinJointIndices = DNAReader->GetSkinWeightsJointIndices(MeshIndex, VertexIndex);

					JointNodeUniqueIDs.Reserve(JointNodeUniqueIDs.Num() + SkinJointIndices.Num());
					for (int32 JointIndex : SkinJointIndices)
					{					
						FString JointUid = GetJointHierarchyName(DNAReader, JointIndex);
						if (!JointNodeUniqueIDs.Contains(JointUid))
						{
							JointNodeUniqueIDs.Add(JointUid);
							MeshNode->SetSkeletonDependencyUid(JointUid);
						}
					}
				}
			}
			
			// Set material slots dependencies.
			if (MaterialSlots.IsValidIndex(MeshIndex)) // Material slot names are corresponding to mesh indices in the same order.
			{
				MeshNode->SetSlotMaterialDependencyUid(MaterialSlots[MeshIndex].Key, MaterialSlots[MeshIndex].Value);
			}

			FString PayLoadKey = MeshUniqueId;
			if (ensure(!PayloadContexts.Contains(PayLoadKey)))
			{
				TSharedPtr<FDnaMeshPayloadContext> DnaMeshPayload = MakeShared<FDnaMeshPayloadContext>();
				DnaMeshPayload->bIsSkinnedMesh = MeshNode->IsSkinnedMesh();
				DnaMeshPayload->DnaLodIndex = LODIndex;
				DnaMeshPayload->DnaMeshIndex = MeshIndex;
				PayloadContexts.Add(PayLoadKey, DnaMeshPayload);
			}
			MeshNode->SetPayLoadKey(PayLoadKey, EInterchangeMeshPayLoadType::SKELETAL); // This payload key is important, it is used to fetch the Mesh container in async mode when requested.

			// Wrap up morph targets.
			const int32 MorphTargetCount = DNAReader->GetBlendShapeTargetCount(MeshIndex); // Add up all meshes (sections).
			for (int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargetCount; ++MorphTargetIndex)
			{
				// Construct MorphTarget name by combining Blend Shape Channel name and Mesh name from DNA.
				const uint16 ChannelIndex = DNAReader->GetBlendShapeChannelIndex(MeshIndex, MorphTargetIndex);
				const FString BlendShapeStr = DNAReader->GetBlendShapeChannelName(ChannelIndex);
				const FString ShapeName = MeshName + TEXT("__") + BlendShapeStr;

				FString MorphTargetAttributeName = ShapeName; // MorphTarget shape mesh name.
				FString MorphTargetUniqueID = TEXT("\\Shape\\") + ShapeName; // MorphTarget shape mesh unique ID.
				const UInterchangeMeshNode* ExistingMorphTargetNode = Cast<const UInterchangeMeshNode>(NodeContainer.GetNode(MorphTargetUniqueID));
				if (!ExistingMorphTargetNode)
				{
					UInterchangeMeshNode* MorphTargetNode = NewObject<UInterchangeMeshNode>(&NodeContainer, NAME_None);
					NodeContainer.SetupNode(MorphTargetNode, MorphTargetUniqueID, MorphTargetAttributeName, EInterchangeNodeContainerType::TranslatedAsset);
					const bool bIsMorphTarget = true;
					MorphTargetNode->SetMorphTarget(bIsMorphTarget);
					MorphTargetNode->SetMorphTargetName(ShapeName);

					FString MorphTargetPayLoadKey = MorphTargetUniqueID;
					if (ensure(!PayloadContexts.Contains(MorphTargetPayLoadKey)))
					{
						TSharedPtr<FDnaMorphTargetPayloadContext> DnaMorphTargetPayload = MakeShared<FDnaMorphTargetPayloadContext>();
						DnaMorphTargetPayload->DnaMeshIndex = MeshIndex;
						DnaMorphTargetPayload->DnaMorphTargetIndex = MorphTargetIndex;
						DnaMorphTargetPayload->DnaChannelIndex = ChannelIndex;
						PayloadContexts.Add(MorphTargetPayLoadKey, DnaMorphTargetPayload);
					}
					MorphTargetNode->SetPayLoadKey(MorphTargetPayLoadKey, EInterchangeMeshPayLoadType::MORPHTARGET);
				}
				//Create a Mesh node dependency, so the mesh node can retrieve is associate morph target
				MeshNode->SetMorphTargetDependencyUid(MorphTargetUniqueID);
			}
		}
	}

	constexpr bool bResetCache = false;

	// Add scene hierarchy.
	// This will include SceneNodes starting from an empty RootNode which is added manually (does not exist in DNA).
	UInterchangeSceneNode* RootNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);	
	const FString RootNodeUid = TEXT("RootNode");
	const FString RootNodeName = RootNodeUid;
	NodeContainer.SetupNode(RootNode, RootNodeUid, RootNodeName, EInterchangeNodeContainerType::TranslatedScene);

	UInterchangeSceneNode* CurrentMeshNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
	const FString MeshNodeName = DNAReader->GetName();
	const FString MeshNodeUid = RootNodeUid + "." + MeshNodeName;
	NodeContainer.SetupNode(CurrentMeshNode, MeshNodeUid, MeshNodeName, EInterchangeNodeContainerType::TranslatedScene, RootNode->GetUniqueID());

	UInterchangeSceneNode* LODGroupNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
	const FString LODGroupUid = MeshNodeUid + TEXT("_LODGroup");
	const FString LODGroupName = MeshNodeName + TEXT("_LODGroup");
	NodeContainer.SetupNode(LODGroupNode, LODGroupUid, LODGroupName, EInterchangeNodeContainerType::TranslatedScene, CurrentMeshNode->GetUniqueID());
	// Set LOD group attribute;
	LODGroupNode->AddSpecializedType(UE::Interchange::FSceneNodeStaticData::GetLodGroupSpecializeTypeString());

	// Inside of LODGroup node we have to specify one child SceneNode per LOD.
	// Each LOD node should contain one SceneNode per Mesh in that LOD group in hierarchical order.
	for (int32 LODIndex = 0; LODIndex < LODCount; LODIndex++)
	{
		UInterchangeSceneNode* LODNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
		const FString LODNodeName = TEXT("LOD") + FString::FromInt(LODIndex);
		const FString LODNodeUid = LODGroupUid + "." + LODNodeName;
		NodeContainer.SetupNode(LODNode, LODNodeUid, LODNodeName, EInterchangeNodeContainerType::TranslatedScene, LODGroupNode->GetUniqueID());

		// Add a SceneNode for each mesh in the LOD level.		
		TArrayView<const uint16> LODMeshIndices = DNAReader->GetMeshIndicesForLOD(LODIndex);
		for (int16 LODMeshIndex = 0; LODMeshIndex < LODMeshIndices.Num(); LODMeshIndex++)
		{
			int32 MeshIndex = LODMeshIndices[LODMeshIndex];
			const FString NodeName = DNAReader->GetMeshName(MeshIndex);
			const FString NodeUniqueId = LODGroupUid + "." + NodeName;

			const UInterchangeSceneNode* ExistingSceneNode = Cast<UInterchangeSceneNode>(NodeContainer.GetNode(NodeUniqueId));
			if (ExistingSceneNode)
			{
				//This scene node was already created.
				continue;
			}
			UInterchangeSceneNode* SceneNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
			NodeContainer.SetupNode(SceneNode, NodeUniqueId, NodeName, EInterchangeNodeContainerType::TranslatedScene, LODNode->GetUniqueID());

			FTransform LocalTransform = FTransform::Identity;
			SceneNode->SetCustomLocalTransform(&NodeContainer, LocalTransform, bResetCache);

			// Assign mesh node dependency.
			const FString MeshUniqueID = TEXT("\\Mesh\\") + NodeName;
			const UInterchangeBaseNode* MeshNode = NodeContainer.GetNode(MeshUniqueID);
			if (MeshNode)
			{
				SceneNode->SetCustomAssetInstanceUid(MeshNode->GetUniqueID());
			}

			// Assign material dependency.
			if (MaterialSlots.IsValidIndex(MeshIndex)) // Material slot names are corresponding to mesh indices in the same order.
			{
				SceneNode->SetSlotMaterialDependencyUid(MaterialSlots[MeshIndex].Key, MaterialSlots[MeshIndex].Value);
			}
		}
	}
	
	// Next, Joint hierarchy needs to be attached to a "RootNode".
	// NOTE: DNA hierarchy starts at spine04 joint, while Archetype skeleton is expected to have root->pelvis->spine01->spine02->spine03->...
	// Total of 5 joints missing at the beginning of the hierarchy. These joints are added here.
	FTransform CombinedMissingJointTransform;
	int32 JointCount = DNAReader->GetJointCount();
	FString JointRoot = MeshNodeUid;
	if (JointCount > 0 && UE::MetaHuman::DNAMissingJoints.Num()> 0 && DNAReader->GetJointName(0) != UE::MetaHuman::DNAMissingJoints[0])
	{
		JointRoot = AddDNAMissingJoints(NodeContainer, MeshNodeUid, CombinedMissingJointTransform);
	}
	
	for (int32 JointIndex = 0; JointIndex < JointCount; JointIndex++)
	{
		FString NodeName = DNAReader->GetJointName(JointIndex);
		FString NodeUniqueID = GetJointHierarchyName(DNAReader, JointIndex);
		int32 ParentIndex = DNAReader->GetJointParentIndex(JointIndex);
		const bool bIsRootNode = JointIndex == ParentIndex;
		
		UInterchangeSceneNode* JointNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
		if (!ensure(JointNode))
		{
			UE_LOG(InterchangeDNATranslator, Error, TEXT("Scene (joint) node allocation failed when importing DNA."));
			return false;
		}

		// Initialize joint node and set the parent association.
		NodeContainer.SetupNode(JointNode, NodeUniqueID, NodeName, EInterchangeNodeContainerType::TranslatedScene, !bIsRootNode ? GetJointHierarchyName(DNAReader, ParentIndex) : JointRoot);

		// Set the node default transform
		{

			FTransform DNATransform = FTransform::Identity;
			FVector JointRotationVector = DNAReader->GetNeutralJointRotation(JointIndex);
			FVector JointTranslation = DNAReader->GetNeutralJointTranslation(JointIndex);
			FVector JointScale = FVector(1.0, 1.0, 1.0);
			FRotator Rotation(JointRotationVector.X, JointRotationVector.Y, JointRotationVector.Z);

			FTransform GlobalTransform = FTransform(); // Create transform from translation and rotation of current joint.
			if (!bIsRootNode)
			{
				DNATransform.SetRotation(Rotation.Quaternion());
				DNATransform.SetTranslation(JointTranslation);
				FTransform LocalTransform = DNATransform;
				JointNode->SetCustomLocalTransform(&NodeContainer, LocalTransform, bResetCache);

				JointNode->SetCustomTimeZeroLocalTransform(&NodeContainer, LocalTransform, bResetCache);
				JointNode->SetCustomBindPoseLocalTransform(&NodeContainer, LocalTransform, bResetCache);
			}
			else
			{
				// Root node here means Spine_04 as that's the first node in the DNA
				// Transform for this node in the DNA contains absolute values. But bones are constructed
				// relative to previous joint positions.So a relative Spine_04 position can be calculated by 
				// combining the hard coded values of Spine_03 to Pelvis x Inverse of Absolute position of Spine_04
				// However rotation/translation values have to be mapped from DNA space to UE space for Spine_04
				// Taking into account the 90 degree rotation, in addition to DNAReader mapping

				FVector FlippedTranslation = FVector(JointTranslation.X, JointTranslation.Z, -JointTranslation.Y);
				FRotator RotationDNA(JointRotationVector.X, JointRotationVector.Y, JointRotationVector.Z);
				FQuat YUpToZUpRotation = FQuat(FRotator(0, 0, 90));
				FQuat TransformRotation = YUpToZUpRotation * FQuat(RotationDNA);

				DNATransform.SetRotation(TransformRotation);
				DNATransform.SetTranslation(FlippedTranslation);

				FTransform AbsoluteSpine3Inverse = CombinedMissingJointTransform.Inverse();
				FTransform LocalTransform = DNATransform * AbsoluteSpine3Inverse;

				JointNode->SetCustomLocalTransform(&NodeContainer, LocalTransform, bResetCache);
				JointNode->SetCustomTimeZeroLocalTransform(&NodeContainer, LocalTransform, bResetCache);
				JointNode->SetCustomBindPoseLocalTransform(&NodeContainer, LocalTransform, bResetCache);
			}
		}

		//Add the joint specialized type
		JointNode->AddSpecializedType(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
		JointNode->SetDisplayLabel(NodeName);
	}

	return true;
}

FString UMetaHumanInterchangeDnaTranslator::GetJointHierarchyName(TSharedPtr<IDNAReader> InDNAReader, int32 JointIndex) const
{
	TArray<FString> UniqueIDTokens;
	int32 ParentIndex = JointIndex;
	do 
	{
		UniqueIDTokens.Add(InDNAReader->GetJointName(ParentIndex));
		JointIndex = ParentIndex;
		ParentIndex = InDNAReader->GetJointParentIndex(ParentIndex);

	} while (ParentIndex != JointIndex);

	// Add missing joints (in reverse order, root being the last token added)
	for (auto it = UE::MetaHuman::DNAMissingJoints.rbegin(); it != UE::MetaHuman::DNAMissingJoints.rend(); ++it)
	{
		UniqueIDTokens.Add(*it);
	}

	FString UniqueID;
	for (int32 TokenIndex = UniqueIDTokens.Num() - 1; TokenIndex >= 0; TokenIndex--)
	{
		UniqueID += UniqueIDTokens[TokenIndex];
		if (TokenIndex > 0)
		{
			UniqueID += TEXT(".");
		}
	}
	return UniqueID;
}

FString UMetaHumanInterchangeDnaTranslator::AddDNAMissingJoints(UInterchangeBaseNodeContainer& NodeContainer, const FString& InLastNodeId, FTransform& OutCombinedTransform) const
{
	FString Heirarchy = "";
	FString LastNodeId = InLastNodeId;
	TMap<FString, FTransform> MissingTransforms;

	// It is assumed that the Transform values for pelvis, spine_01, spine_02 and spine_03 are set
	// and will not change. And that for imported head these values are the same for all MetaHumans.
	// The values below were obtained inspecting the archetype skelmesh editor.
	// BEWARE! The pitch/roll/yaw in skelmesh editor and in C++ DO NOT MATCH! The mapping is:
	// X = Y, Y = Z, Z = X

	FTransform Pelvis;
	FRotator Rotation(87.947094, 90.0, 90.0);
	Pelvis.SetRotation(Rotation.Quaternion());
	Pelvis.SetTranslation(FVector(0.0, 2.094849, 87.070755));

	FTransform Spine01;
	Rotation = FRotator(-0.000213, 10.950073, 0.0);
	Spine01.SetRotation(Rotation.Quaternion());
	Spine01.SetTranslation(FVector(2.031172, -0.104403, 0.0));

	FTransform Spine02;
	Rotation = FRotator(0.0, -7.320824, 0.0);
	Spine02.SetRotation(Rotation.Quaternion());
	Spine02.SetTranslation(FVector(4.267596, 0.0, 0.0));

	FTransform Spine03;
	Rotation = FRotator(-0.000361, -9.506168, 0.0);
	Spine03.SetRotation(Rotation.Quaternion());
	Spine03.SetTranslation(FVector(6.75445, 0.0, 0.0));

	MissingTransforms.Add("pelvis", Pelvis);
	MissingTransforms.Add("spine_01", Spine01);
	MissingTransforms.Add("spine_02", Spine02);
	MissingTransforms.Add("spine_03", Spine03);

	OutCombinedTransform = Spine03 * Spine02 * Spine01 * Pelvis;

	for (const FString& MissingJoint : UE::MetaHuman::DNAMissingJoints)
	{
		Heirarchy = Heirarchy.IsEmpty() ? MissingJoint : Heirarchy + "." + MissingJoint;
		UInterchangeSceneNode* JointNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
		NodeContainer.SetupNode(JointNode, Heirarchy, MissingJoint, EInterchangeNodeContainerType::TranslatedScene, LastNodeId);

		JointNode->SetDisplayLabel(MissingJoint);
		JointNode->SetCustomLocalTransform(&NodeContainer, FTransform::Identity);

		//Add the joint specialized type
		JointNode->AddSpecializedType(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());

		LastNodeId = Heirarchy;
		FTransform DNATransform = FTransform::Identity;

		if (MissingTransforms.Contains(MissingJoint))
		{
			DNATransform = *MissingTransforms.Find(MissingJoint);
		}

		JointNode->SetCustomLocalTransform(&NodeContainer, DNATransform, false /*bResetCache*/);
		JointNode->SetCustomTimeZeroLocalTransform(&NodeContainer, DNATransform, false /*bResetCache*/);
		JointNode->SetCustomBindPoseLocalTransform(&NodeContainer, DNATransform, false /*bResetCache*/);
	}

	return LastNodeId;
}

TOptional<UE::Interchange::FMeshPayloadData> UMetaHumanInterchangeDnaTranslator::GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const UE::Interchange::FAttributeStorage& PayloadAttributes) const
{
	using namespace UE::Interchange;
	FTransform MeshGlobalTransform;
	PayloadAttributes.GetAttribute(UE::Interchange::FAttributeKey{ MeshPayload::Attributes::MeshGlobalTransform }, MeshGlobalTransform);

	TOptional<UE::Interchange::FMeshPayloadData> Result;
	UE::Interchange::FMeshPayloadData MeshPayLoadData;

	if (FetchMeshPayloadData(PayLoadKey.UniqueId, MeshGlobalTransform, MeshPayLoadData))
	{
		if (!FStaticMeshOperations::ValidateAndFixData(MeshPayLoadData.MeshDescription, PayLoadKey.UniqueId))
		{
			UInterchangeResultError_Generic* ErrorResult = AddMessage<UInterchangeResultError_Generic>();
			ErrorResult->SourceAssetName = SourceData ? SourceData->GetFilename() : FString();
			ErrorResult->Text = LOCTEXT("GetMeshPayloadData_ValidateMeshDescriptionFail", "Invalid mesh data (NAN) was found and fix to zero. Mesh render can be bad.");
		}

		Result.Emplace(MeshPayLoadData);
	}
	return Result;
}

bool UMetaHumanInterchangeDnaTranslator::FetchMeshPayloadData(const FString& PayloadKey, const FTransform& MeshGlobalTransform, UE::Interchange::FMeshPayloadData& OutMeshPayloadData) const
{
	if (!DNAReader)
	{
		UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
		Message->Text = LOCTEXT("FetchMeshPayloadInternal_DNAReader_isNULL", "Cannot fetch mesh payload because the DNA reader is null.");
		return false;
	}

	if (!PayloadContexts.Contains(PayloadKey))
	{
		UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
		Message->Text = LOCTEXT("CannotRetrievePayloadContext", "Cannot retrieve payload; payload key doesn't have any context.");
		return false;
	}

	TSharedPtr<FDnaPayloadContextBase>& PayloadContext = PayloadContexts.FindChecked(PayloadKey);
	return PayloadContext->FetchMeshPayload(DNAReader, MeshGlobalTransform, OutMeshPayloadData);
}

bool FDnaMeshPayloadContext::FetchMeshPayload(TSharedPtr<IDNAReader> InDNAReader, const FTransform& MeshGlobalTransform, UE::Interchange::FMeshPayloadData& OutMeshPayloadData)
{
	if (FetchMeshPayloadInternal(InDNAReader, MeshGlobalTransform, OutMeshPayloadData.MeshDescription, OutMeshPayloadData.JointNames))
	{
		return true;
	}
	return false;
}

bool FDnaMeshPayloadContext::FetchMeshPayloadInternal(TSharedPtr<IDNAReader> InDNAReader, const FTransform& MeshGlobalTransform, FMeshDescription& OutMeshDescription, TArray<FString>& OutJointNames) const
{
	if (DnaMeshIndex != INDEX_NONE)
	{
		PopulateStaticMeshDescription(OutMeshDescription, *InDNAReader.Get(), DnaMeshIndex);

		// Apply the skin weights
		FSkeletalMeshAttributes SkeletalMeshAttributes(OutMeshDescription);
		SkeletalMeshAttributes.Register(true);

		//Add the influence data in the skeletal mesh description
		FSkinWeightsVertexAttributesRef VertexSkinWeights = SkeletalMeshAttributes.GetVertexSkinWeights();

		using namespace UE::AnimationCore;
		TMap<FVertexID, TArray<FBoneWeight>> RawBoneWeights;

		int32 NumSkinVerts = InDNAReader->GetVertexPositionCount(DnaMeshIndex);
		for (int32 SkinVert = 0; SkinVert < NumSkinVerts; ++SkinVert)
		{
			TArrayView<const uint16> JointIndices = InDNAReader->GetSkinWeightsJointIndices(DnaMeshIndex, SkinVert);
			TArrayView<const float> VertexWeights = InDNAReader->GetSkinWeightsValues(DnaMeshIndex, SkinVert);

			TArray<FBoneWeight> VertexBoneWeights;
			for (int32 JointIndexCtr = 0; JointIndexCtr < JointIndices.Num(); ++JointIndexCtr)
			{
				float SkinWeightValue = VertexWeights[JointIndexCtr];
				int32 CurrentBoneIndex = JointIndices[JointIndexCtr];

				VertexBoneWeights.Add(FBoneWeight(CurrentBoneIndex, SkinWeightValue));
			}

			RawBoneWeights.Add(SkinVert, VertexBoneWeights);
		}

		// Add all the raw bone weights. This will cause the weights to be sorted and re-normalized after culling to max influences.
		for (const TTuple<FVertexID, TArray<FBoneWeight>>& Item : RawBoneWeights)
		{
			VertexSkinWeights.Set(Item.Key, Item.Value);
		}

		int32 JointCount = InDNAReader->GetJointCount();

		for (int32 JointIndex = 0; JointIndex < JointCount; JointIndex++)
		{
			FString JointName = InDNAReader->GetJointName(JointIndex);
			OutJointNames.Add(JointName);
		}

		return true;
	}

	return false;
}

void FDnaMeshPayloadContext::PopulateStaticMeshDescription(FMeshDescription& OutMeshDescription, const IDNAReader& InDNAReader, const int32 InMeshIndex)
{
	FStaticMeshAttributes Attributes(OutMeshDescription);
	Attributes.Register();

	OutMeshDescription.SuspendVertexInstanceIndexing();
	OutMeshDescription.SuspendEdgeIndexing();
	OutMeshDescription.SuspendPolygonIndexing();
	OutMeshDescription.SuspendPolygonGroupIndexing();
	OutMeshDescription.SuspendUVIndexing();

	// The code to populate static mesh description was adapted from InterchangeOBJTranslator.
	// Similarly, the MeshDescription vertex and UV element buffers are in the same order as the .DNA

	TArray<int32> VertexIndexMapping;
	const int32 VertexIndexMappingNum = InDNAReader.GetVertexPositionCount(InMeshIndex);
	VertexIndexMapping.Init(0, VertexIndexMappingNum);
	for (int32 i = 0; i < VertexIndexMappingNum; i++) {	VertexIndexMapping[i] = i;	}

	TVertexInstanceAttributesRef<FVector4f> VertexColor = Attributes.GetVertexInstanceColors();

	// Create vertices and initialize positions

	TVertexAttributesRef<FVector3f> MeshPositions = Attributes.GetVertexPositions();
	OutMeshDescription.ReserveNewVertices(VertexIndexMapping.Num());
	for (int32 ObjVertexIndex : VertexIndexMapping)
	{
		FVertexID VertexIndex = OutMeshDescription.CreateVertex();
		if (MeshPositions.GetRawArray().IsValidIndex(VertexIndex))
		{
			FVector3f& Position = Attributes.GetVertexPositions()[VertexIndex];
			Position = FVector3f(InDNAReader.GetVertexPosition(InMeshIndex, ObjVertexIndex));
		}
	}

	OutMeshDescription.SetNumUVChannels(1);
	TArray<int32> UVIndexMapping;
	const int32 UVIndexMappingNum = InDNAReader.GetVertexTextureCoordinateCount(InMeshIndex);
	UVIndexMapping.Init(0, UVIndexMappingNum);
	for (int32 i = 0; i < UVIndexMappingNum; i++) { UVIndexMapping[i] = i; }

	// Create UVs and initialize values
	
	const int32 UVChannel = 0;
	OutMeshDescription.ReserveNewUVs(UVIndexMapping.Num());
	for (int32 ObjUVIndex : UVIndexMapping)
	{
		FUVID UVIndex = OutMeshDescription.CreateUV(UVChannel);
		FTextureCoordinate ObjUV = InDNAReader.GetVertexTextureCoordinate(InMeshIndex, ObjUVIndex);
		Attributes.GetUVCoordinates(UVChannel)[UVIndex] = FVector2f(ObjUV.U, ObjUV.V);
	}

	FPolygonGroupID PolygonGroupIndex = OutMeshDescription.CreatePolygonGroup();
	const FString MeshName = InDNAReader.GetMeshName(InMeshIndex);
	const FString MaterialName = UE::MetaHuman::MaterialSlotsMapping.Contains(MeshName) ? UE::MetaHuman::MaterialSlotsMapping[MeshName] : MeshName + TEXT("_shader");
	Attributes.GetPolygonGroupMaterialSlotNames()[PolygonGroupIndex] = FName(MaterialName);

	const int32 NumOfFaces = InDNAReader.GetFaceCount(InMeshIndex);
	OutMeshDescription.ReserveNewTriangles(NumOfFaces);
	OutMeshDescription.ReserveNewPolygons(NumOfFaces);
	TArray<FVertexInstanceID, TInlineAllocator<8>> VertexInstanceIDs;

	auto UVToUEBasis = [](const FVector2f& InVector)
	{
		return FVector2f(InVector.X, 1.0f - InVector.Y);
	};

	UDNAMeshVertexColorDataAsset* ColorAsset = LoadObject<UDNAMeshVertexColorDataAsset>(nullptr, TEXT("/" UE_PLUGIN_NAME "/Face/IdentityTemplate/MeshColorAsset.MeshColorAsset"));
	
	for (int32 FaceIndex = 0; FaceIndex < NumOfFaces; ++FaceIndex)
	{
		VertexInstanceIDs.Reset();
		TArray<FVertexInstanceID> VertexInstanceIds;

		TArrayView<const uint32> FaceLayout = InDNAReader.GetFaceVertexLayoutIndices(InMeshIndex, FaceIndex);		
		OutMeshDescription.ReserveNewVertexInstances(FaceLayout.Num());

		for (uint32 FaceLayoutIndex : FaceLayout)
		{
			FVertexLayout VertexData = InDNAReader.GetVertexLayout(InMeshIndex, FaceLayoutIndex);

			FVertexID VertexID = Algo::BinarySearch(VertexIndexMapping, VertexData.Position);
			FVertexInstanceID VertexInstanceID = OutMeshDescription.CreateVertexInstance(VertexID);
			VertexInstanceIDs.Add(VertexInstanceID);

			if (VertexData.Normal != INDEX_NONE)
			{
				FVector3f& Normal = Attributes.GetVertexInstanceNormals()[VertexInstanceID];
				Normal = FVector3f(InDNAReader.GetVertexNormal(InMeshIndex, VertexData.Normal));
			}

			if (VertexData.TextureCoordinate != INDEX_NONE)
			{
				auto ObjUVVal = InDNAReader.GetVertexTextureCoordinate(InMeshIndex, VertexData.TextureCoordinate);
				Attributes.GetVertexInstanceUVs()[VertexInstanceID] = UVToUEBasis(FVector2f(ObjUVVal.U, ObjUVVal.V));
			}

			VertexColor[VertexInstanceID] = ColorAsset->GetColorByMeshAndIndex(MeshName, VertexID);			
		}

		OutMeshDescription.CreatePolygon(PolygonGroupIndex, VertexInstanceIDs);
	}

	OutMeshDescription.ResumeVertexInstanceIndexing();
	OutMeshDescription.ResumeEdgeIndexing();
	OutMeshDescription.ResumePolygonIndexing();
	OutMeshDescription.ResumePolygonGroupIndexing();
	OutMeshDescription.ResumeUVIndexing();
}

bool FDnaMorphTargetPayloadContext::FetchMeshPayload(TSharedPtr<IDNAReader> InDNAReader, const FTransform& MeshGlobalTransform, UE::Interchange::FMeshPayloadData& OutMeshPayloadData)
{
	return FetchMeshPayloadInternal(InDNAReader, MeshGlobalTransform, OutMeshPayloadData.MeshDescription);
}

bool FDnaMorphTargetPayloadContext::FetchMeshPayloadInternal(TSharedPtr<IDNAReader> InDNAReader, const FTransform& MeshGlobalTransform, FMeshDescription& OutMorphTargetMeshDescription) const
{
	if (DnaMeshIndex == INDEX_NONE)
	{
		UE_LOG(InterchangeDNATranslator, Error, TEXT("Unknown mesh index for morph target import."));
		return false;
	}

	const FString MorphTargetName = InDNAReader->GetBlendShapeChannelName(DnaChannelIndex);

	//Import the MorphTarget
	FSkeletalMeshAttributes MeshAttributes(OutMorphTargetMeshDescription);
	MeshAttributes.Register();

	//Extract the points into a simplified MeshDescription
	{
		OutMorphTargetMeshDescription.SuspendVertexIndexing();

		// Create vertices
		TVertexAttributesRef<FVector3f> VertexPositions = MeshAttributes.GetVertexPositions();

		int32 NumOfVerts = InDNAReader->GetVertexPositionCount(DnaMeshIndex);
		const int32 VertexOffset = OutMorphTargetMeshDescription.Vertices().Num();
		// The below code expects Num() to be equivalent to GetArraySize(), i.e. that all added elements are appended, not inserted into existing gaps
		check(VertexOffset == OutMorphTargetMeshDescription.Vertices().GetArraySize());
		//Fill the vertex array
		OutMorphTargetMeshDescription.ReserveNewVertices(NumOfVerts);
		TArray<FVertexID> VertexIDs;
		VertexIDs.Reserve(NumOfVerts);
		for (int32 VertexIndex = 0; VertexIndex < NumOfVerts; ++VertexIndex)
		{
			const int32 RealVertexIndex = VertexOffset + VertexIndex;

			FVertexID AddedVertexId = OutMorphTargetMeshDescription.CreateVertex();
			if (AddedVertexId.GetValue() != RealVertexIndex)
			{
				UE_LOG(InterchangeDNATranslator, Error, TEXT("Cannot create valid vertex for the morph target '{MorphTargetName}'."));
				return false;
			}

			const FVector VertPosition = InDNAReader->GetVertexPosition(DnaMeshIndex, VertexIndex);
			const FVector3f VertexPosition = FVector3f(VertPosition);
			VertexPositions[AddedVertexId] = VertexPosition;

			OutMorphTargetMeshDescription.CreateVertexInstance(AddedVertexId);
			VertexIDs.Add(AddedVertexId);
		}
		// First get all DNA deltas for current Morph Target.
		TArrayView<const uint32> BlendShapeVertexIndices = InDNAReader->GetBlendShapeTargetVertexIndices(DnaMeshIndex, DnaMorphTargetIndex);
		const int32 DeltaCount = BlendShapeVertexIndices.Num();
		// Add morph target deltas to vertex arrays.
		for (int32 DeltaIndex = 0; DeltaIndex < DeltaCount; ++DeltaIndex)
		{
			const int32 RealVertexIndex = VertexOffset + DeltaIndex;
			const FVector3f DeltaPosition = FVector3f(InDNAReader->GetBlendShapeTargetDelta(DnaMeshIndex, DnaMorphTargetIndex, DeltaIndex));
			//Add delta to get the final morph target position.
			if (VertexIDs.IsValidIndex(BlendShapeVertexIndices[DeltaIndex]))
			{
				const FVertexID MorphTargetVertexID = VertexIDs[BlendShapeVertexIndices[DeltaIndex]];
				VertexPositions[MorphTargetVertexID] += DeltaPosition;
			}
		}
		OutMorphTargetMeshDescription.ResumeVertexIndexing();
	}
	return true;
}

#undef LOCTEXT_NAMESPACE

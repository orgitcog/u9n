// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeDnaTranslator.h"

#include "DNACommon.h"
#include "DNAUtils.h"
#include "StaticMeshOperations.h"
#include "SkeletalMeshAttributes.h"
#include "BoneWeights.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Misc/FileHelper.h"

#include "InterchangeManager.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"

DEFINE_LOG_CATEGORY(InterchangeDNATranslator);
#define LOCTEXT_NAMESPACE "InterchangeDNATranslator"

const TArray<FString> UInterchangeDnaTranslator::DNAMissingJoints = { "root", "pelvis", "spine_01", "spine_02", "spine_03" };

UInterchangeDnaTranslator::UInterchangeDnaTranslator()
{
}

void UInterchangeDnaTranslator::ReleaseSource()
{
}

void UInterchangeDnaTranslator::ImportFinish()
{
}

bool UInterchangeDnaTranslator::IsThreadSafe() const
{
	// This translator is not using dispatcher to translate and return payloads
	return false;
}

EInterchangeTranslatorType UInterchangeDnaTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Assets;
}

EInterchangeTranslatorAssetType UInterchangeDnaTranslator::GetSupportedAssetTypes() const
{
	//DNA translator supports only Meshes
	return EInterchangeTranslatorAssetType::Meshes;
}

TArray<FString> UInterchangeDnaTranslator::GetSupportedFormats() const
{
	TArray<FString> DnaExtensions;

	//TODO: Remove the dna import convenience function when actual dna importer is implemented
	//DnaExtensions.Add(TEXT("dna;MetaHuman DNA format"));
	DnaExtensions.Add(TEXT("ardna;Dna data from AutoRig service"));

	return DnaExtensions;
}

bool UInterchangeDnaTranslator::Translate(UInterchangeBaseNodeContainer& NodeContainer) const
{
	// Interchange handles the source file upload.
	FString FilePath = GetSourceData()->GetFilename();
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(InterchangeDNATranslator, Error, TEXT("DNA file %s does not exist."), *FilePath);
		return false;
	}

	// TODO: Make ReadDNAFromFile work with the plugin stored file in cooked UEFN
	TArray<uint8> DNADataAsBuffer;
	if (FFileHelper::LoadFileToArray(DNADataAsBuffer, *FilePath))
	{
		const_cast<UInterchangeDnaTranslator*>(this)->DNAReader = ReadDNAFromBuffer(&DNADataAsBuffer, EDNADataLayer::All);
	}

	if (DNAReader == nullptr)
	{
		UE_LOG(InterchangeDNATranslator, Error, TEXT("Failed to load DNA file at %s."), *FilePath);
		return false;
	}

	uint16 MeshCount = DNAReader->GetMeshCount();

	//Create one material node per mesh.
	TArray<TPair<FString, FString>> MaterialSlots;
	MaterialSlots.Reserve(MeshCount);
	for (int16 MaterialIndex = 0; MaterialIndex < MeshCount; MaterialIndex++)
	{
		FString MaterialName = DNAReader->GetMeshName(MaterialIndex) + TEXT("_shader");
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
			MeshNode->SetPayLoadKey(PayLoadKey, EInterchangeMeshPayLoadType::SKELETAL); // This payload key is important, it is used to fetch the Mesh container in async mode when requested.
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
	const FString MeshNodeName = FPaths::GetBaseFilename(FilePath);
	const FString MeshNodeUid = RootNodeUid + "." + MeshNodeName;	
	NodeContainer.SetupNode(CurrentMeshNode, MeshNodeUid, MeshNodeName, EInterchangeNodeContainerType::TranslatedScene /*, RootNode->GetUniqueID()*/);

	UInterchangeSceneNode* LODGroupNode = NewObject<UInterchangeSceneNode>(&NodeContainer, NAME_None);
	const FString LODGroupUid = MeshNodeUid + TEXT("_LODGroup");
	const FString LODGroupName = MeshNodeName + TEXT("_LODGroup");
	NodeContainer.SetupNode(LODGroupNode, LODGroupUid, LODGroupName, EInterchangeNodeContainerType::TranslatedScene);
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

			// Assing material dependency.
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
	if (JointCount > 0 && DNAMissingJoints.Num()> 0 && DNAReader->GetJointName(0) != DNAMissingJoints[0])
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

FString UInterchangeDnaTranslator::GetJointHierarchyName(TSharedPtr<IDNAReader> InDNAReader, int32 JointIndex) const
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
	for (auto it = UInterchangeDnaTranslator::DNAMissingJoints.rbegin(); it != UInterchangeDnaTranslator::DNAMissingJoints.rend(); ++it)
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

FString UInterchangeDnaTranslator::AddDNAMissingJoints(UInterchangeBaseNodeContainer& NodeContainer, const FString& InLastNodeId, FTransform& OutCombinedTransform) const
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

	for (const FString& MissingJoint : DNAMissingJoints)
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

TOptional<UE::Interchange::FMeshPayloadData> UInterchangeDnaTranslator::GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const UE::Interchange::FAttributeStorage& PayloadAttributes) const
{
	using namespace UE::Interchange;

	FTransform MeshGlobalTransform;
	PayloadAttributes.GetAttribute(UE::Interchange::FAttributeKey{ MeshPayload::Attributes::MeshGlobalTransform }, MeshGlobalTransform);

	TOptional<UE::Interchange::FMeshPayloadData> Result;
	UE::Interchange::FMeshPayloadData MeshPayLoadData;

	if (FetchMeshPayloadData(PayLoadKey.UniqueId, MeshGlobalTransform, MeshPayLoadData.MeshDescription, MeshPayLoadData.JointNames))
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

int32 UInterchangeDnaTranslator::GetMeshIndexForPayload(const FString& PayloadKey) const
{
	int32 MeshIndex = INDEX_NONE;
	const int32 MeshCount = DNAReader->GetMeshCount();
	FString TempStr;
	FString MeshName;
	if (!PayloadKey.Split(TEXT("\\Mesh\\"), &TempStr, &MeshName))
	{
		// TODO: Maybe store MeshIndex/PayloadKey map in a Interchange DNA Context for faster retrieval.
		UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
		Message->Text = LOCTEXT("PayloadFormatInvlait", "Cannot retrieve payload; payload key is not in the requested format.");
		return false;
	}
	// Find MeshIndex by payload key.
	for (int32 Index = 0; Index < MeshCount; ++Index)
	{
		if (DNAReader->GetMeshName(Index) == MeshName)
		{
			MeshIndex = Index;
			break;
		}
	}
	if (MeshIndex == INDEX_NONE)
	{
		UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
		Message->Text = LOCTEXT("CannotRetrievePayload", "Cannot retrieve payload; payload key doesn't match any mesh within DNA.");
		return false;
	}

	return MeshIndex;
}

void UInterchangeDnaTranslator::PopulateStaticMeshDescription(FMeshDescription& OutMeshDescription, const IDNAReader &InDNAReader, const int32 InMeshIndex)
{
	FStaticMeshAttributes Attributes(OutMeshDescription);
	Attributes.Register();

	OutMeshDescription.SuspendVertexInstanceIndexing();
	OutMeshDescription.SuspendEdgeIndexing();
	OutMeshDescription.SuspendPolygonIndexing();
	OutMeshDescription.SuspendPolygonGroupIndexing();
	OutMeshDescription.SuspendUVIndexing();

	// TODO: Remove the duplication between MetaHumanInterchangeDNATranslator and InterchangeDNATranslator
	// taking into account different material name required by 2 systems

	TArray<int32> VertexIndexMapping;
	const int32 VertexIndexMappingNum = InDNAReader.GetVertexPositionCount(InMeshIndex);
	VertexIndexMapping.Init(0, VertexIndexMappingNum);
	for (int32 i = 0; i < VertexIndexMappingNum; i++) {	VertexIndexMapping[i] = i;	}

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

	const int32 UVChannel = 0;
	OutMeshDescription.ReserveNewUVs(UVIndexMapping.Num());
	for (int32 ObjUVIndex : UVIndexMapping)
	{
		FUVID UVIndex = OutMeshDescription.CreateUV(UVChannel);
		FTextureCoordinate ObjUV = InDNAReader.GetVertexTextureCoordinate(InMeshIndex, ObjUVIndex);
		Attributes.GetUVCoordinates(UVChannel)[UVIndex] = FVector2f(ObjUV.U, ObjUV.V);
	}

	FPolygonGroupID PolygonGroupIndex = OutMeshDescription.CreatePolygonGroup();
	const FString MaterialName = UMaterial::GetDefaultMaterial(MD_Surface)->GetName();
	Attributes.GetPolygonGroupMaterialSlotNames()[PolygonGroupIndex] = FName(MaterialName);

	const int32 NumOfFaces = InDNAReader.GetFaceCount(InMeshIndex);
	OutMeshDescription.ReserveNewTriangles(NumOfFaces);
	OutMeshDescription.ReserveNewPolygons(NumOfFaces);
	TArray<FVertexInstanceID, TInlineAllocator<8>> VertexInstanceIDs;

	auto UVToUEBasis = [](const FVector2f& InVector)
	{
		return FVector2f(InVector.X, 1.0f - InVector.Y);
	};

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
		}

		OutMeshDescription.CreatePolygon(PolygonGroupIndex, VertexInstanceIDs);
	}

	OutMeshDescription.ResumeVertexInstanceIndexing();
	OutMeshDescription.ResumeEdgeIndexing();
	OutMeshDescription.ResumePolygonIndexing();
	OutMeshDescription.ResumePolygonGroupIndexing();
	OutMeshDescription.ResumeUVIndexing();
}

bool UInterchangeDnaTranslator::FetchMeshPayloadData(const FString& PayloadKey, const FTransform& MeshGlobalTransform, FMeshDescription& OutMeshDescription, TArray<FString>& OutJointNames) const
{
	if (!DNAReader)
	{
		UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
		Message->Text = LOCTEXT("FetchMeshPayloadInternal_DNAReader_isNULL", "Cannot fetch mesh payload because the DNA reader is null.");
		return false;
	}

	int32 MeshIndex = GetMeshIndexForPayload(PayloadKey);
	if (MeshIndex != INDEX_NONE)
	{
		PopulateStaticMeshDescription(OutMeshDescription, *DNAReader.Get(), MeshIndex);

		// Apply the skin weights
		FSkeletalMeshAttributes SkeletalMeshAttributes(OutMeshDescription);
		SkeletalMeshAttributes.Register(true);

		//Add the influence data in the skeletal mesh description
		FSkinWeightsVertexAttributesRef VertexSkinWeights = SkeletalMeshAttributes.GetVertexSkinWeights();

		using namespace UE::AnimationCore;
		TMap<FVertexID, TArray<FBoneWeight>> RawBoneWeights;

		int32 NumSkinVerts = DNAReader->GetSkinWeightsCount(MeshIndex);
		for (int32 SkinVert = 0; SkinVert < NumSkinVerts; ++SkinVert)
		{
			TArrayView<const uint16> JointIndices = DNAReader->GetSkinWeightsJointIndices(MeshIndex, SkinVert);
			auto VertexWeights = DNAReader->GetSkinWeightsValues(MeshIndex, SkinVert);

			TArray<FBoneWeight> VertexBoneWeights;
			for (int32 JointIndexCtr = 0; JointIndexCtr < JointIndices.Num(); ++JointIndexCtr)
			{
				float SkinWeightValue = VertexWeights[JointIndexCtr];
				int32 CurrentBoneIndex = JointIndices[JointIndexCtr];

				VertexBoneWeights.Add(FBoneWeight(CurrentBoneIndex, SkinWeightValue));

				FString JointName = DNAReader->GetJointName(JointIndices[JointIndexCtr]);
			}

			RawBoneWeights.Add(SkinVert, VertexBoneWeights);
		}

		// Add all the raw bone weights. This will cause the weights to be sorted and re-normalized after culling to max influences.
		for (const TTuple<FVertexID, TArray<FBoneWeight>>& Item : RawBoneWeights)
		{
			VertexSkinWeights.Set(Item.Key, Item.Value);
		}

		int32 JointCount = DNAReader->GetJointCount();

		for (int32 JointIndex = 0; JointIndex < JointCount; JointIndex++)
		{
			FString JointName = DNAReader->GetJointName(JointIndex);
			OutJointNames.Add(JointName);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterSkelMeshUtils.h"

#include "Engine/SkeletalMesh.h"
#include "SkelMeshDNAUtils.h"
#include "DNAAsset.h"
#include "DNAUtils.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "AnimationRuntime.h"
#include "InterchangeDnaModule.h"
#include "MetaHumanRigEvaluatedState.h"
#include "Animation/AnimCurveMetadata.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Logging/StructuredLog.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Subsystem/MetaHumanCharacterSkinMaterials.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "ControlRigBlueprint.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "SkeletalMeshAttributes.h"
#include "Async/ParallelFor.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorLog.h"
#include "UI/Widgets/DNAImportDialogWidget.h"

static FAutoConsoleCommand CVarImportDNA(
	TEXT("MH.Import.DNA"),
	TEXT("Launches the DNA import dialog."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		TArray<FString> OutFiles;
		TSharedRef<SDNAImportDialogWidget> Window = SNew(SDNAImportDialogWidget);
		FSlateApplication::Get().AddModalWindow(Window, FGlobalTabmanager::Get()->GetRootWindow());
		
		const FString DNAPath = Window->GetFilePath();
		const FString FileName = Window->GetImportName();
		const FString ImportPath = TEXT("/Game/ImportedMesh");
		
		TArray<uint8> DNADataAsBuffer;
		if (FFileHelper::LoadFileToArray(DNADataAsBuffer, *DNAPath))
		{
			TSharedPtr<IDNAReader> DNAReader = ReadDNAFromBuffer(&DNADataAsBuffer, EDNADataLayer::All);
			if (DNAReader)
			{
				EMetaHumanImportDNAType ImportType = Window->GetMeshType() == "Face" ? EMetaHumanImportDNAType::Face : EMetaHumanImportDNAType::Body;
				EMetaHumanCharacterSkinPreviewMaterial MaterialType = *Window->GetSelectedMaterial().Get();
				const FString UniqueAssetName = MakeUniqueObjectName(GetTransientPackage(), USkeletalMesh::StaticClass(), FName{ FileName }, EUniqueObjectNameOptions::GloballyUnique).ToString();			
				USkeletalMesh* SkelMeshAsset = FMetaHumanCharacterSkelMeshUtils::GetSkeletalMeshAssetFromDNA(DNAReader, ImportPath, UniqueAssetName, ImportType);
				FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(SkelMeshAsset, DNAReader, true);
				FMetaHumanCharacterFaceMaterialSet Materials = FMetaHumanCharacterSkinMaterials::GetHeadPreviewMaterialInstance(MaterialType);
				FMetaHumanCharacterSkinMaterials::SetHeadMaterialsOnMesh(Materials, SkelMeshAsset);

				if (MaterialType == EMetaHumanCharacterSkinPreviewMaterial::Clay)
				{
					Materials.ForEachSkinMaterial<UMaterialInstanceDynamic>(
						[](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
						{
							Material->SetScalarParameterValue(TEXT("ClayMaterial"), 1.0f);
						}
					);
				}
			}
		}
	})
);

namespace UE::MetaHuman
{
	static TAutoConsoleVariable<bool> CVarMHCRebuildMeshDescriptionAfterInterchange
	{
		TEXT("mh.Character.RebuildMeshDescriptionAfterInterchange"),
		true,
		TEXT("Set to true to force an update of the skeletal mesh description after it has been imported through the DNA interchange."),
		ECVF_Default
	};

	void SetNormalForSoftSkinVertex(
		const FVector3f& InPosition, 
		const FVector3f& InNormal,
		FSoftSkinVertex& OutVertex,
		EVertexPositionsAndNormals InVertexUpdateOption)
	{
		if (InVertexUpdateOption == EVertexPositionsAndNormals::PositionOnly || InVertexUpdateOption == EVertexPositionsAndNormals::Both)
		{
			OutVertex.Position = InPosition;
		}

		if (InVertexUpdateOption == EVertexPositionsAndNormals::NormalsOnly || InVertexUpdateOption == EVertexPositionsAndNormals::Both)
		{
			// Normalize the input normal to ensure it's unit length.
			FVector3f TangentZVector = InNormal.GetSafeNormal();

			// Store the normal and handedness (always right-handed) in TangentZ.
			// Note that TangentX and TangentY will be regenerated
			OutVertex.TangentZ = FVector4f(TangentZVector, /*Handedness*/-1.0f);
		}
	}



	template<class IdentityState>
	static void UpdateLODModelVertexPositions(
		USkeletalMesh* InSkelMesh,
		const FMetaHumanRigEvaluatedState& InVerticesAndNormals,
		const IdentityState& InState,
		const FDNAToSkelMeshMap* InDNAToSkelMeshMap,
		ELodUpdateOption InUpdateOption,
		EVertexPositionsAndNormals InVertexUpdateOption)
	{
		FSkeletalMeshModel* ImportedModel = InSkelMesh->GetImportedModel();
		// Expects vertex map to be initialized beforehand	
		int32 LODStart = 0;
		int32 LODRangeSize = 1;
		switch (InUpdateOption)
		{
		case ELodUpdateOption::LOD0Only:
			// the default
			break;
		case ELodUpdateOption::LOD1AndHigher:
			LODStart = 1;
			LODRangeSize = ImportedModel->LODModels.Num();
			break;
		case ELodUpdateOption::All:
			LODRangeSize = ImportedModel->LODModels.Num();
			break;
		default:
			check(false);
		}


		for (int32 LODIndex = LODStart; LODIndex < LODRangeSize; LODIndex++)
		{
			FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
			int32 SectionIndex = 0;
			for (FSkelMeshSection& Section : LODModel.Sections)
			{
				const int32 DNAMeshIndex = InDNAToSkelMeshMap->ImportVtxToDNAMeshIndex[LODIndex][Section.GetVertexBufferIndex()];

				const int32 NumSoftVertices = Section.GetNumVertices();
				const TArray<TArray<int32>>& OverlappingMap = InDNAToSkelMeshMap->OverlappingVertices[LODIndex][SectionIndex];
				int32 VertexBufferIndex = Section.GetVertexBufferIndex();
				for (int32 VertexIndex = 0; VertexIndex < NumSoftVertices; VertexIndex++)
				{
					const int32 DNAVertexIndex = InDNAToSkelMeshMap->ImportVtxToDNAVtxIndex[LODIndex][VertexBufferIndex];

					if (DNAVertexIndex >= 0)
					{
						const FVector3f Position = InState.GetVertex(InVerticesAndNormals.Vertices, DNAMeshIndex, DNAVertexIndex);
						const FVector3f Normal = InState.GetVertex(InVerticesAndNormals.VertexNormals, DNAMeshIndex, DNAVertexIndex);
						SetNormalForSoftSkinVertex(Position, Normal, Section.SoftVertices[VertexIndex], InVertexUpdateOption);

						// Check if the current vertex has overlapping vertices, and then update them as well.
						const TArray<int32>& OverlappedIndices = OverlappingMap[VertexIndex];
						for (int32 OverlappingVertexIndex : OverlappedIndices)
						{
							SetNormalForSoftSkinVertex(Position, Normal, Section.SoftVertices[OverlappingVertexIndex], InVertexUpdateOption);
						}
					}
					VertexBufferIndex++;
				}
				SectionIndex++;
			}
		}
	}

	static void UpdateJoints(USkeletalMesh* InSkelMesh, IDNAReader* InDNAReader, FDNAToSkelMeshMap* InDNAToSkelMeshMap, EMetaHumanCharacterOrientation InCharacterOrientation)
	{
		{	// Scoping of RefSkelModifier
			FReferenceSkeletonModifier RefSkelModifier(InSkelMesh->GetRefSkeleton(), InSkelMesh->GetSkeleton());

			// copy here
			TArray<FTransform> RawBonePose = InSkelMesh->GetRefSkeleton().GetRawRefBonePose();

			// calculate component space ahead of current transform
			TArray<FTransform> ComponentTransforms;
			FAnimationRuntime::FillUpComponentSpaceTransforms(InSkelMesh->GetRefSkeleton(), RawBonePose, ComponentTransforms);

			const TArray<FMeshBoneInfo>& RawBoneInfo = InSkelMesh->GetRefSkeleton().GetRawRefBoneInfo();

			// Skipping root joint (index 0) to avoid blinking of the mesh due to bounding box issue
			for (uint16 JointIndex = 0; JointIndex < InDNAReader->GetJointCount(); JointIndex++)
			{
				int32 BoneIndex = InDNAToSkelMeshMap->GetUEBoneIndex(JointIndex);

				FTransform DNATransform = FTransform::Identity;

				// Updating bind pose affects just translations.
				FVector Translate = InDNAReader->GetNeutralJointTranslation(JointIndex);
				FVector RotationVector = InDNAReader->GetNeutralJointRotation(JointIndex);
				FRotator Rotation(RotationVector.X, RotationVector.Y, RotationVector.Z);

				if (InDNAReader->GetJointParentIndex(JointIndex) == JointIndex) // This is the highest joint of the dna - not necessarily the UE root bone  
				{
					if (InCharacterOrientation == EMetaHumanCharacterOrientation::Y_UP)
					{
						FQuat YUpToZUpRotation = FQuat(FRotator(0, 0, 90));
						FQuat ComponentRotation = YUpToZUpRotation * FQuat(Rotation);

						DNATransform.SetTranslation(FVector(Translate.X, Translate.Z, -Translate.Y));
						DNATransform.SetRotation(ComponentRotation);
					}
					else if (InCharacterOrientation == EMetaHumanCharacterOrientation::Z_UP)
					{
						DNATransform.SetTranslation(Translate);
						DNATransform.SetRotation(Rotation.Quaternion());
					}
					else
					{
						check(false);
					}
					
					ComponentTransforms[BoneIndex] = DNATransform;
				}
				else
				{
					DNATransform.SetTranslation(Translate);
					DNATransform.SetRotation(Rotation.Quaternion());

					if (ensure(RawBoneInfo[BoneIndex].ParentIndex != INDEX_NONE))
					{
						ComponentTransforms[BoneIndex] = DNATransform * ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex];
					}
				}

				ComponentTransforms[BoneIndex].NormalizeRotation();
			}

			for (uint16 BoneIndex = 0; BoneIndex < RawBoneInfo.Num(); BoneIndex++)
			{
				FTransform LocalTransform;

				if (BoneIndex == 0)
				{
					LocalTransform = ComponentTransforms[BoneIndex];
				}
				else
				{
					LocalTransform = ComponentTransforms[BoneIndex].GetRelativeTransform(ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex]);
				}

				LocalTransform.NormalizeRotation();

				RefSkelModifier.UpdateRefPoseTransform(BoneIndex, LocalTransform);
			}
		}

		InSkelMesh->GetRefBasesInvMatrix().Reset();
		InSkelMesh->CalculateInvRefMatrices(); // Needs to be called after RefSkelModifier is destroyed
	}

	static FVector3f GetOrientatedPosition(const FVector& InPosition, EMetaHumanCharacterOrientation InCharacterOrientation)
	{
		FVector3f OutPosition;
		if (InCharacterOrientation == EMetaHumanCharacterOrientation::Y_UP)
		{
			OutPosition = FVector3f{ InPosition };
		}
		else if (InCharacterOrientation == EMetaHumanCharacterOrientation::Z_UP)
		{
			OutPosition[0] = InPosition.X;
			OutPosition[1] = -InPosition.Z;
			OutPosition[2] = InPosition.Y;
		}
		else
		{
			check(false);
		}
		return OutPosition;
	}

	static void UpdateBaseMesh(USkeletalMesh* InSkelMesh, IDNAReader* InDNAReader, FDNAToSkelMeshMap* InDNAToSkelMeshMap, ELodUpdateOption InUpdateOption, EMetaHumanCharacterOrientation InCharacterOrientation)
	{
		FSkeletalMeshModel* ImportedModel = InSkelMesh->GetImportedModel();
		int32 LODStart = 0;
		int32 LODRangeSize = ImportedModel->LODModels.Num();

		if (InUpdateOption == ELodUpdateOption::LOD1AndHigher)
		{
			LODStart = 1;
		}
		else if (InUpdateOption == ELodUpdateOption::LOD0Only && LODRangeSize > 0)
		{
			LODRangeSize = 1;
		}

		// Expects vertex map to be initialized beforehand
		for (int32 LODIndex = LODStart; LODIndex < LODRangeSize; LODIndex++)
		{
			FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
			int32 SectionIndex = 0;
			for (FSkelMeshSection& Section : LODModel.Sections)
			{
				int32& DNAMeshIndex = InDNAToSkelMeshMap->ImportVtxToDNAMeshIndex[LODIndex][Section.GetVertexBufferIndex()];

				const int32 NumSoftVertices = Section.GetNumVertices();
				auto& OverlappingMap = InDNAToSkelMeshMap->OverlappingVertices[LODIndex][SectionIndex];
				int32 VertexBufferIndex = Section.GetVertexBufferIndex();
				for (int32 VertexIndex = 0; VertexIndex < NumSoftVertices; VertexIndex++)
				{
					int32& DNAVertexIndex = InDNAToSkelMeshMap->ImportVtxToDNAVtxIndex[LODIndex][VertexBufferIndex];

					if (DNAVertexIndex >= 0)
					{
						const FVector Position = InDNAReader->GetVertexPosition(DNAMeshIndex, DNAVertexIndex);
						FSoftSkinVertex& Vertex = Section.SoftVertices[VertexIndex];
						Vertex.Position = GetOrientatedPosition(Position, InCharacterOrientation);

						// Check if the current vertex has overlapping vertices, and then update them as well.
						TArray<int32>& OverlappedIndices = OverlappingMap[VertexIndex];
						int32 OverlappingCount = OverlappedIndices.Num();
						for (int32 OverlappingIndex = 0; OverlappingIndex < OverlappingCount; ++OverlappingIndex)
						{
							int32 OverlappingVertexIndex = OverlappedIndices[OverlappingIndex];
							FSoftSkinVertex& OverlappingVertex = Section.SoftVertices[OverlappingVertexIndex];
							OverlappingVertex.Position = GetOrientatedPosition(Position, InCharacterOrientation);
						}
					}
					VertexBufferIndex++;
				}
				SectionIndex++;
			}
		}
	}
}

void FMetaHumanCharacterSkelMeshUtils::UpdateSkelMeshFromDNA(TSharedRef<IDNAReader> InDNAReader, EUpdateFlags InUpdateFlags, TSharedRef<FDNAToSkelMeshMap>& InOutDNAToSkelMeshMap, EMetaHumanCharacterOrientation InCharacterOrientation, TNotNull<USkeletalMesh*> OutSkeletalMesh)
{
	// The order of execution in this function is fairly important and is split in 3 steps:
	// 1. The USkelMeshDNAUtils update the Import Model LOD data of the Skeletal Mesh since this is the reference for the DNA vertex map
	// 2. The Mesh Description of the Skeletal Mesh is updated from the Import Model so that the internal mesh state is in sync with the changes
	// 3. The Skeleta Mesh is built with the DDC & render data being fully updated
	// 
	// TODO: Note that it is not necessary to do update the whole mesh; the process could be simplified by updating the Mesh Description 
	// directly from the DNA and potentially not re-building the entire Skeletal Mesh instead only the required parts of the cache/DDC

	if (EnumHasAllFlags(InUpdateFlags, EUpdateFlags::Joints))
	{
		InOutDNAToSkelMeshMap->MapJoints(&InDNAReader.Get());
		UE::MetaHuman::UpdateJoints(OutSkeletalMesh, &InDNAReader.Get(), &InOutDNAToSkelMeshMap.Get(), InCharacterOrientation);
	}

	if (EnumHasAllFlags(InUpdateFlags, EUpdateFlags::BaseMesh))
	{
		UE::MetaHuman::UpdateBaseMesh(OutSkeletalMesh, &InDNAReader.Get(), &InOutDNAToSkelMeshMap.Get(), ELodUpdateOption::All, InCharacterOrientation);
	}

	if (EnumHasAllFlags(InUpdateFlags, EUpdateFlags::SkinWeights))
	{
		USkelMeshDNAUtils::UpdateSkinWeights(OutSkeletalMesh, &InDNAReader.Get(), &InOutDNAToSkelMeshMap.Get(), ELodUpdateOption::All);
	}

	if (EnumHasAnyFlags(InUpdateFlags, EUpdateFlags::DNABehavior | EUpdateFlags::DNAGeometry))
	{
		// Set the Behavior part of DNA in skeletal mesh AssetUserData
		if (UAssetUserData* UserData = OutSkeletalMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass()))
		{
			UDNAAsset* DNAAsset = CastChecked<UDNAAsset>(UserData);

			if (EnumHasAllFlags(InUpdateFlags, EUpdateFlags::DNABehavior))
			{
				DNAAsset->SetBehaviorReader(InDNAReader);
			}

			if (EnumHasAllFlags(InUpdateFlags, EUpdateFlags::DNAGeometry))
			{
				DNAAsset->SetGeometryReader(InDNAReader);
			}
		}
	}

	// Skeletal mesh has changed, so mark it as dirty
	//OutSkeletalMesh->Modify();
	OutSkeletalMesh->MarkPackageDirty();

	// Commit a Mesh Descriptions for each ImportModel LOD
	UpdateMeshDescriptionFromLODModel(OutSkeletalMesh);
	//OutSkeletalMesh->InvalidateDeriveDataCacheGUID();

	OutSkeletalMesh->PostEditChange();

	// Update the DNA vertex map since building the Skeletal Mesh can result in re-ordering of the render vertices
	InOutDNAToSkelMeshMap = TSharedRef<FDNAToSkelMeshMap>(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(OutSkeletalMesh));
}

void FMetaHumanCharacterSkelMeshUtils::UpdateMeshDescriptionFromLODModel(TNotNull<USkeletalMesh*> InSkeletalMesh)
{
	for (int32 LODIndex = 0; LODIndex < InSkeletalMesh->GetImportedModel()->LODModels.Num(); ++LODIndex)
	{
		const FSkeletalMeshLODModel& LODModel = InSkeletalMesh->GetImportedModel()->LODModels[LODIndex];
		FMeshDescription MeshDescription;
		LODModel.GetMeshDescription(InSkeletalMesh, LODIndex, MeshDescription);
		InSkeletalMesh->CreateMeshDescription(LODIndex, MoveTemp(MeshDescription));
		InSkeletalMesh->CommitMeshDescription(LODIndex);
	}
}

void FMetaHumanCharacterSkelMeshUtils::UpdateMeshDescriptionFromLODModelVerticesNormalsAndTangents(TNotNull<USkeletalMesh*> InSkeletalMesh)
{
	// based on void FSkeletalMeshLODModel::GetMeshDescription(const USkeletalMesh * InSkeletalMesh, const int32 InLODIndex, FMeshDescription & OutMeshDescription) const

	TArray<bool> IsUpdated;
	IsUpdated.AddDefaulted(InSkeletalMesh->GetImportedModel()->LODModels.Num());

	// run all LODs in parallel as USkeletalMesh::CommitMeshDescription is threadsafe
	ParallelFor(InSkeletalMesh->GetImportedModel()->LODModels.Num(), [&](int32 LODIndex)
	{
		IsUpdated[LODIndex] = false;
		const FSkeletalMeshLODModel& LODModel = InSkeletalMesh->GetImportedModel()->LODModels[LODIndex];
		FMeshDescription* MeshDescription = InSkeletalMesh->GetMeshDescription(LODIndex);
		if (!MeshDescription)
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("No mesh description for LOD %d"), LODIndex);
			return;
		}
		InSkeletalMesh->ModifyMeshDescription(LODIndex);

		FSkeletalMeshAttributes MeshAttributes(*MeshDescription);
		TVertexAttributesRef<FVector3f> VertexPositions = MeshAttributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = MeshAttributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = MeshAttributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshAttributes.GetVertexInstanceBinormalSigns();

		// Map the section vertices back to the import vertices to remove seams, but only if there's
		// mapping available.
		TArray<int32> SourceToTargetVertexMap;
		int32 TargetVertexCount = 0;

		if (LODModel.GetRawPointIndices().Num() == LODModel.NumVertices)
		{
			SourceToTargetVertexMap.Reserve(LODModel.GetRawPointIndices().Num());
			for (const uint32 VertexIndex : LODModel.GetRawPointIndices())
			{
				SourceToTargetVertexMap.Add(VertexIndex);
				TargetVertexCount = FMath::Max(TargetVertexCount, static_cast<int32>(VertexIndex));
			}
			TargetVertexCount += 1;
		}
		else
		{
			SourceToTargetVertexMap.Reserve(LODModel.NumVertices);
			for (uint32 Index = 0; Index < LODModel.NumVertices; Index++)
			{
				SourceToTargetVertexMap.Add(Index);
			}
			TargetVertexCount = LODModel.NumVertices;
		}

		if (MeshDescription->Vertices().Num() != TargetVertexCount)
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Mesh Description does not match Skeletal Mesh model for LOD %d"), LODIndex);
			return;
		}

		// verify that the target normals, tangents, and sign match in size
		int32 NextVertexInstanceID = 0;
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			NextVertexInstanceID += Section.NumTriangles * 3;
		}
		if (NextVertexInstanceID != MeshDescription->VertexInstances().Num())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Mesh Description does not match Skeletal Mesh model for LOD %d"), LODIndex);
			return;
		}

		NextVertexInstanceID = 0;
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

			const TArray<FSoftSkinVertex>& SourceVertices = Section.SoftVertices;
			for (int32 VertexIndex = 0; VertexIndex < SourceVertices.Num(); VertexIndex++)
			{
				const int32 SourceVertexIndex = VertexIndex + Section.BaseVertexIndex;
				const int32 TargetVertexIndex = SourceToTargetVertexMap[SourceVertexIndex];
				// the original method creates a target VertexIDs array that is incremental
				//const FVertexID VertexID = VertexIDs[TargetVertexIndex];
				const FVertexID VertexID = FVertexID(TargetVertexIndex);
				VertexPositions.Set(VertexID, SourceVertices[VertexIndex].Position);
			}

			for (int32 TriangleID = 0; TriangleID < int32(Section.NumTriangles); TriangleID++)
			{
				const int32 VertexIndexBase = TriangleID * 3 + Section.BaseIndex;

				for (int32 Corner = 0; Corner < 3; Corner++)
				{
					const int32 SourceVertexIndex = LODModel.IndexBuffer[VertexIndexBase + Corner];
					const int32 TargetVertexIndex = SourceToTargetVertexMap[SourceVertexIndex];

					//const FVertexID VertexID = VertexIDs[TargetVertexIndex];
					const FVertexID VertexID = FVertexID(TargetVertexIndex);
					const FVertexInstanceID VertexInstanceID = FVertexInstanceID(NextVertexInstanceID++);
					const FSoftSkinVertex& SourceVertex = SourceVertices[SourceVertexIndex - Section.BaseVertexIndex];

					// set normals, tangents, and sign
					VertexInstanceNormals.Set(VertexInstanceID, SourceVertex.TangentZ);
					VertexInstanceTangents.Set(VertexInstanceID, SourceVertex.TangentX);
					VertexInstanceBinormalSigns.Set(VertexInstanceID, FMatrix44f(
						SourceVertex.TangentX.GetSafeNormal(),
						SourceVertex.TangentY.GetSafeNormal(),
						FVector3f(SourceVertex.TangentZ.GetSafeNormal()),
						FVector3f::ZeroVector).Determinant() < 0.0f ? -1.0f : +1.0f);

				}
			}
		}
		
		IsUpdated[LODIndex] = true;

		InSkeletalMesh->CommitMeshDescription(LODIndex);
	});

	for (int32 LODIndex = 0; LODIndex < InSkeletalMesh->GetImportedModel()->LODModels.Num(); ++LODIndex)
	{
		if (!IsUpdated[LODIndex])
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Full mesh description update for lod %d"), LODIndex);
			const FSkeletalMeshLODModel& LODModel = InSkeletalMesh->GetImportedModel()->LODModels[LODIndex];
			FMeshDescription MeshDescription;
			LODModel.GetMeshDescription(InSkeletalMesh, LODIndex, MeshDescription);
			InSkeletalMesh->CreateMeshDescription(LODIndex, MoveTemp(MeshDescription));
			InSkeletalMesh->CommitMeshDescription(LODIndex);
		}
	}
}

bool FMetaHumanCharacterSkelMeshUtils::CompareDnaToSkelMeshVertices(TSharedPtr<const IDNAReader> InDNAReader, TNotNull<const USkeletalMesh*> InSkeletalMesh, const FDNAToSkelMeshMap& InDNAToSkelMeshMap, float Tolerance /*= UE_KINDA_SMALL_NUMBER*/)
{
	const FSkeletalMeshModel* ImportedModel = InSkeletalMesh->GetImportedModel();
	if (!ImportedModel)
	{
		return false;
	}

	const int32 MeshCount = InDNAReader->GetMeshCount();
	for (int32 LODIndex = 0; LODIndex < InDNAReader->GetLODCount(); LODIndex++)
	{
		if (ImportedModel->LODModels.IsValidIndex(LODIndex))
		{
			// Skeletal mesh might have fewer LODs than DNA, and that is fine.
			const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
			for (int32 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
			{
				const int32 VertexCount = InDNAReader->GetVertexPositionCount(MeshIndex);
				for (int32 DNAVertexIndex = 0; DNAVertexIndex < VertexCount; DNAVertexIndex++)
				{
					const int32 VertexIndex = InDNAToSkelMeshMap.ImportDNAVtxToUEVtxIndex[LODIndex][MeshIndex][DNAVertexIndex];
					TArray<FSoftSkinVertex> Vertices;
					LODModel.GetVertices(Vertices);
					if (Vertices.IsValidIndex(VertexIndex))
					{
						const FVector UpdatedPosition = InDNAReader->GetVertexPosition(MeshIndex, DNAVertexIndex);
						const bool bPositionsEqual = Vertices[VertexIndex].Position.Equals(FVector3f{ UpdatedPosition }, Tolerance);
						if (!bPositionsEqual)
						{
							// TODO: Log vertex index with mismatching position.
							return false;
						}
					}
					else
					{
						// TODO: Log mismatching vertex index/ DNA index not found.
						return false;
					}
				}
			}
		}
	}

	return true;
}

bool FMetaHumanCharacterSkelMeshUtils::CompareDnaToStateVerticesAndNormals(TSharedPtr<const IDNAReader> InDNAReader, const TArray<FVector3f>& InStateVertices, const TArray<FVector3f>& InStateNormals, TSharedPtr<const FMetaHumanCharacterIdentity::FState> InState, float Tolerance /*= UE_KINDA_SMALL_NUMBER*/)
{
	const int32 MeshCount = InDNAReader->GetMeshCount();
	for (int32 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
	{
		const int32 VertexCount = InDNAReader->GetVertexPositionCount(MeshIndex);
		for (int32 DNAVertexIndex = 0; DNAVertexIndex < VertexCount; DNAVertexIndex++)
		{
			const FVector UpdatedPosition = InDNAReader->GetVertexPosition(MeshIndex, DNAVertexIndex);
			const FVector3f StatePosition = InState->GetVertex(InStateVertices, MeshIndex, DNAVertexIndex);
			const bool bPositionsEqual = StatePosition.Equals(FVector3f{ UpdatedPosition }, Tolerance);
			if (!bPositionsEqual)
			{
				UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Vertex position mismatch at mesh %d (%s) and index %d, DNA: %f,%f,%f, State: %f,%f,%f"),MeshIndex, *InDNAReader->GetMeshName(MeshIndex), DNAVertexIndex,
					UpdatedPosition.X, UpdatedPosition.Y, UpdatedPosition.Z, StatePosition.X, StatePosition.Y, StatePosition.Z);
				return false;
			}

			const FVector UpdatedNormal = InDNAReader->GetVertexNormal(MeshIndex, DNAVertexIndex);
			const FVector3f StateNormal = InState->GetVertex(InStateNormals, MeshIndex, DNAVertexIndex);
			const bool bNormalsEqual = StateNormal.Equals(FVector3f{ UpdatedNormal }, Tolerance);
			if (!bNormalsEqual)
			{
				UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Vertex normal mismatch at mesh %d (%s) and index %d, DNA: %f,%f,%f, State: %f,%f,%f"), MeshIndex, *InDNAReader->GetMeshName(MeshIndex), DNAVertexIndex,
					UpdatedNormal.X, UpdatedNormal.Y, UpdatedNormal.Z, StateNormal.X, StateNormal.Y, StateNormal.Z);
				return false;
			}
		}
	}

	return true;
}

bool FMetaHumanCharacterSkelMeshUtils::CheckDNACompatibility(IDNAReader* InDnaReaderA, IDNAReader* InDnaReaderB)
{
	if (!InDnaReaderA || !InDnaReaderB)
	{
		return false;
	}

	// Joints
	{
		const uint16 JointCountA = InDnaReaderA->GetJointCount();
		const uint16 JointCountB = InDnaReaderB->GetJointCount();

		// Compare joint count
		if (JointCountA != JointCountB)
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Joint count mismatch: %d vs %d"), JointCountA, JointCountB);
			return false;
		}

		bool bJointsOk = true;
		FStringBuilderBase ResultMsg;

		for (uint16 JointIndex = 0; JointIndex < JointCountA; JointIndex++)
		{
			const uint16 JointParentA = InDnaReaderA->GetJointParentIndex(JointIndex);
			const uint16 JointParentB = InDnaReaderB->GetJointParentIndex(JointIndex);

			// Compare joint names
			if (InDnaReaderA->GetJointName(JointIndex) != InDnaReaderB->GetJointName(JointIndex))
			{
				ResultMsg.Appendf(TEXT("Joint name mismatch: '%s' vs '%s'"), *InDnaReaderA->GetJointName(JointParentA), *InDnaReaderB->GetJointName(JointParentB));
				ResultMsg.AppendChar('\n');
				bJointsOk = false;
				continue;
			}

			// Compare parents
			if (InDnaReaderA->GetJointParentIndex(JointIndex) != InDnaReaderB->GetJointParentIndex(JointIndex))
			{
				ResultMsg.Appendf(TEXT("Joint parent mismatch for joint '%s': '%s' vs '%s'"), *InDnaReaderA->GetJointName(JointIndex), *InDnaReaderA->GetJointName(JointParentA), *InDnaReaderA->GetJointName(JointParentB));
				ResultMsg.AppendChar('\n');
				bJointsOk = false;
			}
		}

		if (!bJointsOk)
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("%s"), *ResultMsg);
			return false;
		}
	}

	// Meshes
	{
		bool bMeshesOk = true;

		const uint16 MeshCountA = InDnaReaderA->GetMeshCount();
		const uint16 MeshCountB = InDnaReaderB->GetMeshCount();
		const uint16 MeshCount = FMath::Max(MeshCountA, MeshCountB);

		FStringBuilderBase ResultMsg;

		for (uint16 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
		{
			if (MeshIndex < MeshCountA && MeshIndex < MeshCountB)
			{
				const uint16 VertexCountA = InDnaReaderA->GetVertexPositionCount(MeshIndex);
				const uint16 VertexCountB = InDnaReaderB->GetVertexPositionCount(MeshIndex);

				// Compare vertex count
				if (VertexCountA != VertexCountB)
				{
					ResultMsg.Appendf(TEXT("Vertex count mismatch on mesh '%s' (mesh index: %u): %u vs %u"), *InDnaReaderA->GetMeshName(MeshIndex), MeshIndex, VertexCountA, VertexCountB);
					ResultMsg.AppendChar('\n');
					bMeshesOk = false;
				}
			}
			else
			{
				break;
			}
		}

		if (!bMeshesOk)
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("%s"), *ResultMsg);
			return false;
		}
	}

	return true;
}

void FMetaHumanCharacterSkelMeshUtils::UpdateLODModelVertexPositions(TNotNull<USkeletalMesh*> InSkelMesh, const FMetaHumanRigEvaluatedState& InVerticesAndNormals,
	const FMetaHumanCharacterIdentity::FState& InState, const FDNAToSkelMeshMap& InDNAToSkelMeshMap, ELodUpdateOption InUpdateOption, EVertexPositionsAndNormals InVertexUpdateOption)
{
	UE::MetaHuman::UpdateLODModelVertexPositions<FMetaHumanCharacterIdentity::FState>(InSkelMesh, InVerticesAndNormals, InState, &InDNAToSkelMeshMap, InUpdateOption, InVertexUpdateOption);
}

void FMetaHumanCharacterSkelMeshUtils::UpdateLODModelVertexPositions(TNotNull<USkeletalMesh*> InSkelMesh, const FMetaHumanRigEvaluatedState& InVerticesAndNormals,
	const FMetaHumanCharacterBodyIdentity::FState& InState, const FDNAToSkelMeshMap& InDNAToSkelMeshMap, ELodUpdateOption InUpdateOption, EVertexPositionsAndNormals InVertexUpdateOption)
{
	UE::MetaHuman::UpdateLODModelVertexPositions<FMetaHumanCharacterBodyIdentity::FState>(InSkelMesh, InVerticesAndNormals, InState, &InDNAToSkelMeshMap, InUpdateOption, InVertexUpdateOption);
}

void FMetaHumanCharacterSkelMeshUtils::UpdateBindPoseFromSource(TNotNull<USkeletalMesh*> InSourceSkelMesh, TNotNull<USkeletalMesh*> InTargetSkelMesh)
{
	// Scoping of RefSkelModifier
	{
		FReferenceSkeletonModifier RefSkelModifier(InTargetSkelMesh->GetRefSkeleton(), InTargetSkelMesh->GetSkeleton());

		TArray<FTransform> SourceRawBonePose = InSourceSkelMesh->GetRefSkeleton().GetRawRefBonePose();
		TArray<FMeshBoneInfo> SourceBoneInfo = InSourceSkelMesh->GetRefSkeleton().GetRefBoneInfo();

		// Set bone transforms from source pose by matching bone name
		for (int32 SourceBoneIndex = 0; SourceBoneIndex < SourceBoneInfo.Num(); SourceBoneIndex++)
		{
			int32 TargetBoneIndex = InTargetSkelMesh->GetRefSkeleton().FindBoneIndex(SourceBoneInfo[SourceBoneIndex].Name);			
			if (TargetBoneIndex == INDEX_NONE)
			{
				continue;
			}
		
			RefSkelModifier.UpdateRefPoseTransform(TargetBoneIndex, SourceRawBonePose[SourceBoneIndex]);
		}
	}
	InTargetSkelMesh->GetRefBasesInvMatrix().Reset();
	InTargetSkelMesh->CalculateInvRefMatrices(); // Needs to be called after RefSkelModifier is destroyed
}

static FAssetData GetFirstAssetData(const FName& InPackageName)
{
	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	TArray<FAssetData> AnimBPData;
	AssetRegistry.GetAssetsByPackageName(InPackageName, AnimBPData);
	if (!AnimBPData.IsEmpty())
	{
		return AnimBPData[0];
	}

	return FAssetData();
}

void FMetaHumanCharacterSkelMeshUtils::SetPostProcessAnimBP(TNotNull<USkeletalMesh*> InSkelMesh, FName PackageName)
{
	FAssetData AnimBPAsset = GetFirstAssetData(PackageName);
	if (AnimBPAsset.IsValid())
	{
		if (AnimBPAsset.IsInstanceOf(UAnimBlueprint::StaticClass()))
		{
			// UE editor is going through this route
			UAnimBlueprint* LoadedAnimBP = Cast<UAnimBlueprint>(AnimBPAsset.GetAsset());
			InSkelMesh->SetPostProcessAnimBlueprint(LoadedAnimBP->GetAnimBlueprintGeneratedClass());
		}
		else if (AnimBPAsset.IsInstanceOf(UAnimBlueprintGeneratedClass::StaticClass()))
		{
			// Cooked UEFN seems to be going via this route
			UAnimBlueprintGeneratedClass* LoadedAnimBP = Cast<UAnimBlueprintGeneratedClass>(AnimBPAsset.GetAsset());
			InSkelMesh->SetPostProcessAnimBlueprint(LoadedAnimBP);
		}
	}
	else
	{
		InSkelMesh->SetPostProcessAnimBlueprint(nullptr); 
	}
}

void FMetaHumanCharacterSkelMeshUtils::EnableRecomputeTangents(TNotNull<USkeletalMesh*> InSkelMesh)
{
	// Code extracted from PersonaMeshDetails for Recompute Tangents update
	auto SetSkelMeshSourceSectionUserData = [](FSkeletalMeshLODModel& LODModel, const int32 SectionIndex, const int32 OriginalSectionIndex)
	{
		FSkelMeshSourceSectionUserData& SourceSectionUserData = LODModel.UserSectionsData.FindOrAdd(OriginalSectionIndex);
		SourceSectionUserData.bDisabled = LODModel.Sections[SectionIndex].bDisabled;
		SourceSectionUserData.bCastShadow = LODModel.Sections[SectionIndex].bCastShadow;
		SourceSectionUserData.bVisibleInRayTracing = LODModel.Sections[SectionIndex].bVisibleInRayTracing;
		SourceSectionUserData.bRecomputeTangent = LODModel.Sections[SectionIndex].bRecomputeTangent;
		SourceSectionUserData.RecomputeTangentsVertexMaskChannel = LODModel.Sections[SectionIndex].RecomputeTangentsVertexMaskChannel;
		SourceSectionUserData.GenerateUpToLodIndex = LODModel.Sections[SectionIndex].GenerateUpToLodIndex;
		SourceSectionUserData.CorrespondClothAssetIndex = LODModel.Sections[SectionIndex].CorrespondClothAssetIndex;
		SourceSectionUserData.ClothingData = LODModel.Sections[SectionIndex].ClothingData;
	};

	// Green mask for recompute tangents is currently set to LODs [0-3]
	int32 LODNumberInMesh = InSkelMesh->GetImportedModel()->LODModels.Num();
	int32 LODsForRecompute = LODNumberInMesh > 4 ? 4 : LODNumberInMesh;
	for (int32 LODIndex = 0; LODIndex < LODsForRecompute; ++LODIndex)
	{
		FSkeletalMeshModel* ImportedModel = InSkelMesh->GetImportedModel();
		if (!ImportedModel || LODIndex >= ImportedModel->LODModels.Num())
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Warning, "No imported model data for LOD {LODIndex}", LODIndex);
			continue;
		}
		
		FSkeletalMeshLODInfo* LODInfo = InSkelMesh->GetLODInfo(LODIndex);
		LODInfo->SkinCacheUsage = ESkinCacheUsage::Enabled;
		
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
		// Recompute tangents from green mask is only valid for section with skin
		FSkelMeshSection& Section = LODModel.Sections[0];
		Section.bRecomputeTangent = true;
		Section.RecomputeTangentsVertexMaskChannel = ESkinVertexColorChannel::Green;
		SetSkelMeshSourceSectionUserData(LODModel, 0, Section.OriginalDataSectionIndex); 
	}
	
	InSkelMesh->Build();
	InSkelMesh->PostEditChange();
	InSkelMesh->InitResources();
}

void FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(TNotNull<USkeletalMesh*> InSkelMesh, TSharedPtr<IDNAReader> InDNAReader, bool bIsFaceMesh)
{
	TObjectPtr<UDNAAsset> DNAAsset = NewObject<UDNAAsset>(InSkelMesh);
	DNAAsset->SetBehaviorReader(InDNAReader);
	DNAAsset->SetGeometryReader(InDNAReader);
	if (DNAAsset)
	{
		InSkelMesh->AddAssetUserData(DNAAsset);
	}

	// Update bind pose. TODO: This should be moved to skeleton creation in Interchange.
	const TSharedRef<FDNAToSkelMeshMap> FaceDnaToSkelMeshMap = MakeShareable(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(InSkelMesh));
	FaceDnaToSkelMeshMap->MapJoints(InDNAReader.Get());
	UE::MetaHuman::UpdateJoints(InSkelMesh, InDNAReader.Get(), &FaceDnaToSkelMeshMap.Get(), EMetaHumanCharacterOrientation::Y_UP);
	constexpr EMetaHumanCharacterTemplateType TemplateType = EMetaHumanCharacterTemplateType::MetaHuman;

	if (bIsFaceMesh)
	{
		SetPostProcessAnimBP(InSkelMesh, TEXT("/" UE_PLUGIN_NAME "/Face/ABP_Face_PostProcess"));
		// Assign the physics asset to the newly create skeletal mesh
		InSkelMesh->SetPhysicsAsset(GetFaceArchetypePhysicsAsset(TemplateType));

		// Assign the LOD Settings to the face mesh
		InSkelMesh->SetLODSettings(GetFaceArchetypeLODSettings(TemplateType));

		// Assign the Face Board Control Rig
		InSkelMesh->SetDefaultAnimatingRig(GetFaceArchetypeDefaultAnimatingRig(TemplateType));

		TArray<FSkeletalMaterial>& MeshMaterials = InSkelMesh->GetMaterials();
		for (FSkeletalMaterial& Material : MeshMaterials)
		{
			const FString Name = Material.MaterialSlotName.ToString();
			// TODO: Do this in a proper way through MetaHumanCharacteSkinMaterials.
			if (Name == "eyeshell_shader_shader")
			{
				UMaterialInterface* EyeShellMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Lookdev_UHM/Eye/Materials/MI_eye_occlusion_unified.MI_eye_occlusion_unified'"));
				Material.MaterialInterface = EyeShellMaterial;
			}
			else if (!Name.Contains("head") && !Name.Contains("teeth") && !Name.Contains("eyeLeft") && !Name.Contains("eyeRight") && !Name.Contains("body") && !Name.Contains("combined"))
			{
				UMaterialInterface* EmptyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.MaterialInstanceConstant'/" UE_PLUGIN_NAME "/Materials/M_Hide.M_Hide'"));
				Material.MaterialInterface = EmptyMaterial;
			}
		}

		EnableRecomputeTangents(InSkelMesh);
	}
	else
	{
		SetPostProcessAnimBP(InSkelMesh, TEXT("/" UE_PLUGIN_NAME "/Body/ABP_Body_PostProcess"));
		InSkelMesh->SetLODSettings(GetBodyArchetypeLODSettings(TemplateType));
		InSkelMesh->SetDefaultAnimatingRig(GetBodyArchetypeDefaultAnimatingRig(TemplateType));
	}
}

TArray<FVector3f> FMetaHumanCharacterSkelMeshUtils::GetComponentSpaceJointTranslations(TNotNull<USkeletalMesh*> InSkelMesh)
{
	TArray<FTransform> RawBonePose = InSkelMesh->GetRefSkeleton().GetRawRefBonePose();
	TArray<FTransform> ComponentTransforms;
	FAnimationRuntime::FillUpComponentSpaceTransforms(InSkelMesh->GetRefSkeleton(), RawBonePose, ComponentTransforms);

	TArray<FVector3f> ComponentSpaceTranslations;
	ComponentSpaceTranslations.AddUninitialized(ComponentTransforms.Num());
	for (int32 I = 0; I < ComponentTransforms.Num(); I++)
	{
		FVector ComponentTranslation = ComponentTransforms[I].GetTranslation();
		ComponentSpaceTranslations[I] = FVector3f(ComponentTranslation.X, ComponentTranslation.Y, ComponentTranslation.Z);
	}

	return ComponentSpaceTranslations;
}

USkeletalMesh* FMetaHumanCharacterSkelMeshUtils::GetSkeletalMeshAssetFromDNA(TSharedPtr<IDNAReader>& InDNAReader, const FString& InAssetPath, const FString& InAssetName,
										   const EMetaHumanImportDNAType InImportDNAType)
{
	FInterchangeDnaModule& DNAImportModule = FInterchangeDnaModule::GetModule();
	if (USkeletalMesh* SkelMeshAsset = DNAImportModule.ImportSync(InAssetName, InAssetPath, InDNAReader, InImportDNAType))
	{
		// Interchange system doesn't make an asset transient when the transient path is supplied
		if (InAssetPath.Contains("Engine/Transient") || InAssetPath.Contains("Engine.Transient"))
		{
			SkelMeshAsset->GetPackage()->SetFlags(RF_Transient);
		}

		if (UE::MetaHuman::CVarMHCRebuildMeshDescriptionAfterInterchange.GetValueOnAnyThread())
		{
			// This seems to clear any extra allocated data for blend shapes in the mesh description during the interchange calls
			FMetaHumanCharacterSkelMeshUtils::UpdateMeshDescriptionFromLODModel(SkelMeshAsset);

			// This frees around 1.5GB of memory after importing the mesh from DNA
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		return SkelMeshAsset;
	}

	return nullptr;
}

USkeletalMesh* FMetaHumanCharacterSkelMeshUtils::CreateArchetypeSkelMeshFromDNA(const EMetaHumanImportDNAType InImportDNAType, TSharedPtr<IDNAReader>& OutArchetypeDnaReader)
{
	USkeletalMesh* SkelMeshAsset = nullptr;
	const FString DNAPath = GetArchetypeDNAPath(InImportDNAType);

	TArray<uint8> DNADataAsBuffer;
	if (FFileHelper::LoadFileToArray(DNADataAsBuffer, *DNAPath))
	{
		FString ArchetypeAssetName = GetTransientArchetypeMeshAssetName(InImportDNAType);
		OutArchetypeDnaReader = ReadDNAFromBuffer(&DNADataAsBuffer, EDNADataLayer::All);
		if (OutArchetypeDnaReader)
		{
			const FString UniqueAssetName = MakeUniqueObjectName(GetTransientPackage(), USkeletalMesh::StaticClass(), FName{ ArchetypeAssetName }, EUniqueObjectNameOptions::GloballyUnique).ToString();			
			SkelMeshAsset = GetSkeletalMeshAssetFromDNA(OutArchetypeDnaReader, TEXT("/Engine/Transient"), UniqueAssetName, InImportDNAType);
		}
	}

	return SkelMeshAsset;
}

UDNAAsset* FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAAseet(const EMetaHumanImportDNAType InImportDNAType, UObject* InOuter)
{
	const FString DNAPath = GetArchetypeDNAPath(InImportDNAType);
	return GetDNAAssetFromFile(DNAPath, InOuter);
}

FString FMetaHumanCharacterSkelMeshUtils::GetTransientArchetypeMeshAssetName(const EMetaHumanImportDNAType InImportDNAType)
{
	FString ArchetypeAssetName;
	switch (InImportDNAType)
	{
	case EMetaHumanImportDNAType::Face:
		ArchetypeAssetName = TEXT("Face");
		break;
	case EMetaHumanImportDNAType::Body:
		ArchetypeAssetName = TEXT("Body");
		break;
	case EMetaHumanImportDNAType::Combined:
		ArchetypeAssetName = TEXT("Combined");
		break;
	default:
		ArchetypeAssetName = TEXT("Default");
		break;
	}

	return ArchetypeAssetName;
}

FString FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAPath(const EMetaHumanImportDNAType InImportDNAType)
{
	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
	FString DNAPath;

	switch (InImportDNAType)
	{
	case EMetaHumanImportDNAType::Face:
		DNAPath = PluginDir + TEXT("/Face/IdentityTemplate/SKM_Face.dna");
		break;
	case EMetaHumanImportDNAType::Body:
		DNAPath = PluginDir + TEXT("/Body/IdentityTemplate/SKM_Body.dna");
		break;
	case EMetaHumanImportDNAType::Combined:
		DNAPath = PluginDir + TEXT("/Body/IdentityTemplate/body_head_combined.dna");
	default:
		break;
	}

	return DNAPath;
}

UPhysicsAsset* FMetaHumanCharacterSkelMeshUtils::GetFaceArchetypePhysicsAsset(EMetaHumanCharacterTemplateType InTemplateType)
{
	UPhysicsAsset* FaceArchetypePhysics = nullptr;

	if (ensureAlways(InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman))
	{
		FaceArchetypePhysics = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Script/Engine.PhysicsAsset'/" UE_PLUGIN_NAME "/Face/PHYS_Face.PHYS_Face'"));
	}

	return FaceArchetypePhysics;
}

USkeletalMeshLODSettings* FMetaHumanCharacterSkelMeshUtils::GetFaceArchetypeLODSettings(EMetaHumanCharacterTemplateType InTemplateType)
{
	USkeletalMeshLODSettings* FaceArchetypeLODSettings = nullptr;

	if (ensureAlways(InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman))
	{
		FaceArchetypeLODSettings = LoadObject<USkeletalMeshLODSettings>(nullptr, TEXT("/Script/Engine.SkeletalMeshLODSettings'/" UE_PLUGIN_NAME "/Face/Face_LODSettings.Face_LODSettings'"));
	}

	return FaceArchetypeLODSettings;
}

UControlRigBlueprint* FMetaHumanCharacterSkelMeshUtils::GetFaceArchetypeDefaultAnimatingRig(EMetaHumanCharacterTemplateType InTemplateType)
{
	UControlRigBlueprint* FaceArchetypeDefaultAnimatingRig = nullptr;

	if (ensureAlways(InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman))
	{
		FaceArchetypeDefaultAnimatingRig = LoadObject<UControlRigBlueprint>(nullptr, TEXT("/Script/ControlRigDeveloper.ControlRigBlueprint'/" UE_PLUGIN_NAME "/Face/Face_ControlBoard_CtrlRig.Face_ControlBoard_CtrlRig'"));
	}

	return FaceArchetypeDefaultAnimatingRig;
}

UPhysicsAsset* FMetaHumanCharacterSkelMeshUtils::GetBodyArchetypePhysicsAsset(EMetaHumanCharacterTemplateType InTemplateType)
{
	UPhysicsAsset* BodyArchetypePhysics = nullptr;

	if (ensureAlways(InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman))
	{
		BodyArchetypePhysics = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Script/Engine.PhysicsAsset'/" UE_PLUGIN_NAME "/Body/IdentityTemplate/PHYS_Body.PHYS_Body'"));
	}

	return BodyArchetypePhysics;
}

USkeletalMeshLODSettings* FMetaHumanCharacterSkelMeshUtils::GetBodyArchetypeLODSettings(EMetaHumanCharacterTemplateType InTemplateType)
{
	USkeletalMeshLODSettings* BodyArchetypeLODSettings = nullptr;

	if (ensureAlways(InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman))
	{
		BodyArchetypeLODSettings = LoadObject<USkeletalMeshLODSettings>(nullptr, TEXT("/Script/Engine.SkeletalMeshLODSettings'/" UE_PLUGIN_NAME "/Body/IdentityTemplate/Body_LODSettings.Body_LODSettings'"));
	}

	return BodyArchetypeLODSettings;
}

UControlRigBlueprint* FMetaHumanCharacterSkelMeshUtils::GetBodyArchetypeDefaultAnimatingRig(EMetaHumanCharacterTemplateType InTemplateType)
{
	UControlRigBlueprint* BodyArchetypeDefaultAnimatingRig = nullptr;

	if (ensureAlways(InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman))
	{
		BodyArchetypeDefaultAnimatingRig = LoadObject<UControlRigBlueprint>(nullptr, TEXT("/Script/ControlRigDeveloper.ControlRigBlueprint'/" UE_PLUGIN_NAME "/Common/MetaHuman_ControlRig.MetaHuman_ControlRig'"));
	}

	return BodyArchetypeDefaultAnimatingRig;
}

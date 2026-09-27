// Copyright Epic Games, Inc. All Rights Reserved.
#include "Subsystem/MetaHumanCharacterSkelmeshHelper.h"

#include "MetaHumanCharacterEditorLog.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/MeshBones.h"
#include "UDynamicMesh.h"

#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "RenderingThread.h"

#include "ConversionUtils/DynamicMeshToVolume.h"
#include "AssetUtils/CreateStaticMeshUtil.h"
#include "AssetUtils/CreateSkeletalMeshUtil.h"
#include "AssetUtils/CreateTexture2DUtil.h"
#include "ModelingObjectsCreationAPI.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshAttributes.h"
#include "MeshConversionOptions.h"
#include "DynamicMeshToMeshDescription.h"


#define LOCTEXT_NAMESPACE "MetaHumanCharacterBuild"

namespace UE::MetaHuman::Build
{
	static bool AddLODFromMeshDescription(FMeshDescription&& InMeshDescription, USkeletalMesh* InSkeletalMesh)
	{
		double time = FPlatformTime::Seconds();

		FSkeletalMeshModel* ImportedModels = InSkeletalMesh->GetImportedModel();
		const int32 LODIndex = ImportedModels->LODModels.Num();
		ImportedModels->LODModels.Add(new FSkeletalMeshLODModel);
		if (!ensure(ImportedModels->LODModels.Num() == InSkeletalMesh->GetLODNum()))
		{
			return false;
		}

		InSkeletalMesh->CreateMeshDescription(LODIndex, MoveTemp(InMeshDescription));

		return true;
	}




	static bool ValidateSkinWeightAttribute(
		const FMeshDescription& InMeshDescription,
		const FReferenceSkeleton& InReferenceSkeleton
	)
	{
		using namespace UE::AnimationCore;
		FSkeletalMeshConstAttributes MeshAttributes{ InMeshDescription };

		TArray<FName> Profiles = MeshAttributes.GetSkinWeightProfileNames();
		if (Profiles.IsEmpty())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Mesh description doesn't have a skin weight attribute."));
			return false;
		}

		FBoneIndexType BoneIndexMax = static_cast<FBoneIndexType>(InReferenceSkeleton.GetRawBoneNum());

		// We use the first profile. Usually that's the default profile, unless we have nothing but alternate profiles.
		FSkinWeightsVertexAttributesConstRef VertexSkinWeights = MeshAttributes.GetVertexSkinWeights(Profiles[0]);
		for (const FVertexID VertexID : InMeshDescription.Vertices().GetElementIDs())
		{
			for (FBoneWeight BoneWeight : VertexSkinWeights.Get(VertexID))
			{
				if (BoneWeight.GetBoneIndex() >= BoneIndexMax)
				{
					UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Mesh description's skin weight refers to a non-existent bone (%d of %d)."), BoneWeight.GetBoneIndex(), BoneIndexMax);
					return false;
				}
			}
		}
		return true;
	}


	bool InitializeSkeletalMeshFromMeshDescriptions(
		USkeletalMesh* InSkeletalMesh,
		TArrayView<const FMeshDescription*> InMeshDescriptions,
		TConstArrayView<FSkeletalMaterial> InMaterials,
		const FReferenceSkeleton& InReferenceSkeleton,
		const bool bInRecomputeNormals,
		const bool bInRecomputeTangents
	)
	{
		if (!ensure(InSkeletalMesh))
		{
			return false;
		}

		if (!InSkeletalMesh->GetImportedModel()->LODModels.IsEmpty())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Skeletal mesh '%s' is not empty"), *InSkeletalMesh->GetPathName());
			return false;
		}

		if (InMeshDescriptions.IsEmpty())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("No mesh descriptions given"));
			return false;
		}

		// Ensure all mesh descriptions have a skin weight attribute.
		for (const FMeshDescription* MeshDescription : InMeshDescriptions)
		{
			if (!ValidateSkinWeightAttribute(*MeshDescription, InReferenceSkeleton))
			{
				return false;
			}
		}

		// Set the materials before we start converting. We'll add dummy materials afterward if there are more sections
		// than materials in any of the LODs. Not the best system, but the best we have for now.
		InSkeletalMesh->SetMaterials(TArray<FSkeletalMaterial>{InMaterials});

		TSet<FName> ValidMaterialSlotNames;
		for (int32 Index = 0; Index < InMaterials.Num(); Index++)
		{
			const FSkeletalMaterial& Material = InMaterials[Index];
			if (!Material.MaterialSlotName.IsNone())
			{
				ValidMaterialSlotNames.Add(Material.MaterialSlotName);
			}
		}

		// This ensures that the render data gets built before we return, by calling PostEditChange when we fall out of scope.
		{
			//FScopedSkeletalMeshPostEditChange ScopedPostEditChange(InSkeletalMesh);
			//InSkeletalMesh->PreEditChange(nullptr);
			InSkeletalMesh->SetRefSkeleton(InReferenceSkeleton);

			// Calculate the initial pose from the reference skeleton.
			InSkeletalMesh->CalculateInvRefMatrices();

			bool bFirstSourceModel = true;

			for (const FMeshDescription* MeshDescription : InMeshDescriptions)
			{
				// Add default LOD build settings.
				FSkeletalMeshLODInfo& SkeletalLODInfo = InSkeletalMesh->AddLODInfo();
				SkeletalLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
				SkeletalLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
				SkeletalLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
				SkeletalLODInfo.LODHysteresis = 0.02f;

				SkeletalLODInfo.BuildSettings.bRecomputeNormals = bInRecomputeNormals;
				SkeletalLODInfo.BuildSettings.bRecomputeTangents = bInRecomputeTangents;

				FMeshDescription ClonedDescription(*MeshDescription);

				// Fix up the material slot names on the mesh to match the ones in the material list. If the name is
				// either NAME_None, or doesn't exist in the material list, we use the group index to index into the
				// material list to resolve the name.
				FSkeletalMeshAttributes Attributes(ClonedDescription);
				TPolygonGroupAttributesRef<FName> MaterialSlotNamesAttribute = Attributes.GetPolygonGroupMaterialSlotNames();
				for (FPolygonGroupID PolygonGroupID : ClonedDescription.PolygonGroups().GetElementIDs())
				{
					if (!ValidMaterialSlotNames.Contains(MaterialSlotNamesAttribute.Get(PolygonGroupID)))
					{
						int32 MaterialIndex = PolygonGroupID.GetValue();
						MaterialIndex = FMath::Clamp(MaterialIndex, 0, InMaterials.Num() - 1);
						MaterialSlotNamesAttribute.Set(PolygonGroupID, InMaterials[MaterialIndex].MaterialSlotName);
					}
				}

				if (!AddLODFromMeshDescription(MoveTemp(ClonedDescription), InSkeletalMesh))
				{
					// If we didn't get a model for LOD index 0, we don't have a mesh. Bail out.
					if (bFirstSourceModel)
					{
						return false;
					}

					// Otherwise, we have a model, so let's continue with what we have.
					break;
				}
				bFirstSourceModel = false;
			}
		}

		// Compute the bbox, now that we have the model mesh generated. 

		//FBox3f BoundingBox{ ForceInit };
		int32 MaxSectionCount = 0;
		for (const FSkeletalMeshLODModel& MeshModel : InSkeletalMesh->GetImportedModel()->LODModels)
		{
			MaxSectionCount = FMath::Max(MaxSectionCount, MeshModel.Sections.Num());
			/*
			// Compute the overall bbox.
			for (const FSkelMeshSection& Section : MeshModel.Sections)
			{
				for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
				{
					BoundingBox += Vertex.Position;
				}
			}
			*/
		}

		// If we're short on materials, compared to sections, add dummy materials to fill in the gap. Not ideal, but
		// best we can do for now.
		const TArray<FSkeletalMaterial>& ExistingMaterials = InSkeletalMesh->GetMaterials();
		if (MaxSectionCount > ExistingMaterials.Num())
		{
			TArray<FSkeletalMaterial> NewMaterials{ ExistingMaterials };
			for (int32 Index = ExistingMaterials.Num(); Index < MaxSectionCount; Index++)
			{
				NewMaterials.Add(FSkeletalMaterial{});
			}
			InSkeletalMesh->SetMaterials(NewMaterials);
		}

		// ignore bounds
		//InSkeletalMesh->SetImportedBounds(FBox3d{ BoundingBox });

		return true;
	}


	UE::AssetUtils::ECreateSkeletalMeshResult CreateSkeletalMeshAsset(
		UObject* InOuter,
		const UE::AssetUtils::FSkeletalMeshAssetOptions& Options,
		UE::AssetUtils::FSkeletalMeshResults& ResultsOut
	)
	{
		using namespace UE::AssetUtils;

		constexpr EObjectFlags UseFlags = RF_Transient;
		USkeletalMesh* NewSkeletalMesh = NewObject<USkeletalMesh>(InOuter, NAME_None, UseFlags);
		if (ensure(NewSkeletalMesh != nullptr) == false)
		{
			return ECreateSkeletalMeshResult::UnknownError;
		}

		if (!ensure(Options.Skeleton))
		{
			return ECreateSkeletalMeshResult::InvalidSkeleton;
		}

		const int32 UseNumSourceModels = FMath::Max(1, Options.NumSourceModels);

		TArray<const FMeshDescription*> MeshDescriptions;
		TArray<FMeshDescription> ConstructedMeshDescriptions;
		if (!Options.SourceMeshes.MoveMeshDescriptions.IsEmpty())
		{
			if (ensure(Options.SourceMeshes.MoveMeshDescriptions.Num() == UseNumSourceModels))
			{
				MeshDescriptions.Append(Options.SourceMeshes.MoveMeshDescriptions);
			}
		}
		else if (!Options.SourceMeshes.MeshDescriptions.IsEmpty())
		{
			if (ensure(Options.SourceMeshes.MeshDescriptions.Num() == UseNumSourceModels))
			{
				MeshDescriptions.Append(Options.SourceMeshes.MeshDescriptions);
			}
		}
		else if (!Options.SourceMeshes.DynamicMeshes.IsEmpty())
		{
			if (ensure(Options.SourceMeshes.DynamicMeshes.Num() == UseNumSourceModels))
			{
				for (const FDynamicMesh3* DynamicMesh : Options.SourceMeshes.DynamicMeshes)
				{
					ConstructedMeshDescriptions.AddDefaulted();
					FConversionToMeshDescriptionOptions ConverterOptions;
					ConverterOptions.bConvertBackToNonManifold = Options.bConvertBackToNonManifold;
					FDynamicMeshToMeshDescription Converter(ConverterOptions);
					FSkeletalMeshAttributes Attributes(ConstructedMeshDescriptions.Last());
					Attributes.Register();
					Converter.Convert(DynamicMesh, ConstructedMeshDescriptions.Last(), !Options.bEnableRecomputeTangents);
					MeshDescriptions.Add(&ConstructedMeshDescriptions.Last());
				}
			}
		}

		TArray<FSkeletalMaterial> Materials;
		TConstArrayView<FSkeletalMaterial> MaterialView;
		if (!Options.SkeletalMaterials.IsEmpty())
		{
			MaterialView = Options.SkeletalMaterials;
		}
		else if (!Options.AssetMaterials.IsEmpty())
		{
			for (UMaterialInterface* MaterialInterface : Options.AssetMaterials)
			{
				Materials.Add(FSkeletalMaterial(MaterialInterface));
			}
			MaterialView = Materials;
		}

		// ensure there is at least one material
		if (MaterialView.IsEmpty())
		{
			Materials.Add(FSkeletalMaterial());
			MaterialView = Materials;
		}

		if (Options.bApplyNaniteSettings)
		{
			NewSkeletalMesh->NaniteSettings = Options.NaniteSettings;
		}

		if (!InitializeSkeletalMeshFromMeshDescriptions(
			NewSkeletalMesh, MeshDescriptions, MaterialView,
			Options.RefSkeleton ? *Options.RefSkeleton : Options.Skeleton->GetReferenceSkeleton(),
			Options.bEnableRecomputeNormals, Options.bEnableRecomputeTangents))
		{
			return ECreateSkeletalMeshResult::UnknownError;
		}
		
		// Update the skeletal mesh and the skeleton so that their ref skeletons are in sync and the skeleton's preview mesh
		// is the one we just created.
		NewSkeletalMesh->SetSkeleton(Options.Skeleton);
		Options.Skeleton->MergeAllBonesToBoneTree(NewSkeletalMesh);
		if (!Options.Skeleton->GetPreviewMesh())
		{
			Options.Skeleton->SetPreviewMesh(NewSkeletalMesh);
		}

		ResultsOut.SkeletalMesh = NewSkeletalMesh;
		return ECreateSkeletalMeshResult::Ok;
	}


	static bool CreateReferenceSkeletonFromMeshLods(const TArray<FDynamicMesh3>& MeshLods, FReferenceSkeleton& RefSkeleton, bool& bOrderChanged)
	{
		TArray<FName> BoneNames;
		TArray<int32> BoneParentIdx;
		TArray<FTransform> BonePose;

		if (!UE::Geometry::FMeshBones::CombineLodBonesToReferenceSkeleton(MeshLods, BoneNames, BoneParentIdx, BonePose, bOrderChanged))
		{
			return false;
		}

		FReferenceSkeletonModifier Modifier(RefSkeleton, nullptr);

		for (int32 BoneIdx = 0; BoneIdx < BoneNames.Num(); ++BoneIdx)
		{
			Modifier.Add(FMeshBoneInfo(BoneNames[BoneIdx], BoneNames[BoneIdx].ToString(), BoneParentIdx[BoneIdx]), BonePose[BoneIdx]);
		}

		return true;
	}

	USkeletalMesh* CreateNewIncompleteSkeletalIncludingMeshDescriptions(
		UObject* InOuter,
		TArray<UDynamicMesh*> FromDynamicMeshLODs,
		USkeleton* InSkeleton,
		FGeometryScriptCreateNewSkeletalMeshAssetOptions Options,
		EGeometryScriptOutcomePins& Outcome)
	{
		using namespace UE::AssetUtils;

		Outcome = EGeometryScriptOutcomePins::Failure;
		if (FromDynamicMeshLODs.IsEmpty())
		{
			return nullptr;
		}

		for (int32 LodIdx = 0; LodIdx < FromDynamicMeshLODs.Num(); ++LodIdx)
		{
			const UDynamicMesh* FromDynamicMesh = FromDynamicMeshLODs[LodIdx];
			if (FromDynamicMesh == nullptr)
			{
				return nullptr;
			}
			if (FromDynamicMesh->GetTriangleCount() == 0)
			{
				return nullptr;
			}
			if (FromDynamicMesh->GetMeshRef().HasAttributes() == false || FromDynamicMesh->GetMeshRef().Attributes()->GetSkinWeightsAttributes().Num() == 0)
			{
				return nullptr;
			}
			if (InSkeleton == nullptr)
			{
				return nullptr;
			}
		}

		// todo: other safety checks

		FSkeletalMeshAssetOptions AssetOptions;
		AssetOptions.Skeleton = InSkeleton;

		AssetOptions.NumSourceModels = FromDynamicMeshLODs.Num();

		if (!Options.Materials.IsEmpty())
		{
			TArray<FSkeletalMaterial> Materials;

			for (const TPair<FName, TObjectPtr<UMaterialInterface>>& Item : Options.Materials)
			{
				Materials.Add(FSkeletalMaterial{ Item.Value, Item.Key });
			}
			AssetOptions.SkeletalMaterials = MoveTemp(Materials);
			AssetOptions.NumMaterialSlots = AssetOptions.SkeletalMaterials.Num();
		}
		else
		{
			AssetOptions.NumMaterialSlots = 1;
		}

		AssetOptions.bEnableRecomputeNormals = Options.bEnableRecomputeNormals;
		AssetOptions.bEnableRecomputeTangents = Options.bEnableRecomputeTangents;
		AssetOptions.bApplyNaniteSettings = Options.bApplyNaniteSettings;
		AssetOptions.NaniteSettings = Options.NaniteSettings;

		/**
		 * We are making a copy of each LOD mesh since UDynamicMesh can potentially be editable asynchronously in the future,
		 * so we should not hold onto the pointer outside the function.
		 */
		TArray<FDynamicMesh3> CopyFromDynamicMeshLODs;
		CopyFromDynamicMeshLODs.SetNum(FromDynamicMeshLODs.Num());

		for (int32 LodIdx = 0; LodIdx < FromDynamicMeshLODs.Num(); ++LodIdx)
		{
			const UDynamicMesh* LODMesh = FromDynamicMeshLODs[LodIdx];
			FDynamicMesh3* CopyLODMesh = &CopyFromDynamicMeshLODs[LodIdx];

			LODMesh->ProcessMesh([CopyLODMesh](const FDynamicMesh3& ReadMesh)
				{
					*CopyLODMesh = ReadMesh;
				});
		}

		// Check if all LODs have bone attributes.
		int32 LODWithoutBoneAttrib = -1;
		for (int32 Idx = 0; Idx < CopyFromDynamicMeshLODs.Num(); ++Idx)
		{
			if (!CopyFromDynamicMeshLODs[Idx].Attributes()->HasBones())
			{
				LODWithoutBoneAttrib = Idx;
				break;
			}
		}

		TUniquePtr<FReferenceSkeleton> RefSkeleton = nullptr;
		if (LODWithoutBoneAttrib >= 0) // If at least one LOD doesn't contain the bone attributes then add LOD meshes as is
		{
			for (const FDynamicMesh3& FromDynamicMesh : CopyFromDynamicMeshLODs)
			{
				AssetOptions.SourceMeshes.DynamicMeshes.Add(&FromDynamicMesh);
			}

			if (Options.bUseMeshBoneProportions)
			{
			}
		}
		else // If bone attributes are available then attempt to reindex the weights and if requested create a ReferenceSkeleton
		{
			TArray<FName> ToSkeleton; // array of bone names in the final reference skeleton
			bool bNeedToReindex = true; // do we need to re-index the bone weights with respect to the reference skeleton
			if (Options.bUseMeshBoneProportions)
			{
				// If mesh LODs have bone attributes and the user requested to use mesh bone proportions then we create a 
				// new reference skeleton by finding the mesh with the largest number of bones and creating reference  
				// skeleton out of its bone attributes
				RefSkeleton = MakeUnique<FReferenceSkeleton>();
				if (CreateReferenceSkeletonFromMeshLods(CopyFromDynamicMeshLODs, *RefSkeleton, bNeedToReindex))
				{
					ToSkeleton = RefSkeleton->GetRawRefBoneNames();

					// Asset will now use the custom reference skeleton instead of the InSkeleton reference skeleton
					AssetOptions.RefSkeleton = RefSkeleton.Get();
				}
				else
				{
					// if we failed to get reference skeleton from Lods, fall back to the skeleton asset
					ToSkeleton = InSkeleton->GetReferenceSkeleton().GetRawRefBoneNames();
				}
			}
			else
			{
				ToSkeleton = InSkeleton->GetReferenceSkeleton().GetRawRefBoneNames();
			}

			for (int32 LodIdx = 0; LodIdx < CopyFromDynamicMeshLODs.Num(); ++LodIdx)
			{
				FDynamicMesh3& FromDynamicMesh = CopyFromDynamicMeshLODs[LodIdx];

				if (bNeedToReindex) // potentially need to re-index the weights
				{
					FDynamicMeshAttributeSet* AttribSet = FromDynamicMesh.Attributes();

					// Check if the skeleton we are trying to bind the mesh to is the same as the current mesh skeleton.
					const TArray<FName>& FromSkeleton = AttribSet->GetBoneNames()->GetAttribValues();

					if (FromSkeleton != ToSkeleton)
					{
						for (const TPair<FName, TUniquePtr<FDynamicMeshVertexSkinWeightsAttribute>>& Entry : AttribSet->GetSkinWeightsAttributes())
						{
							FDynamicMeshVertexSkinWeightsAttribute* SkinWeightAttrib = Entry.Value.Get();

							// Reindex the bone indices
							if (SkinWeightAttrib->ReindexBoneIndicesToSkeleton(FromSkeleton, ToSkeleton) == false)
							{
								return nullptr;
							}
						}

						// Update the bones on the mesh to match the reference skeleton being used. We try to retain any existing bone color assignments
						// as much as we can.
						TMap<FName, FVector4f> BoneColors;
						if (AttribSet->GetBoneNames() && AttribSet->GetBoneColors())
						{
							for (int32 BoneIndex = 0; BoneIndex < AttribSet->GetNumBones(); BoneIndex++)
							{
								BoneColors.Add(AttribSet->GetBoneNames()->GetValue(BoneIndex), AttribSet->GetBoneColors()->GetValue(BoneIndex));
							}
						}
						const FReferenceSkeleton* ToRefSkeleton = AssetOptions.RefSkeleton ? AssetOptions.RefSkeleton : &InSkeleton->GetReferenceSkeleton();

						AttribSet->EnableBones(ToRefSkeleton->GetRawBoneNum());
						const TArray<FMeshBoneInfo>& BoneInfos = ToRefSkeleton->GetRawRefBoneInfo();
						const TArray<FTransform>& BonePoses = ToRefSkeleton->GetRawRefBonePose();

						for (int32 BoneIndex = 0; BoneIndex < ToRefSkeleton->GetRawBoneNum(); BoneIndex++)
						{
							AttribSet->GetBoneNames()->SetValue(BoneIndex, BoneInfos[BoneIndex].Name);
							AttribSet->GetBoneParentIndices()->SetValue(BoneIndex, BoneInfos[BoneIndex].ParentIndex);
							AttribSet->GetBonePoses()->SetValue(BoneIndex, BonePoses[BoneIndex]);

							if (const FVector4f* BoneColor = BoneColors.Find(BoneInfos[BoneIndex].Name))
							{
								AttribSet->GetBoneColors()->SetValue(BoneIndex, *BoneColor);
							}
						}
					}
				}

				AssetOptions.SourceMeshes.DynamicMeshes.Add(&FromDynamicMesh);
			}
		}


		FSkeletalMeshResults ResultData;
		const ECreateSkeletalMeshResult AssetResult = CreateSkeletalMeshAsset(InOuter, AssetOptions, ResultData);
		if (AssetResult != ECreateSkeletalMeshResult::Ok)
		{
			return nullptr;
		}

		Outcome = EGeometryScriptOutcomePins::Success;
		return ResultData.SkeletalMesh;
	}
}

#undef LOCTEXT_NAMESPACE

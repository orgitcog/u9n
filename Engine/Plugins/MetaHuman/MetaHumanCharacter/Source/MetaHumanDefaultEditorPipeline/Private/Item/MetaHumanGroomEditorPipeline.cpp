// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanGroomEditorPipeline.h"

#include "Item/MetaHumanGroomPipeline.h"
#include "MetaHumanDefaultEditorPipelineLog.h"
#include "MetaHumanWardrobeItem.h"
#include "MetaHumanTypes.h"
#include "Subsystem/MetaHumanCharacterBuild.h"

#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomRBFDeformer.h"
#include "AssetCompilingManager.h"
#include "Logging/StructuredLog.h"
#include "Interfaces/ITargetPlatform.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"


namespace UE::MetaHuman
{
	// Temporary helper to determine the quality from the input
	// TODO: remove and configure all optimizations from FMetaHumanGroomPipelineBuildInput properties
	static EMetaHumanQualityLevel QualityFromFaceLODs(const TArray<int32> InFaceLODs)
	{
		EMetaHumanQualityLevel Quality;
		switch (InFaceLODs.Num())
		{
			case 4:
				Quality = EMetaHumanQualityLevel::High;
				break;
			case 3:
				Quality = EMetaHumanQualityLevel::Medium;
				break;
			case 2:
				Quality = EMetaHumanQualityLevel::Low;
				break;
			default:
				Quality = EMetaHumanQualityLevel::Cinematic;
				break;
		}

		return Quality;
	}

	// Determines the maximum LOD this groom will be visible at and compares
	// this with the minimum LOD for the export quality. This allows
	// removing grooms that won't be used in MetaHumans for UEFN
	static int32 GetMaxVisibleLOD(TNotNull<const UGroomAsset*> InGroomAsset)
	{
		const TArray<FHairGroupsLOD> GroomLODGroups = InGroomAsset->GetHairGroupsLOD();
		int32 MaxVisibleLOD = INDEX_NONE;
		for (const FHairGroupsLOD& GroupLOD : GroomLODGroups)
		{
			for (int32 LODIndex = 0; LODIndex < GroupLOD.LODs.Num(); ++LODIndex)
			{
				const FHairLODSettings& HairLODSettings = GroupLOD.LODs[LODIndex];
				if (HairLODSettings.bVisible)
				{
					MaxVisibleLOD = FMath::Max(MaxVisibleLOD, LODIndex);
				}
			}
		}

		return MaxVisibleLOD;
	}

	// Reduces the hair data in the groom asset based on the input quality
	// TODO: behavior should be determined by FMetaHumanGroomPipelineBuildInput properties
	static void OptimizeGroom(TNotNull<UGroomAsset*> GroomAsset, EMetaHumanQualityLevel InQuality)
	{
		// Interpolation
		for (int32 GroupIndex = 0; GroupIndex < GroomAsset->GetNumHairGroups(); ++GroupIndex)
		{
			FHairGroupsInterpolation& HairGroupInterpolation = GroomAsset->GetHairGroupsInterpolation()[GroupIndex];
			FHairGroupsLOD& HairGroupLOD = GroomAsset->GetHairGroupsLOD()[GroupIndex];
			check(!HairGroupLOD.LODs.IsEmpty());

			FHairLODSettings& LOD1GroupSettings = HairGroupLOD.LODs[1];

			switch (InQuality)
			{
			case EMetaHumanQualityLevel::High:
				HairGroupInterpolation.DecimationSettings.CurveDecimation = LOD1GroupSettings.CurveDecimation;
				HairGroupInterpolation.DecimationSettings.VertexDecimation = LOD1GroupSettings.VertexDecimation;
				break;

			case EMetaHumanQualityLevel::Medium:
			case EMetaHumanQualityLevel::Low:
				HairGroupInterpolation.DecimationSettings.CurveDecimation = 0.0f;
				HairGroupInterpolation.DecimationSettings.VertexDecimation = 0.0f;
				break;

			default:
				break;
			}

			LOD1GroupSettings.CurveDecimation = 1.0f;
			LOD1GroupSettings.VertexDecimation = 1.0f;

			// Multiply HairWidth by ThicknessScale scale to compensate for the reduced number of strands when targeting UEFN
			FHairGroupsRendering& HairGroupRendering = GroomAsset->GetHairGroupsRendering()[GroupIndex];
			HairGroupRendering.GeometrySettings.HairWidth *= LOD1GroupSettings.ThicknessScale;
		}

		// Meshes
		GroomAsset->GetHairGroupsMeshes().RemoveAll([](const FHairGroupsMeshesSourceDescription& HairMeshDescription)
			{
				return HairMeshDescription.LODIndex == 6;
			});

		// Cards
		GroomAsset->GetHairGroupsCards().RemoveAll([InQuality](const FHairGroupsCardsSourceDescription& HairGroupCardDescription)
			{
				const int32 LODIndex = HairGroupCardDescription.LODIndex;

				switch (InQuality)
				{
				case EMetaHumanQualityLevel::High:
					return LODIndex == 0 || LODIndex == 2 || LODIndex == 4;

				case EMetaHumanQualityLevel::Medium:
					return LODIndex == 0 || LODIndex == 1 || LODIndex == 2 || LODIndex == 4;

				case EMetaHumanQualityLevel::Low:
					return true;

				default:
					break;
				}

				return false;
			});
	}

	// Downsize the Groom textures, creating new texture assets as needed
	// TODO: behavior should be determined by FMetaHumanGroomPipelineBuildInput properties
	static void DownsizeGroomTextures(
		TNotNull<UGroomAsset*> GroomAsset, 
		TNotNull<ITargetPlatform*> TargetPlatform, 
		TNotNull<UObject*> OuterForGeneratedObjects, 
		FMetaHumanPipelineBuiltData& BuiltData)
	{
		// we do the resizing considering only mip/LOD/build settings for the running Editor platform (eg. Windows)

		// Collect all textures that need to be replaced with resized ones
		TSortedMap<UTexture2D*, UTexture2D*> ResizedTextures;

		auto DownsizeAndUpdateTexture =
			[TargetPlatform, OuterForGeneratedObjects, &ResizedTextures, &BuiltData](TObjectPtr<UTexture2D>& Texture, int32 MaxSize, int32 TargetSize)
			{
				int32 BeforeSizeX;
				int32 BeforeSizeY;
				Texture->GetBuiltTextureSize(TargetPlatform, BeforeSizeX, BeforeSizeY);

				if (BeforeSizeX >= MaxSize)
				{
					if (ResizedTextures.Contains(Texture))
					{
						Texture = ResizedTextures.FindRef(Texture);
					}
					else
					{
						UTexture2D* DownsizeTexture = DuplicateObject<UTexture2D>(Texture, OuterForGeneratedObjects);
						BuiltData.Metadata.Emplace(DownsizeTexture, TEXT("Grooms/Textures"), DownsizeTexture->GetName());
						FMetaHumanCharacterEditorBuild::DownsizeTexture(DownsizeTexture, TargetSize, TargetPlatform);

						Texture = DownsizeTexture;
						ResizedTextures.Add(Texture, DownsizeTexture);
					}
				}
			};

		TArray<FHairGroupsCardsSourceDescription>& HairGroupCards = GroomAsset->GetHairGroupsCards();
		for (FHairGroupsCardsSourceDescription& HairGroupCard : HairGroupCards)
		{
			for (TObjectPtr<UTexture2D>& Texture : HairGroupCard.Textures.Textures)
			{
				if (Texture != nullptr)
				{
					const int32 MaxSize = 4096;
					const int32 TargetSize = 2048;
					DownsizeAndUpdateTexture(Texture, MaxSize, TargetSize);
				}
			}
		}

		TArray<FHairGroupsMeshesSourceDescription>& HairGroupMeshes = GroomAsset->GetHairGroupsMeshes();
		for (FHairGroupsMeshesSourceDescription& HairGroupMesh : HairGroupMeshes)
		{
			for (TObjectPtr<UTexture2D>& Texture : HairGroupMesh.Textures.Textures)
			{
				if (Texture != nullptr)
				{
					if (Texture->GetName().Contains(TEXT("_RootUVSeedCoverage"))) // _ColorXYSeedCoverage
					{
						const int32 MaxSize = 2048;
						const int32 TargetSize = 512;
						DownsizeAndUpdateTexture(Texture, MaxSize, TargetSize);
					}
					else
					{
						const int32 MaxSize = 4096;
						const int32 TargetSize = 2048;
						DownsizeAndUpdateTexture(Texture, MaxSize, TargetSize);
					}
				}
			}
		}
	}

static void ProcessSkeletalMeshes(
	const UGroomBindingAsset* GroomBinding,
	const FMetaHumanGroomPipelineBuildInput& GroomBuildInput,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	TNotNull<UObject*> OuterForGeneratedObjects,
	FMetaHumanPipelineBuiltData& GroomBuiltData)
{
	// TODO: this is temporary to determine an overall optimization based on the cloud MHC export implementation
	// Groom optimization should be configurable by data driven properties in the GroomBuildInput
	const EMetaHumanQualityLevel GroomQuality = UE::MetaHuman::QualityFromFaceLODs(GroomBuildInput.FaceLODs);

	if (Quality == EMetaHumanCharacterPaletteBuildQuality::Production)
	{
		// For production quality, create a new Groom 
		for (const TObjectPtr<USkeletalMesh>& Mesh : GroomBuildInput.BindingMeshes)
		{
			if (Mesh)
			{
				UGroomBindingAsset* CharacterBinding = DuplicateObject<UGroomBindingAsset>(GroomBinding, OuterForGeneratedObjects);
				CharacterBinding->ClearFlags(RF_Public | RF_Standalone);
				GroomBuiltData.Metadata.Emplace(CharacterBinding, TEXT("Grooms"), GroomBinding->GetName());

				FMetaHumanGroomPipelineBuildOutput& GroomBuildOutput = GroomBuiltData.BuildOutput.GetMutable<FMetaHumanGroomPipelineBuildOutput>();
				GroomBuildOutput.Bindings.Add(CharacterBinding);

				CharacterBinding->SetTargetSkeletalMesh(Mesh);

				UGroomAsset* NewGroom = DuplicateObject<UGroomAsset>(GroomBinding->GetGroom(), OuterForGeneratedObjects);
				GroomBuiltData.Metadata.Emplace(NewGroom, TEXT("Grooms"), NewGroom->GetName());
				NewGroom->PostLoad();

				// Cards - Duplicate all cards static meshes (prior to deformation)
				for (FHairGroupsCardsSourceDescription& Desc : NewGroom->GetHairGroupsCards())
				{
					if (Desc.ImportedMesh)
					{
						Desc.ImportedMesh = DuplicateObject<UStaticMesh>(Desc.ImportedMesh, OuterForGeneratedObjects);
						GroomBuiltData.Metadata.Emplace(Desc.ImportedMesh, TEXT("Grooms"), Desc.ImportedMesh->GetName());
					}
				}

				// Meshes - Duplicate all cards static meshes (prior to deformation)
				for (FHairGroupsMeshesSourceDescription& Desc : NewGroom->GetHairGroupsMeshes())
				{
					if (Desc.ImportedMesh)
					{
						Desc.ImportedMesh = DuplicateObject<UStaticMesh>(Desc.ImportedMesh, OuterForGeneratedObjects);
						GroomBuiltData.Metadata.Emplace(Desc.ImportedMesh, TEXT("Grooms"), Desc.ImportedMesh->GetName());
					}
				}

				// Bake RBF transforms. This needs to happen before decimation to match the mesh vertices
				{
					// Mask modulation not used atm
					FTextureSource* MaskSource = nullptr;
					float MaskScale = 0.0f;

					// Null pointers are allowed here, so this will work even if there is no source mesh
					FAssetCompilingManager::Get().FinishCompilationForObjects({ CharacterBinding->GetSourceSkeletalMesh(), CharacterBinding->GetTargetSkeletalMesh() });

					// Bake the RBF transformation within the groom asset
					FGroomRBFDeformer().GetRBFDeformedGroomAsset(
						GroomBinding->GetGroom(),
						CharacterBinding,
						MaskSource,
						MaskScale,
						NewGroom,
						TargetPlatform);
				}

				// Need to optimize the Groom before downsizing to have final Groom data and avoid creating textures that will not be used later on
				if (GroomQuality != EMetaHumanQualityLevel::Cinematic)
				{
					OptimizeGroom(NewGroom, GroomQuality);
					DownsizeGroomTextures(NewGroom, TargetPlatform, OuterForGeneratedObjects, GroomBuiltData);
				}

				// Need to call PostEditChangeProperty for the hair cards after any changes
				FPropertyChangedEvent HairGroupCardsChanged(UGroomAsset::StaticClass()->FindPropertyByName(UGroomAsset::GetHairGroupsCardsMemberName()));
				NewGroom->PostEditChangeProperty(HairGroupCardsChanged);

				CharacterBinding->SetGroom(NewGroom);

				// Need to reset the source mesh now that the groom was baked into the target mesh
				// NOTE: we encountered issues with the UEFN cooker hanging when the source mesh was set
				CharacterBinding->SetSourceSkeletalMesh(nullptr);
				 
				CharacterBinding->Build();
			}
		}
	}
	else
	{
		for (const TObjectPtr<USkeletalMesh>& Mesh : GroomBuildInput.BindingMeshes)
		{
			if (Mesh)
			{
				UGroomBindingAsset* CharacterBinding = DuplicateObject<UGroomBindingAsset>(GroomBinding, OuterForGeneratedObjects);
				CharacterBinding->ClearFlags(RF_Public | RF_Standalone);
				GroomBuiltData.Metadata.Emplace(CharacterBinding, TEXT("Grooms"), GroomBinding->GetName());

				FMetaHumanGroomPipelineBuildOutput& GroomBuildOutput = GroomBuiltData.BuildOutput.GetMutable<FMetaHumanGroomPipelineBuildOutput>();
				GroomBuildOutput.Bindings.Add(CharacterBinding);

				CharacterBinding->SetTargetSkeletalMesh(Mesh);

				// Kick off the async binding build for preview builds
				// 
				// Currently there's no reason to wait until this completes, as the 
				// binding asset can still be set on a Groom Component while it's 
				// building. The groom just won't appear on any actors using it until
				// the build is finished.
				CharacterBinding->Build();
			}
		}
	}
}

}

UMetaHumanGroomEditorPipeline::UMetaHumanGroomEditorPipeline()
{
	Specification = CreateDefaultSubobject<UMetaHumanCharacterEditorPipelineSpecification>("Specification");
	Specification->BuildInputStruct = FMetaHumanGroomPipelineBuildInput::StaticStruct();
}

void UMetaHumanGroomEditorPipeline::BuildItem(
	const FMetaHumanPaletteItemPath& ItemPath,
	TNotNull<const UMetaHumanWardrobeItem*> WardrobeItem,
	const FInstancedStruct& BuildInput,
	TArrayView<const FMetaHumanPinnedSlotSelection> SortedPinnedSlotSelections,
	TArrayView<const FMetaHumanPaletteItemPath> SortedItemsToExclude,
	FMetaHumanPaletteBuildCacheEntry& BuildCache,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	TNotNull<UObject*> OuterForGeneratedObjects,
	const FOnBuildComplete& OnComplete) const
{
	if (!BuildInput.GetPtr<FMetaHumanGroomPipelineBuildInput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Build input not provided to Groom pipeline during build");
		
		OnComplete.ExecuteIfBound(FMetaHumanPaletteBuiltData());
		return;
	}

	const UObject* LoadedAsset = WardrobeItem->PrincipalAsset.LoadSynchronous();
	const UGroomBindingAsset* GroomBinding = Cast<UGroomBindingAsset>(LoadedAsset);
	if (!GroomBinding)
	{
		UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Groom pipeline failed to load groom binding {Binding} during build", WardrobeItem->PrincipalAsset.ToString());
		
		OnComplete.ExecuteIfBound(FMetaHumanPaletteBuiltData());
		return;
	}

	if (!GroomBinding->GetGroom())
	{
		UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "No Groom asset assigned to binding {Binding} for item {ItemPath}", GroomBinding->GetName(), ItemPath.ToDebugString());
		
		OnComplete.ExecuteIfBound(FMetaHumanPaletteBuiltData());
		return;
	}

	const FMetaHumanGroomPipelineBuildInput& GroomBuildInput = BuildInput.Get<FMetaHumanGroomPipelineBuildInput>();

	// For production we may skip this binding if the referenced Groom has no active LODs in the range of the the face LODs
	if (Quality == EMetaHumanCharacterPaletteBuildQuality::Production &&
		!GroomBuildInput.FaceLODs.IsEmpty())	// Consider empty face LODs as full LODs
	{
		const int32 MaxVisibleLOD = UE::MetaHuman::GetMaxVisibleLOD(GroomBinding->GetGroom());
		const int32 MinLODForQuality = GroomBuildInput.FaceLODs[0];
		if (MaxVisibleLOD < MinLODForQuality)
		{
			UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Display,
				"Skipping building {Groom} because its maximum visible LOD is {MaxVisibleLOD} and the minimum Face LOD for the assembly is {MinLODForQuality}",
				GroomBinding->GetGroom()->GetName(), MaxVisibleLOD, MinLODForQuality);
				
			OnComplete.ExecuteIfBound(FMetaHumanPaletteBuiltData());
			return;
		}
	}

	FMetaHumanPaletteBuiltData BuiltDataResult;
	FMetaHumanPipelineBuiltData& GroomBuiltData = BuiltDataResult.ItemBuiltData.Add(ItemPath);
	FMetaHumanGroomPipelineBuildOutput& GroomBuildOutput = GroomBuiltData.BuildOutput.InitializeAs<FMetaHumanGroomPipelineBuildOutput>();

	UE::MetaHuman::ProcessSkeletalMeshes(GroomBinding,
										GroomBuildInput,
										Quality,
										TargetPlatform,
										OuterForGeneratedObjects,
										GroomBuiltData);

	OnComplete.ExecuteIfBound(MoveTemp(BuiltDataResult));
}

TNotNull<const UMetaHumanCharacterEditorPipelineSpecification*> UMetaHumanGroomEditorPipeline::GetSpecification() const
{
	return Specification;
}

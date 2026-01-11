// Copyright Epic Games, Inc. All Rights Reserved.

#include "Verification/VerifyMetaHumanGroom.h"

#include "MetaHumanAssetReport.h"
#include "Verification/MetaHumanCharacterVerification.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "Misc/RuntimeErrors.h"

#define LOCTEXT_NAMESPACE "VerifyMetaHumanGroom"

namespace UE::MetaHuman::Private
{
bool VerifyMeshUVs(const UStaticMesh* Mesh)
{
	// Check we have at least one lod and one UV channel as part of the data
	constexpr uint32 LodIndexZero = 0;
	if (Mesh->GetNumLODs() && Mesh->GetNumUVChannels(LodIndexZero))
	{
		// Simple check that the values aren't all (0,0)
		const FStaticMeshVertexBuffer& Buffer = Mesh->GetRenderData()->LODResources[LodIndexZero].VertexBuffers.StaticMeshVertexBuffer;
		for (uint32 UVIndex = 0; UVIndex < Buffer.GetNumVertices(); UVIndex++)
		{
			constexpr uint32 UvChannelIndexZero = 0;
			const FVector2f& UVValue = Buffer.GetVertexUV(UVIndex, UvChannelIndexZero);
			if (UVValue.X || UVValue.Y)
			{
				return true;
			}
		}
	}
	return false;
}

void VerifyGroomingMesh(const UGroomBindingAsset* GroomBindingAsset, FFormatNamedArguments Args, UMetaHumanAssetReport* Report, FBox& OutMeshBounds)
{
	USkeletalMesh* SourceGroomingMesh = GroomBindingAsset->GetSourceSkeletalMesh();

	if (!SourceGroomingMesh)
	{
		// 3003 Source grooming mesh missing
		Report->AddError({
			FText::Format(LOCTEXT("MissingSourceMesh", "Groom Binding {AssetName} does not have an associated source mesh."), Args),
			GroomBindingAsset
		});
	}

	if (!GroomBindingAsset->GetTargetSkeletalMesh())
	{
		// 3020 Target grooming mesh missing
		Report->AddError({
			FText::Format(LOCTEXT("MissingTargetMesh", "Groom Binding {AssetName} does not have an associated target mesh."), Args),
			GroomBindingAsset
		});
	}

	// Want to test for both source and target, but if we don't have a source we can't continue verification...
	if (!SourceGroomingMesh)
	{
		return;
	}

	Args.Add(TEXT("SourceSkelMesh"), FText::FromString(SourceGroomingMesh->GetName()));
	Report->AddVerbose({
		FText::Format(LOCTEXT("FoundSourceMesh", "Found {SourceSkelMesh}, using as source mesh for {AssetName}"), Args),
		GroomBindingAsset
	});

	const FBoxSphereBounds& SourceMeshBounds = SourceGroomingMesh->GetBounds();
	UGroomAsset* GroomAsset = GroomBindingAsset->GetGroom();
	check(GroomAsset);
	const FBoxSphereBounds& GroomBounds = FBoxSphereBounds(GroomAsset->GetHairDescriptionGroups().Bounds);

	if (!SourceMeshBounds.SphereRadius || !FBoxSphereBounds::BoxesIntersect(SourceMeshBounds, GroomBounds))
	{
		// 3004 Source grooming mesh not found - get extents of mesh and check they overlap with extents of groom
		Report->AddError({
			FText::Format(LOCTEXT("SourceMeshNotInCorrectLocation", "{AssetName} does not overlap spatially with {SourceSkelMesh} and so it can not be used as a source mesh."), Args),
			GroomAsset
		});
	}

	const FTriangleArray& Triangles = SourceGroomingMesh->GetMeshDescription(0)->Triangles();
	// TODO - this is a simple placeholder test to be replaced with some more in-depth topological comparison of some kind.
	if (Triangles.Num() != 64094)
	{
		// 3005 Geometry mismatch, groom can not be bound to target geometry - check that source mesh can bind to MH mesh (do we need to include archetype MH mesh for that?)
		Report->AddError({
			FText::Format(LOCTEXT("SourceMeshNotConformal", "{SourceSkelMesh} does not match the standard MetaHuman topology."), Args),
			SourceGroomingMesh
		});
	}

	OutMeshBounds = SourceMeshBounds.GetBox();
}

void VerifyGlobalStrandsInfoValid(const UGroomAsset* GroomAsset, FFormatNamedArguments Args, UMetaHumanAssetReport* Report)
{
	// 3001 groom_width attribute missing from alembic file
	if (!GroomAsset->GetHairDescription().HasAttribute(EHairAttribute::Width))
	{
		Report->AddInfo({
			FText::Format(LOCTEXT("MissingHairWidth", "The groom_width attribute was missing from the Alembic file used to generate Groom Asset {AssetName}"), Args),
			GroomAsset
		});
	}

	// 3002 groom_rootuv attribute missing from alembic file
	if (!GroomAsset->GetHairDescription().HasAttribute(EHairAttribute::RootUV))
	{
		Report->AddWarning({
			FText::Format(LOCTEXT("MissingRootUV", "The root_uv attribute was missing from the Alembic file used to generate Groom Asset {AssetName}"), Args),
			GroomAsset
		});
	}
}

void VerifyGroupsStrandsInfoValid(const UGroomAsset* GroomAsset, const FHairGroupInfoWithVisibility& GroupInfo, FFormatNamedArguments Args, UMetaHumanAssetReport* Report, const FBox& GroomMeshBounds)
{
	Args.Add(TEXT("GroupIndex"), GroupInfo.GroupIndex);

	// 3008 Follicle verts not aligned with grooming mesh data
	const FBox& StrandsBounds = GroomAsset->GetHairGroupsPlatformData()[GroupInfo.GroupIndex].Strands.GetBounds();
	if (GroomMeshBounds.IsValid && !GroomMeshBounds.Intersect(StrandsBounds))
	{
		Report->AddError({
			FText::Format(LOCTEXT("StrandsNotAlignedToSourceMesh", "Group {GroupIndex} on Groom Asset {AssetName} does not have strands aligned with the source mesh"), Args),
			GroomAsset
		});
	}


	// 3009 Number of vertices per curve exceeding 255
	const FHairGroupPlatformData& PlatformData = GroomAsset->GetHairGroupsPlatformData()[GroupInfo.GroupIndex];
	const uint32 StrandsFlags = PlatformData.Strands.BulkData.Header.Flags;
	const uint32 GuidesFlags = PlatformData.Guides.BulkData.Header.Flags;
	if (StrandsFlags & FHairStrandsBulkData::DataFlags_HasTrimmedPoint || GuidesFlags & FHairStrandsBulkData::DataFlags_HasTrimmedPoint)
	{
		Report->AddWarning({
			FText::Format(LOCTEXT("TooManyVerticesPerCurve", "Group {GroupIndex} on Groom Asset {AssetName} has more than 255 vertices per curve"), Args),
			GroomAsset
		});
	}

	// 3010 Number of vertices exceeding16 mil per group
	Args.Add(TEXT("MaxNumPoints"), HAIR_MAX_NUM_POINT_PER_GROUP);
	if (GroupInfo.NumCurveVertices + GroupInfo.NumGuideVertices > HAIR_MAX_NUM_POINT_PER_GROUP)
	{
		Report->AddWarning({
			FText::Format(LOCTEXT("TooManyVertices", "Strands for Group {GroupIndex} on Groom Asset {AssetName} have more than {MaxNumPoints} vertices"), Args),
			GroomAsset
		});
	}

	// 3011 Number of curves exceeding 4 mil per group
	Args.Add(TEXT("MaxNumCurves"), HAIR_MAX_NUM_CURVE_PER_GROUP);
	if (GroupInfo.NumCurves + GroupInfo.NumGuides > HAIR_MAX_NUM_CURVE_PER_GROUP)
	{
		Report->AddWarning({
			FText::Format(LOCTEXT("TooManyCurves", "Strands for Group {GroupIndex} on Groom Asset {AssetName} have more than {MaxNumCurves} curves"), Args),
			GroomAsset
		});
	}
}

void VerifyCardsInfoValid(const UGroomAsset* GroomAsset, const FHairGroupsCardsSourceDescription& CardsDescription, FFormatNamedArguments Args, UMetaHumanAssetReport* Report)
{
	Args.Add(TEXT("LodIndex"), CardsDescription.LODIndex);
	Args.Add(TEXT("GroupIndex"), CardsDescription.GroupIndex);

	const FHairGroupPlatformData& PlatformData = GroomAsset->GetHairGroupsPlatformData()[CardsDescription.GroupIndex];

	const UStaticMesh* Mesh = CardsDescription.GetMesh();
	// Cards do not have any mesh data associated
	if (!IsValid(Mesh))
	{
		return;
	}
	Args.Add(TEXT("CardsMesh"), FText::FromName(Mesh->GetFName()));

	// 3013 card mesh not alligned with strand data - get extents and check overlap is 70% of the smallest volume
	const FBox& StrandsBounds = PlatformData.Strands.GetBounds();
	const FBox& CardsBounds = PlatformData.Cards.GetBounds();
	const double MeshesVolume = CardsBounds.GetVolume();
	const double StrandsVolume = StrandsBounds.GetVolume();
	const double OverlapVolume = StrandsBounds.Overlap(CardsBounds).GetVolume();
	if (OverlapVolume < FMath::Min(MeshesVolume, StrandsVolume) * 0.7)
	{
		Report->AddError({
			FText::Format(LOCTEXT("CardsNotAlignedToStrands", "{CardsMesh} Assigned to LOD {LodIndex} of Group {GroupIndex} on Groom Asset {AssetName} does not have cards aligned with the strands"), Args),
			Mesh
		}); // TODO - should this type of error be a warning and only an error if the overlap is less than 20% or something?
	}

	// 3015 card mesh UVs missing
	if (!VerifyMeshUVs(Mesh))
	{
		Report->AddWarning({
			FText::Format(LOCTEXT("MissingCardUVs", "{CardsMesh} Assigned to LOD {LodIndex} of Group {GroupIndex} on Groom Asset {AssetName} does not have valid UVs."), Args),
			Mesh
		});
	}
}


void VerifyMeshesInfoValid(const UGroomAsset* GroomAsset, const FHairGroupsMeshesSourceDescription& MeshesDescription, FFormatNamedArguments& Args, UMetaHumanAssetReport* Report)
{
	Args.Add(TEXT("LodIndex"), MeshesDescription.LODIndex);
	Args.Add(TEXT("GroupIndex"), MeshesDescription.GroupIndex);
	const FHairGroupPlatformData& PlatformData = GroomAsset->GetHairGroupsPlatformData()[MeshesDescription.GroupIndex];

	// Helmet does not have any mesh data associated
	if (!MeshesDescription.ImportedMesh)
	{
		return;
	}

	const TObjectPtr<class UStaticMesh>& HairMesh = MeshesDescription.ImportedMesh;
	Args.Add(TEXT("MeshName"), FText::FromName(HairMesh.GetFName()));

	// 3016 helmet mesh not aligned with strand data - get extents and check overlap is 70% of the smallest volume
	const FBox& StrandsBounds = PlatformData.Strands.GetBounds();
	const FBox& MeshesBounds = PlatformData.Meshes.GetBounds();
	const double MeshesVolume = MeshesBounds.GetVolume();
	const double StrandsVolume = StrandsBounds.GetVolume();
	const double OverlapVolume = StrandsBounds.Overlap(MeshesBounds).GetVolume();
	if (OverlapVolume < FMath::Min(MeshesVolume, StrandsVolume) * 0.7)
	{
		Report->AddError({
			FText::Format(LOCTEXT("MeshesNotAlignedToStrands", "{MeshName} assigned to LOD {LodIndex} of Group {GroupIndex} on Groom Asset {AssetName} is not aligned with the strands"), Args),
			HairMesh
		}); // TODO - should this type of error be a warning and only an error if the overlap is less than 20% or something?
	}

	// 3017 helmet mesh missing UVs
	if (!VerifyMeshUVs(HairMesh))
	{
		Report->AddWarning({
			FText::Format(LOCTEXT("MeshesMissingUVs", "{MeshName} assigned to LOD {LodIndex} of Group {GroupIndex} on Groom Asset {AssetName} is missing UVs"), Args),
			HairMesh
		});
	}
}

void VerifyWardrobeItem(const UGroomBindingAsset* GroomBindingAsset, UMetaHumanAssetReport* Report)
{
	FString RootFolder = FPaths::GetPath(GroomBindingAsset->GetPathName());

	TArray<FAssetData> TopLevelItems;
	IAssetRegistry::GetChecked().GetAssetsByPath(FName(RootFolder), TopLevelItems);

	bool bWardrobeItemFound = false;

	for (const FAssetData& Item : TopLevelItems)
	{
		if (FPaths::GetBaseFilename(Item.PackageName.ToString()).StartsWith(TEXT("WI_")))
		{
			bWardrobeItemFound = true;
			UE::MetaHuman::FMetaHumanCharacterVerification::Get().VerifyGroomWardrobeItem(Item.GetAsset(), GroomBindingAsset, Report);
		}
	}

	// 3014 - Check for MetaHuman Wardrobe Item per asset (tbd)
	if (!bWardrobeItemFound)
	{
		Report->AddWarning({LOCTEXT("MissingWardrobeItem", "The package does not contain a Wardrobe Item. Certain features will not work or will be at default values")});
	}
}
} // namespace UE::MetaHuman::Private

void UVerifyMetaHumanGroom::Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const
{
	using namespace UE::MetaHuman::Private;

	if (!ensureAsRuntimeWarning(ToVerify) || !ensureAsRuntimeWarning(Report))
	{
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(ToVerify->GetName()));
	const UGroomBindingAsset* GroomBindingAsset = Cast<UGroomBindingAsset>(ToVerify);
	if (!GroomBindingAsset)
	{
		Report->AddError({
			FText::Format(LOCTEXT("ObjectNotAGroomBinding", "The UObject {AssetName} is not a GroomBinding"), Args),
			GroomBindingAsset
		});
		return;
	}


	const UGroomAsset* GroomAsset = GroomBindingAsset->GetGroom();

	// 3000 groom asset missing
	if (!GroomAsset)
	{
		Report->AddError({
			FText::Format(LOCTEXT("MissingGroom", "The Groom Binding {AssetName} does not have a valid Groom assigned"), Args),
			GroomBindingAsset
		});
		return;
	}

	// Basic validity test
	if (!GroomAsset->IsValid())
	{
		Report->AddError({
			FText::Format(LOCTEXT("GroomNotValid", "The Groom Asset {AssetName} is not a valid Groom"), Args),
			GroomAsset
		});
		return;
	}

	// Verify basic consistency
	if (!GroomAsset->AreGroupsValid())
	{
		Report->AddError({
			FText::Format(LOCTEXT("GroomGroupsNotValid", "The Groom Asset {AssetName} does not have valid Groups"), Args),
			GroomAsset
		});
	}

	// Check that Grooming mesh is present and correct
	FBox GroomMeshBounds;
	VerifyGroomingMesh(GroomBindingAsset, Args, Report, GroomMeshBounds);

	//Check that any WardrobeItems present are correct
	VerifyWardrobeItem(GroomBindingAsset, Report);

	// Checks for global properties that affect strands
	VerifyGlobalStrandsInfoValid(GroomAsset, Args, Report);

	// Check all the parts of the groom for validity against the various rules
	uint32 TotalGuideCount = 0;
	for (const FHairGroupInfoWithVisibility& GroupInfo : GroomAsset->GetHairGroupsInfo())
	{
		// Check per-group strands info
		VerifyGroupsStrandsInfoValid(GroomAsset, GroupInfo, Args, Report, GroomMeshBounds);
		TotalGuideCount += GroupInfo.NumGuides;
	}
	for (const FHairGroupsCardsSourceDescription& Cards : GroomAsset->GetHairGroupsCards())
	{
		// Check per-group cards info
		VerifyCardsInfoValid(GroomAsset, Cards, Args, Report);
	}
	for (const FHairGroupsMeshesSourceDescription& Meshes : GroomAsset->GetHairGroupsMeshes())
	{
		// Check per-group meshes (helmets) info
		VerifyMeshesInfoValid(GroomAsset, Meshes, Args, Report);
	}

	const TArray<FHairGroupsMaterial>& Materials = GroomAsset->GetHairGroupsMaterials();
	for (int MaterialIndex = 0; MaterialIndex < Materials.Num(); MaterialIndex++)
	{
		if (!Materials[MaterialIndex].Material)
		{
			Args.Add(TEXT("MaterialIndex"), MaterialIndex);
			// 3012 Groom asset missing material
			Report->AddWarning({
				FText::Format(LOCTEXT("MissingMaterial", "The Material {MaterialIndex} on Groom Asset {AssetName} has not got a material set"), Args),
				GroomAsset
			});
		}
	}

	// 3018 Too many guides (UEFN specific)
	if (TotalGuideCount > 2000)
	{
		Report->AddWarning({
			FText::Format(LOCTEXT("UEFNStrandCountExceeded", "Groom Asset {AssetName} has more than 2000 guide curves making it unsuitable for use in UEFN"), Args),
			GroomAsset
		});
	}

	// 3019 LODs incomplete (UEFN specific)
}

#undef LOCTEXT_NAMESPACE

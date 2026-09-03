// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteAssemblyDataBuilder.h"

#if WITH_EDITOR

#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "StaticMeshAttributes.h"
#include "NaniteDefinitions.h"
#include "EngineLogs.h"

static void AddMaterialRemap(TArray<int32>& RemapTable, int32 LocalMaterialIndex, int32 MaterialIndex)
{
	const int32 NumRemapElements = RemapTable.Num();
	if (LocalMaterialIndex >= NumRemapElements)
	{
		RemapTable.SetNumUninitialized(LocalMaterialIndex + 1);
		for (int32 i = NumRemapElements; i < LocalMaterialIndex; ++i)
		{
			RemapTable[i] = INDEX_NONE;
		}
	}
	RemapTable[LocalMaterialIndex] = MaterialIndex;
}

int32 FNaniteAssemblyDataBuilder::AddPart(const FSoftObjectPath& StaticMeshPath)
{
	int32 NewPartIndex = AssemblyData.Parts.Num();
	FNaniteAssemblyPart& Part = AssemblyData.Parts.Emplace_GetRef();
	Part.MeshObjectPath = StaticMeshPath;

	return NewPartIndex;
}

int32 FNaniteAssemblyDataBuilder::FindPart(const FSoftObjectPath& StaticMeshPath)
{
	return AssemblyData.Parts.IndexOfByPredicate(
		[&StaticMeshPath](const FNaniteAssemblyPart& Part)
		{
			return Part.MeshObjectPath == StaticMeshPath;
		}
	);
}

int32 FNaniteAssemblyDataBuilder::FindOrAddPart(const FSoftObjectPath& StaticMeshPath)
{
	int32 PartIndex = FindPart(StaticMeshPath);
	if (PartIndex == INDEX_NONE)
	{
		PartIndex = AddPart(StaticMeshPath);
	}	
	return PartIndex;
}

int32 FNaniteAssemblyDataBuilder::AddNode(int32 PartIndex, const FMatrix44f& LocalTransform, int32 ParentNodeIndex)
{
	check(AssemblyData.Parts.IsValidIndex(PartIndex));
	check(ParentNodeIndex == INDEX_NONE || AssemblyData.Nodes.IsValidIndex(ParentNodeIndex));

	int32 NewNodeIndex = AssemblyData.Nodes.Num();
	FNaniteAssemblyNode& Node = AssemblyData.Nodes.Emplace_GetRef();
	Node.ParentIndex = ParentNodeIndex;
	Node.PartIndex = PartIndex;
	Node.Transform = LocalTransform;

	return NewNodeIndex;
}

void FNaniteAssemblyDataBuilder::SetNumMaterials(int32 NumMaterials)
{
	Materials.SetNum(NumMaterials);

	// Invalidate any remappings that may now be invalid
	for (FNaniteAssemblyPart& Part : AssemblyData.Parts)
	{
		for (int32& MaterialIndex : Part.MaterialRemap)
		{
			if (MaterialIndex >= NumMaterials)
			{
				MaterialIndex = INDEX_NONE;
			}
		}
	}

	for (int32& MaterialIndex : BaseMeshMaterialRemap)
	{
		if (MaterialIndex >= NumMaterials)
		{
			MaterialIndex = INDEX_NONE;
		}
	}
}

void FNaniteAssemblyDataBuilder::SetMaterial(int32 MaterialIndex, const FStaticMaterial& StaticMaterial)
{
	Materials[MaterialIndex] = StaticMaterial;
}

void FNaniteAssemblyDataBuilder::RemapPartMaterial(int32 PartIndex, int32 LocalMaterialIndex, int32 MaterialIndex)
{
	check(AssemblyData.Parts.IsValidIndex(PartIndex));
	check(LocalMaterialIndex >= 0);
	check(Materials.IsValidIndex(MaterialIndex));

	AddMaterialRemap(AssemblyData.Parts[PartIndex].MaterialRemap, LocalMaterialIndex, MaterialIndex);
}

void FNaniteAssemblyDataBuilder::RemapBaseMeshMaterial(int32 LocalMaterialIndex, int32 MaterialIndex)
{
	check(LocalMaterialIndex >= 0);
	check(Materials.IsValidIndex(MaterialIndex));

	AddMaterialRemap(BaseMeshMaterialRemap, LocalMaterialIndex, MaterialIndex);
}

void FNaniteAssemblyDataBuilder::ApplyToStaticMesh(UStaticMesh& TargetMesh, bool bFinalizeMesh)
{
#if NANITE_ASSEMBLY_DATA
	TargetMesh.NaniteSettings.NaniteAssemblyData = AssemblyData;
	
	TArray<FStaticMaterial>& StaticMaterials = TargetMesh.GetStaticMaterials();
	StaticMaterials = Materials;
	if (StaticMaterials.IsEmpty())
	{
		// Ensure we have at least one material slot in the end
		StaticMaterials.Emplace();
	}

	// Check to remap the base mesh's sections and validate their material index against the new material list
	for (TPair<uint32, FMeshSectionInfo>& Entry : TargetMesh.GetSectionInfoMap().Map)
	{
		if (!BaseMeshMaterialRemap.IsEmpty()) // Empty means don't remap
		{
			Entry.Value.MaterialIndex = BaseMeshMaterialRemap.IsValidIndex(Entry.Value.MaterialIndex) ?
				BaseMeshMaterialRemap[Entry.Value.MaterialIndex] : 0;
		}

		if (!StaticMaterials.IsValidIndex(Entry.Value.MaterialIndex))
		{
			// Ensure valid material index
			Entry.Value.MaterialIndex = 0;
		}
	}

	if (bFinalizeMesh && AssemblyData.IsValid() && !TargetMesh.IsMeshDescriptionValid(0))
	{
		// "Legitamize" the mesh if it has no source models or valid LOD0 mesh description by adding a single degenerate triangle.
		// This is necessary to prevent various failures that can occur from not having any source models or valid mesh descriptions,
		// and ensures the user can edit LOD0 build settings.
		if (TargetMesh.GetNumSourceModels() == 0)
		{
			TargetMesh.SetNumSourceModels(1);
		}

		FMeshDescription& MeshDescription = *TargetMesh.GetSourceModel(0).CreateMeshDescription();
		FStaticMeshAttributes Attributes(MeshDescription);
		Attributes.Register();

		MeshDescription.ReserveNewVertices(3);
		MeshDescription.ReserveNewVertexInstances(3);
		MeshDescription.ReserveNewTriangles(1);
		MeshDescription.ReserveNewPolygonGroups(1);

		const FPolygonGroupID PolyGroup = MeshDescription.CreatePolygonGroup();
		const FVertexID Verts[] =
		{
			MeshDescription.CreateVertex(),
			MeshDescription.CreateVertex(),
			MeshDescription.CreateVertex()
		};
		const FVertexInstanceID VertInstances[] =
		{
			MeshDescription.CreateVertexInstance(Verts[0]),
			MeshDescription.CreateVertexInstance(Verts[1]),
			MeshDescription.CreateVertexInstance(Verts[2])
		};
		const FTriangleID Tri = MeshDescription.CreateTriangle(PolyGroup, MakeArrayView(VertInstances, 3));

		TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
		VertexPositions.Set(Verts[0], FVector3f::Zero());
		VertexPositions.Set(Verts[1], FVector3f::Zero());
		VertexPositions.Set(Verts[2], FVector3f::Zero());

		// Commit the mesh description
		UStaticMesh::FCommitMeshDescriptionParams CommitParams;
		CommitParams.bMarkPackageDirty = false;
		CommitParams.bUseHashAsGuid = true;
		TargetMesh.CommitMeshDescription(0, CommitParams);
	}
#else
	UE_LOG(
		LogStaticMesh, Warning,
		TEXT("Attempt to build assembly for StaticMesh '%s' has no effect because NANITE_ASSEMBLY_DATA=0"),
		*TargetMesh.GetFullName()
	);
#endif // NANITE_ASSEMBLY_DATA
}

#endif // WITH_EDITOR
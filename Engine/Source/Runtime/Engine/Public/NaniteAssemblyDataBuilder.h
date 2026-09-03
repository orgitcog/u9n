// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Engine/NaniteAssemblyData.h"
#include "Engine/StaticMesh.h"	// Fix for non-unity build

struct FStaticMaterial;

/** Helper class for building a Nanite assembly */
class FNaniteAssemblyDataBuilder
{
public:
	friend FArchive& operator<<(FArchive& Ar, FNaniteAssemblyDataBuilder& Builder);

	FNaniteAssemblyDataBuilder() {}
	
	const FNaniteAssemblyData& GetData() const { return AssemblyData; }

	ENGINE_API int32 AddPart(const FSoftObjectPath& StaticMeshPath);
	ENGINE_API int32 FindPart(const FSoftObjectPath& StaticMeshPath);
	ENGINE_API int32 FindOrAddPart(const FSoftObjectPath& StaticMeshPath);
	
	ENGINE_API int32 AddNode(
		int32 PartIndex,
		const FMatrix44f& LocalTransform = FMatrix44f::Identity,
		int32 ParentNodeIndex = INDEX_NONE
	);

	const TArray<FStaticMaterial>& GetMaterials() const { return Materials; };
	int32 AddMaterial(const FStaticMaterial& StaticMaterial) { return Materials.Add(StaticMaterial); }
	ENGINE_API void SetNumMaterials(int32 NumMaterials);
	ENGINE_API void SetMaterial(int32 MaterialIndex, const FStaticMaterial& StaticMaterial);
		
	ENGINE_API void RemapPartMaterial(int32 PartIndex, int32 LocalMaterialIndex, int32 MaterialIndex);
	ENGINE_API void RemapBaseMeshMaterial(int32 LocalMaterialIndex, int32 MaterialIndex);

	ENGINE_API void ApplyToStaticMesh(UStaticMesh& TargetMesh, bool bFinalizeMesh = true);

private:
	FNaniteAssemblyData AssemblyData;
	TArray<FStaticMaterial> Materials;
	TArray<int32> BaseMeshMaterialRemap;
};

inline FArchive& operator<<(FArchive& Ar, FNaniteAssemblyDataBuilder& Builder)
{
	Ar << Builder.AssemblyData;
	Ar << Builder.Materials;
	Ar << Builder.BaseMeshMaterialRemap;

	return Ar;
}

#endif // WITH_EDITOR

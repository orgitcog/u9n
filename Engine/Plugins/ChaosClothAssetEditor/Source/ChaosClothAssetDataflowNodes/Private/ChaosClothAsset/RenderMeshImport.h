// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"

struct FManagedArrayCollection;
struct FMeshBuildSettings;
struct FMeshDescription;
struct FStaticMaterial;

namespace UE::Chaos::ClothAsset
{
	/**
	 * Render mesh import from mesh description.
	 */
	struct FRenderMeshImport
	{
		struct FVertex
		{
			FVector3f RenderPosition;
			FVector3f RenderNormal;
			FVector3f RenderTangentU;
			FVector3f RenderTangentV;
			TArray<FVector2f> RenderUVs;
			FLinearColor RenderColor;
			int32 OriginalIndex;
		};

		struct FTriangle
		{
			FIntVector3 VertexIndices;
			int32 OriginalIndex;
			int32 MaterialIndex;
		};

		struct FSection
		{
			TArray<FVertex> Vertices;
			TArray<FTriangle> Triangles;
			int32 NumTexCoords;
		};
		TSortedMap<int32, FSection> Sections;

		FRenderMeshImport(const FMeshDescription& InMeshDescription, const FMeshBuildSettings& BuildSettings);

		void AddRenderSections(
			const TSharedRef<FManagedArrayCollection> ClothCollection,
			const TArray<FStaticMaterial>& Materials,
			const FName OriginalTrianglesName,
			const FName OriginalVerticesName);
	};
}  // End namespace UE::Chaos::ClothAsset


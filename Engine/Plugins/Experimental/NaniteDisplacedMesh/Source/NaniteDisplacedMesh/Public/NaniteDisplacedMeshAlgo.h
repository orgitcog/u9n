// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Containers/Array.h"
#include "Math/Bounds.h"

struct FMeshBuildVertexData;
struct FNaniteDisplacedMeshParams;

namespace EDisplaceNaniteMeshOptions
{
	enum Type : int32
	{
		/** No custom displacement behavior. */
		None = 0,

		/** Will ignore any displacement when the displacement map's UVs are outside the range [0, 1]. */
		IgnoreNonNormalizedDisplacementUVs = 1 << 0,
	};
}

NANITEDISPLACEDMESH_API bool DisplaceNaniteMesh(
	const FNaniteDisplacedMeshParams& Parameters,
	const uint32 NumTextureCoord,
	FMeshBuildVertexData& Verts,
	TArray< uint32 >& Indexes,
	TArray< int32 >& MaterialIndexes,
	FBounds3f& VertexBounds,
	EDisplaceNaniteMeshOptions::Type Options = EDisplaceNaniteMeshOptions::None
);

#endif

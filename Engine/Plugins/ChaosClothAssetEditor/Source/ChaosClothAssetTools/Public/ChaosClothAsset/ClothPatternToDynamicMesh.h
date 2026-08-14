// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothPatternVertexType.h"
#include "Templates/SharedPointer.h"


namespace UE::Geometry { class FDynamicMesh3; }
class UChaosClothAsset;
struct FManagedArrayCollection;

namespace UE::Chaos::ClothAsset
{

/**
* Convert a single pattern from a ClothAsset to a FDynamicMesh3. 
* When PatternIndex = INDEX_NONE, convert the entire cloth asset to an FDynamicMesh3.
* When VertexDataType = EClothPatternVertexType::Sim3D, this generates a welded mesh (with native welded indexing) when PatternIndex = INDEX_NONE,
*	and an unwelded (with native 2D unwelded indexing) when PatternIndex is a valid SimPattern Index.
* Use MaterialOffset to specify an offset into the material group ID, and run a Fix up on any triangles without valid material IDs.
* When MaterialOffset is INDEX_NONE, no offset is applied and nbo fix up is run. 
*/
class CHAOSCLOTHASSETTOOLS_API FClothPatternToDynamicMesh
{
public:

	void Convert(
		const TSharedRef<const FManagedArrayCollection> ClothCollection,
		int32 PatternIndex,
		EClothPatternVertexType VertexDataType,
		UE::Geometry::FDynamicMesh3& MeshOut,
		bool bDisableAttributes = false,
		int32 MaterialOffset = INDEX_NONE);

	void Convert(
		const UChaosClothAsset* ClothAssetMeshIn,
		int32 LODIndex,
		int32 PatternIndex,
		EClothPatternVertexType VertexDataType,
		UE::Geometry::FDynamicMesh3& MeshOut,
		int32 MaterialOffset = INDEX_NONE);
};

}	// namespace UE::Chaos::ClothAsset


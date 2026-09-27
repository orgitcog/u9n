// Copyright Epic Games, Inc. All Rights Reserved. 

#pragma once

#include "MeshDescription.h"
#include "StaticMeshOperations.h"

#include "InterchangeFactoryBase.h"

class UBodySetup;
class UInterchangeBaseNodeContainer;
class UInterchangeSceneNode;
class UInterchangeStaticMeshFactoryNode;
class UInterchangeStaticMeshLodDataNode;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
struct FKAggregateGeom;
struct FSkeletalMaterial;
struct FStaticMaterial;
struct FInterchangeMeshPayLoadKey;

namespace UE::Interchange
{
	struct FMeshPayloadData;
	struct FMeshPayload;
}

namespace UE::Interchange::Private::MeshHelper
{
	/** These are largely copied from StaticMeshImportUtils.h */

	INTERCHANGEIMPORT_API bool AddConvexGeomFromVertices(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const FMeshDescription& MeshDescription,
		FKAggregateGeom& AggGeom
	);

	INTERCHANGEIMPORT_API bool DecomposeConvexMesh(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const FMeshDescription& MeshDescription,
		UBodySetup* BodySetup
	);

	INTERCHANGEIMPORT_API bool AddBoxGeomFromTris(
		const FMeshDescription& MeshDescription, 
		FKAggregateGeom& AggGeom,
		bool bForcePrimitiveGeneration = true
	);

	INTERCHANGEIMPORT_API bool AddSphereGeomFromVertices(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const FMeshDescription& MeshDescription,
		FKAggregateGeom& AggGeom,
		bool bForcePrimitiveGeneration = true
	);

	INTERCHANGEIMPORT_API bool AddCapsuleGeomFromVertices(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const FMeshDescription& MeshDescription,
		FKAggregateGeom& AggGeom
	);

	INTERCHANGEIMPORT_API bool ImportBoxCollision(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const TMap<FInterchangeMeshPayLoadKey, FMeshPayload>& BoxCollisionPayloads,
		UStaticMesh* StaticMesh,
		bool bForcePrimitiveGeneration = true
	);

	INTERCHANGEIMPORT_API bool ImportCapsuleCollision(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const TMap<FInterchangeMeshPayLoadKey, FMeshPayload>& CapsuleCollisionPayloads,
		UStaticMesh* StaticMesh
	);

	INTERCHANGEIMPORT_API bool ImportSphereCollision(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const TMap<FInterchangeMeshPayLoadKey, FMeshPayload>& SphereCollisionPayloads, 
		UStaticMesh* StaticMesh,
		bool bForcePrimitiveGeneration = true
	);

	INTERCHANGEIMPORT_API bool ImportConvexCollision(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		const TMap<FInterchangeMeshPayLoadKey, FMeshPayload>& ConvexCollisionPayloads,
		UStaticMesh* StaticMesh,
		const UInterchangeStaticMeshLodDataNode* LodDataNode
	);

	INTERCHANGEIMPORT_API bool ImportSockets(
		const UInterchangeFactoryBase::FImportAssetObjectParams& Arguments,
		UStaticMesh* StaticMesh,
		const UInterchangeStaticMeshFactoryNode* FactoryNode
	);

	INTERCHANGEIMPORT_API void RemapPolygonGroups(
		const FMeshDescription& SourceMesh,
		FMeshDescription& TargetMesh,
		PolygonGroupMap& RemapPolygonGroup
	);

	/* return the result of the global transform with the geometric and pivot transform of the scene node. */
	INTERCHANGEIMPORT_API void AddSceneNodeGeometricAndPivotToGlobalTransform(FTransform& GlobalTransform, const UInterchangeSceneNode* SceneNode, const bool bBakeMeshes, const bool bBakePivotMeshes);

	INTERCHANGEIMPORT_API void SkeletalMeshFactorySetupAssetMaterialArray(TArray<FSkeletalMaterial>& ExistMaterials
		, TMap<FString, FString> ImportedSlotMaterialDependencies
		, const UInterchangeBaseNodeContainer* NodeContainer
		, const bool bIsReimport);

	INTERCHANGEIMPORT_API void StaticMeshFactorySetupAssetMaterialArray(TArray<FStaticMaterial>& ExistMaterials
		, TMap<FString, FString> ImportedSlotMaterialDependencies
		, const UInterchangeBaseNodeContainer* NodeContainer
		, const bool bIsReimport);

	INTERCHANGEIMPORT_API void GeometryCacheFactorySetupAssetMaterialArray(TArray<TObjectPtr<UMaterialInterface>>& ExistMaterials
		, TArray<FName>& MaterialSlotNames
		, TMap<FString, FString> ImportedSlotMaterialDependencies
		, const UInterchangeBaseNodeContainer* NodeContainer
		, const bool bIsReimport);

	INTERCHANGEIMPORT_API void CopyMorphTargetsMeshDescriptionToSkeletalMeshDescription(TArray<FString>& SkeletonMorphCurveMetadataNames
		, const TMap<FString, TOptional<UE::Interchange::FMeshPayloadData>>& LodMorphTargetMeshDescriptions
		, FMeshDescription& DestinationMeshDescription
		, const bool bMergeMorphTargetWithSameName);

#if WITH_EDITOR
	INTERCHANGEIMPORT_API void RemapSkeletalMeshVertexColorToMeshDescription(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, FMeshDescription& MeshDescription);
#endif

} //ns UE::Interchange::Private::MeshHelper

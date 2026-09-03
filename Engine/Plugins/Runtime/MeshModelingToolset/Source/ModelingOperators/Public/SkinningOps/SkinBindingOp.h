// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModelingOperators.h"
#include "ReferenceSkeleton.h"
#include "SkeletalMeshAttributes.h"

#define UE_API MODELINGOPERATORS_API


struct FReferenceSkeleton;

namespace UE::Geometry
{

template<typename ParentType> class TDynamicVertexSkinWeightsAttribute;
class FDynamicMesh3;
using FDynamicMeshVertexSkinWeightsAttribute = TDynamicVertexSkinWeightsAttribute<FDynamicMesh3>;


enum class ESkinBindingType : uint8
{
	// Computes the binding strength by computing the Euclidean distance to the closest set of bones,
	// where the strength of binding is proportional to the inverse distance. May cause bones to affects
	// parts of geometry that, although close in space, may be topologically distant.
	DirectDistance = 0,

	// Computes the binding by computing the geodesic distance from each set of bones. This is slower than the
	// direct distance.
	GeodesicVoxel = 1,
};


class FSkinBindingOp : public FDynamicMeshOperator
{
public:
	virtual ~FSkinBindingOp() override {}
	
	// The transform hierarchy to bind to. Listed in the same order as the bones in the
	// reference skeleton that this skeletal mesh is tied to.
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	TArray<TPair<FTransform, FMeshBoneInfo>> TransformHierarchy;

	FName ProfileName = FSkeletalMeshAttributes::DefaultSkinWeightProfileName;
	
	ESkinBindingType BindType = ESkinBindingType::DirectDistance;
	float Stiffness = 0.2f;
	int32 MaxInfluences = 5;
	int32 VoxelResolution = 256;

	UE_API void SetTransformHierarchyFromReferenceSkeleton(const FReferenceSkeleton& InRefSkeleton);
	
	UE_API virtual void CalculateResult(FProgressCancel* InProgress) override;

private:
	static UE_API FDynamicMeshVertexSkinWeightsAttribute* GetOrCreateSkinWeightsAttribute(
		FDynamicMesh3& InMesh,
		FName InProfileName
		);


	UE_API void CreateSkinWeights_DirectDistance(
		FDynamicMesh3& InMesh,
		float InStiffness,
		const AnimationCore::FBoneWeightsSettings& InSettings
		) const;


	UE_API void CreateSkinWeights_GeodesicVoxel(
		FDynamicMesh3& InMesh,
		float InStiffness,
		const AnimationCore::FBoneWeightsSettings& InSettings
		) const; 
};


} // namespace UE::Geometry

#undef UE_API

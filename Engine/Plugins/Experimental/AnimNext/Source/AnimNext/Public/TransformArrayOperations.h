// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TransformArrayView.h"
#include "BoneIndices.h"

namespace UE::AnimNext
{
	// Sets the identity (regular or additive) over the whole destination array
	ANIMNEXT_API void SetIdentity(const FTransformArrayAoSView& Dest, bool bIsAdditive);
	ANIMNEXT_API void SetIdentity(const FTransformArrayAoSView& Dest, bool bIsAdditive, int32 StartIndex, int32 NumTransformsToSet);
	ANIMNEXT_API void SetIdentity(const FTransformArraySoAView& Dest, bool bIsAdditive);
	ANIMNEXT_API void SetIdentity(const FTransformArraySoAView& Dest, bool bIsAdditive, int32 StartIndex, int32 NumTransformsToSet);

	// Copies the specified number of transforms from a source into a destination starting at the specified start index
	// If NumToCopy is -1, we copy until the end
	ANIMNEXT_API void CopyTransforms(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, int32 StartIndex = 0, int32 NumToCopy = -1);
	ANIMNEXT_API void CopyTransforms(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, int32 StartIndex = 0, int32 NumToCopy = -1);

	// Normalizes rotations in input transform view
	ANIMNEXT_API void NormalizeRotations(const FTransformArrayAoSView& Input);
	ANIMNEXT_API void NormalizeRotations(const FTransformArraySoAView& Input);

	// Convert a pose from local space to mesh space (rotations only)
	ANIMNEXT_API void ConvertPoseLocalToMeshRotation(const FTransformArrayAoSView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);
	ANIMNEXT_API void ConvertPoseLocalToMeshRotation(const FTransformArraySoAView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);

	// Convert a pose from mesh space to local space (rotations only)
	ANIMNEXT_API void ConvertPoseMeshToLocalRotation(const FTransformArrayAoSView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);
	ANIMNEXT_API void ConvertPoseMeshToLocalRotation(const FTransformArraySoAView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);
	
	// Convert a pose from local space to mesh space (rotation and translation only)
	ANIMNEXT_API void ConvertPoseLocalToMeshRotationTranslation(const FTransformArrayAoSView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);
	ANIMNEXT_API void ConvertPoseLocalToMeshRotationTranslation(const FTransformArraySoAView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);

	// Convert a pose from mesh space to local space (rotation and translation only)
	ANIMNEXT_API void ConvertPoseMeshToLocalRotationTranslation(const FTransformArrayAoSView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);
	ANIMNEXT_API void ConvertPoseMeshToLocalRotationTranslation(const FTransformArraySoAView& Input, const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap);

	// The additive transform view is blended with the additive identity using the provided blend weight
	// We then accumulate the resulting transforms on top of the base transforms
	// Delta = Blend(Identity, Additive, BlendWeight)
	// Base.Accumulate(Delta);
	ANIMNEXT_API void BlendWithIdentityAndAccumulate(const FTransformArrayAoSView& Base, const FTransformArrayAoSConstView& Additive, const float BlendWeight);
	ANIMNEXT_API void BlendWithIdentityAndAccumulate(const FTransformArraySoAView& Base, const FTransformArraySoAConstView& Additive, const float BlendWeight);

	// Performs a BlendWithIdentityAndAccumulate in mesh space, result is in local space
	ANIMNEXT_API void BlendWithIdentityAndAccumulateMesh(
		const FTransformArrayAoSView& Base, const FTransformArrayAoSConstView& Additive, 
		const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap, const float BlendWeight);
	ANIMNEXT_API void BlendWithIdentityAndAccumulateMesh(
		const FTransformArraySoAView& Base, const FTransformArraySoAConstView& Additive, 
		const TArrayView<const FBoneIndexType>& LODBoneIndexToParentLODBoneIndexMap, const float BlendWeight);

	// The source transforms are scaled by the provided weight and the result is written in the destination
	// Dest = Source * ScaleWeight
	ANIMNEXT_API void BlendOverwriteWithScale(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, const float ScaleWeight);
	ANIMNEXT_API void BlendOverwriteWithScale(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, const float ScaleWeight);

	// The source transforms are scaled by the provided weight and the result is added to the destination
	// Dest = Dest + (Source * ScaleWeight)
	ANIMNEXT_API void BlendAddWithScale(const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source, const float ScaleWeight);
	ANIMNEXT_API void BlendAddWithScale(const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source, const float ScaleWeight);

	// The source transforms are scaled by the provided per bone weight and the result is written in the destination
	// If bInvert is set, the bone weights are set to (1 - weight), DefaultScaleWeight remains unchanged, i.e. it is not inverted
	// Dest = Source * (WeightIndex != INDEX_NONE ? Weights[WeightIndex] : DefaultScaleWeight)
	ANIMNEXT_API void BlendOverwritePerBoneWithScale(
		const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight, const bool bInvert = false);
	ANIMNEXT_API void BlendOverwritePerBoneWithScale(
		const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight, const bool bInvert = false);

	// The source transforms are scaled by the provided per bone weight and the result is added to the destination
	// Dest = Dest + (Source * (WeightIndex != INDEX_NONE ? Weights[WeightIndex] : DefaultScaleWeight))
	ANIMNEXT_API void BlendAddPerBoneWithScale(
		const FTransformArrayAoSView& Dest, const FTransformArrayAoSConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight);
	ANIMNEXT_API void BlendAddPerBoneWithScale(
		const FTransformArraySoAView& Dest, const FTransformArraySoAConstView& Source,
		const TArrayView<const int32>& LODBoneIndexToWeightIndexMap, const TArrayView<const float>& BoneWeights, const float DefaultScaleWeight);
}

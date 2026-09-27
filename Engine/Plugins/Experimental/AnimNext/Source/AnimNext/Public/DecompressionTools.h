// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReferencePose.h"
#include "LODPose.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesRuntime.h"

#define UE_API ANIMNEXT_API

struct FCompactPose;
struct FCompressedAnimSequence;
struct FAnimExtractContext;
struct FAnimSequenceDecompressionContext;
struct FRootMotionReset;
class UAnimSequence;

namespace UE::AnimNext
{

class FDecompressionTools
{
public:
	// Returns whether decompression should use raw data or not
	static UE_API bool ShouldUseRawData(const UAnimSequence* AnimSequence, const FLODPose& AnimationPoseData);

	// Extracted from UAnimSequence
	// Extracts Animation Data from the provided AnimSequence, using AnimExtractContext extraction parameters
	static UE_API void GetAnimationPose(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FLODPose& OutAnimationPoseData, bool bForceUseRawData = false);

	/**
	* Get Bone Transform of the Time given, relative to Parent for all RequiredBones
	* This returns different transform based on additive or not. Or what kind of additive.
	*/
	static UE_API void GetBonePose(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FLODPose& OutAnimationPoseData, bool bForceUseRawData = false);

	static UE_API void GetBonePose_Additive(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FLODPose& OutAnimationPoseData);
	static UE_API void GetBonePose_AdditiveMeshRotationOnly(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FLODPose& OutAnimationPoseData);

	static UE_API void GetAnimationCurves(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, FBlendedCurve& OutCurves, bool bForceUseRawData = false);
	static UE_API void GetAnimationAttributes(const UAnimSequence* AnimSequence, const FAnimExtractContext& ExtractionContext, const FReferencePose& RefPose, UE::Anim::FStackAttributeContainer& OutAttributes, bool bForceUseRawData = false);

	// Decompress and retarget animation data using provided RetargetTransforms
	static UE_API void DecompressPose(FLODPose& OutAnimationPoseData,
		const FCompressedAnimSequence& CompressedData,
		const FAnimExtractContext& ExtractionContext,
		FAnimSequenceDecompressionContext& DecompressionContext,
		const TArray<FTransform>& RetargetTransforms,
		const FRootMotionReset& RootMotionReset);

	// Decompress and retarget animation data
	static UE_API void DecompressPose(FLODPose& OutAnimationPoseData,
		const FCompressedAnimSequence& CompressedData,
		const FAnimExtractContext& ExtractionContext,
		FAnimSequenceDecompressionContext& DecompressionContext,
		FName RetargetSource,
		const FRootMotionReset& RootMotionReset);

	static UE_API void RetargetBoneTransform(const FReferencePose& ReferencePose
		, const USkeleton* SourceSkeleton
		, const USkeleton* TargetSkeleton
		, const FName& RetargetSource
		, FTransform& BoneTransform
		, const int32 SkeletonBoneIndex
		, const int32 LODBoneIndex
		, const bool bIsBakedAdditive
		, const bool bDisableRetargeting);

	static UE_API void RetargetBoneTransform(const FReferencePose& ReferencePose
		, const USkeleton* SourceSkeleton
		, const USkeleton* TargetSkeleton
		, const FName& SourceName
		, const TArray<FTransform>& RetargetTransforms
		, FTransform& BoneTransform
		, const int32 SkeletonBoneIndex
		, const int32 LODBoneIndex
		, const bool bIsBakedAdditive
		, const bool bDisableRetargeting);

	static UE_API bool UseRawDataForPoseExtraction(const UAnimSequence* AnimSequence, const FLODPose& AnimationPoseData);
};

} // end namespace UE::AnimNext

#undef UE_API

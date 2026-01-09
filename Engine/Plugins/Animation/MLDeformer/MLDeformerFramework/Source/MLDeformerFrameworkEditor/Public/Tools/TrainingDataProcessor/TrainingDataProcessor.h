// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerTrainingDataProcessorSettings.h"

struct FReferenceSkeleton;
class USkeleton;

namespace UE::MLDeformer::TrainingDataProcessor
{
	/**
	 * The training data processor algorithm, which is executed when you press the Generate button.
	 * This will take a set of animation sequences as input, then find the 'best' N number of frames.
	 * After that it can remix the poses based on a set of bone groups. Basically the keyframes of the
	 * bones inside the groups get shuffled. So all the bones in the bone group will change together.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FTrainingDataProcessor final
	{
	public:
		/**
		 * Run the algorithm using specific settings.
		 * The output of this process is a UAnimSequence that is specified inside the Settings.
		 * @param Settings The settings to use during execution. This could come from the UMLDeformerModel::GetTrainingDataProcessorSettings() for example.
		 * @param Skeleton The skeleton to use when sampling animations and apply the transforms on in the output animation.
		 * @return Returns true if successful, or false if not. Errors will be logged in case false is returned. 
		 */
		bool Execute(const UMLDeformerTrainingDataProcessorSettings& Settings, const USkeleton* Skeleton);

	private:
		void Clear();
		bool SampleFrames(const UMLDeformerTrainingDataProcessorSettings& Settings, const USkeleton& Skeleton);
		TArray<int32> FindBestFrameIndices(int32 MaxNumFrames, const FReferenceSkeleton& RefSkeleton) const;
		bool RemixPoses(const UMLDeformerTrainingDataProcessorSettings& Settings, const FReferenceSkeleton& RefSkeleton);
		bool SaveAnimationDataInAnimSequence(const UMLDeformerTrainingDataProcessorSettings& Settings, const FReferenceSkeleton& RefSkeleton, const TArray<int32>& FramesToInclude);
		int32 GetNumFrames() const;
		TArrayView<FTransform3f> GetFrameTransforms(int32 FrameIndex);
		TConstArrayView<FTransform3f> GetFrameTransforms(int32 FrameIndex) const;
		TConstArrayView<FTransform3f> GetFrameTransforms(const TArray<FTransform3f>& Transforms, int32 FrameIndex) const;
		TConstArrayView<FTransform3f> GetRefPoseTransforms() const;
		double CalculateMeanError(TConstArrayView<FTransform3f> PoseA, TConstArrayView<FTransform3f> PoseB) const;
		double CalculateMeanError(int32 PreviousPoseIndex, int32 CurrentPoseIndex) const;
		static FTransform3f GetRefPoseTransform(const FReferenceSkeleton& RefSkeleton, FName BoneName);
		
	private:
		/**
		 * The animation data represented as buffers of vectors and quaternions.
		 * The size of the arrays are the number of bones in the reference skeleton, multiplied by the number of frames.
		 * So the layout for a two bone skeleton would be like this:
		 * [(Bone1, Bone2), (Bone1, Bone2), (Bone1, Bone2), ...] where each (Bone1, Bone2) represents a single frame.
		 * In case of positions and scales, the value for Bone1 and Bone2 would be a FVector.
		 * For the rotations it is an FQuat.
		 */
		struct FAnimFrameData
		{
			// NumRefSkelBones * GetNumFrames().
			TArray<FTransform3f> Transforms;

			// NumRefSkelBones FTransforms.			
			TArray<FTransform3f> RefPoseTransforms;

			// Bone names in the bones list, that actually exist.
			TArray<FName> BoneNames;

			// For each entry in the BoneNames array, this contains an index into the FReferenceSkeleton.
			TArray<int32> BoneIndices;

			// Number of bones in the reference skeleton.
			int32 NumRefSkelBones = 0;
		};

		/** The animation data as flat float arrays. @see FAnimFrameData for more information. */
		FAnimFrameData AnimFrameData;
	};
} // namespace UE::MLDeformer::TrainingDataProcessor

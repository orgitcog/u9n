// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "SupressWarnings.h"

MH_DISABLE_EIGEN_WARNINGS
#include "Eigen/Dense"
MH_ENABLE_WARNINGS

namespace UE::MetaHuman::Pipeline
{
	class METAHUMANPIPELINECORE_API FHyprsenseUtils
	{
	protected:

		using Matrix23f = Eigen::Matrix<float, 2, 3>;
		using Matrix32f = Eigen::Matrix<float, 3, 2>;
		using Matrix33f = Eigen::Matrix<float, 3, 3>;

		struct Bbox
		{
			float X1, Y1;
			float X2, Y2;
			float Score;
			float Area;
		};

		enum class PartType : uint8
		{
			FaceDetector,
			SparseTracker,
			PartwiseTracker
		};

		uint32 DetectorInputSizeX = 300;
		uint32 DetectorInputSizeY = 300;

		Matrix23f GetTransformFromBbox(const Bbox& InBbox, int32 InImageWidth, int32 InImageHeight, int32 InCropBoxSize, float InRotation, bool bInFlip, PartType InPartType) const;

		TArray<float> WarpAffineBilinear(const uint8* InSrcImage, int32 InSrcWidth, int32 InSrcHeight, const Matrix23f& InTransform, int32 InTargetWidth, int32 InTargetHeight, bool bInIsDetector) const;

		TArray<Bbox> HardNMS(const TArray<float>& InScores, const TArray<float>& InBoxes, float InIouThreshold, float InProbThreshold, int32 InTotalSize, int32 InTopK = -1) const;
		float IOU(const Bbox& InBox1, const Bbox& InBox2) const;
	};
};
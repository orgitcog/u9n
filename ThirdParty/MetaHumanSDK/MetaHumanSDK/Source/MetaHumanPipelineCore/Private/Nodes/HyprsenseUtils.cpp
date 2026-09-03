// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/HyprsenseUtils.h"
#include "MetaHumanTrace.h"
#include "Async/ParallelFor.h"

namespace UE::MetaHuman::Pipeline
{

	FHyprsenseUtils::Matrix23f FHyprsenseUtils::GetTransformFromBbox(const Bbox& InBbox, int32 InImageWidth, int32 InImageHeight, int32 InCropBoxSize, float InRotation, bool bInFlip, PartType InPartType) const
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseUtils::GetTransformFromBbox);

		const int32 X = InBbox.X1 * InImageWidth;
		const int32 Y = InBbox.Y1 * InImageHeight;
		const int32 W = (InBbox.X2 - InBbox.X1) * InImageWidth;
		const int32 H = (InBbox.Y2 - InBbox.Y1) * InImageHeight;

		const float Cx = X + 0.5 * W;
		const float Cy = Y + 0.5 * H;

		float SizeX = 0;
		float SizeY = 0;

		if (InPartType == PartType::FaceDetector)
		{
			SizeX = W;
			SizeY = H;
		}
		else if (InPartType == PartType::SparseTracker)
		{
			SizeX = FMath::Sqrt(float(W * W + H * H)) * 256.f / 192.f;
			SizeY = SizeX;
		}
		else if (InPartType == PartType::PartwiseTracker)
		{
			SizeX = (W > H ? W : H);
			SizeY = SizeX;
		}

		// affine a face box on an image to an input frame
		Matrix33f TransformFlip;
		{
			if (bInFlip)
			{
				TransformFlip << -1, 0, InCropBoxSize,
					0, 1, 0,
					0, 0, 1;
			}
			else
			{
				TransformFlip << 1, 0, 0,
					0, 1, 0,
					0, 0, 1;
			}
		}

		// affine a face box on an image to an input frame
		Eigen::Matrix2f Rot;
		Rot << std::cos(InRotation), -std::sin(InRotation), std::sin(InRotation), std::cos(InRotation);

		// warning: do not cast center positions into INT type. It will cause shaking input_image_data and jitters in the tracking.
		Eigen::Vector2f center(Cx, Cy);
		Eigen::Vector2f Rot_center = Rot * center;

		Matrix23f TransformSrcToDst;
		TransformSrcToDst.leftCols(2) = Rot;
		TransformSrcToDst.rightCols(1) = Rot_center;

		Matrix23f SrcFrame;
		{
			Eigen::Matrix3Xf SrcFrameOrig(3, 3);

			const float CroppedHalfWidthX = (0.5) * SizeX;
			const float CroppedHalfWidthY = (0.5) * SizeY;
			SrcFrameOrig << -CroppedHalfWidthX, CroppedHalfWidthX, CroppedHalfWidthX,
				-CroppedHalfWidthY, -CroppedHalfWidthY, CroppedHalfWidthY,
				1.f, 1.f, 1.f;
			SrcFrame = TransformSrcToDst * SrcFrameOrig;
		}

		Matrix23f DstFrame;
		DstFrame << 0, InCropBoxSize, InCropBoxSize, 0, 0, InCropBoxSize;

		Eigen::Matrix3Xf Src3(3, DstFrame.cols());
		Src3.topRows(2) = DstFrame;
		Src3.row(2).setOnes();

		const Eigen::MatrixX3f Src3T = Src3.transpose(); // x3f
		const Matrix23f Transform = SrcFrame * Src3T * (Src3 * Src3T).inverse() * TransformFlip;

		return Transform;
	}

	TArray<float> FHyprsenseUtils::WarpAffineBilinear(const uint8* InSrcImage, int32 InSrcWidth, int32 InSrcHeight, const Matrix23f& InTransform, int32 InTargetWidth, int32 InTargetHeight, bool bInIsDetector) const
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseUtils::WarpAffineBilinear);

		const uint8* SourceData = InSrcImage;

		const int32 PixelSize = 4;
		const int32 NumPixels = InTargetWidth * InTargetHeight;

		TArray<float> ResizedNNInput;
		ResizedNNInput.SetNum(NumPixels * 3);

		float* ResizedNNInputGreen = &ResizedNNInput[NumPixels];
		float* ResizedNNInputBlue = &ResizedNNInput[NumPixels * 2];

		const float Sqrt2 = FMath::Sqrt(2.f);
		const float ImageMean = 127.0f;
		const float ImageStd = 128.0f;

		const float BlackPixel = (bInIsDetector) ? -ImageMean / ImageStd : -0.5 * Sqrt2;

		ParallelFor(NumPixels, [&](int32 RC)
			{
				const float R = RC / InTargetWidth;
				const float C = RC - R * InTargetWidth;

				const Eigen::Vector3f TargetPixel(C, R, 1.0f);
				const Eigen::Vector2f SourcePixel = InTransform * TargetPixel;

				const float X = SourcePixel[0];
				const float Y = SourcePixel[1];

				const float X1 = FMath::Floor(X);
				const float X2 = FMath::CeilToFloat(X);

				const float Y1 = FMath::Floor(Y);
				const float Y2 = FMath::CeilToFloat(Y);

				const int32 OutputArrayLinearPosition = RC;
				if (0 <= X1 && X2 < InSrcWidth && 0 <= Y1 && Y2 < InSrcHeight)
				{
					const float XWeight1 = X2 - X;
					const float XWeight2 = 1.f - XWeight1;
					const float YWeight1 = Y2 - Y;
					const float YWeight2 = 1.f - YWeight1;

					const float Weight11 = XWeight1 * YWeight1;
					const float Weight12 = XWeight2 * YWeight1;
					const float Weight21 = XWeight1 * YWeight2;
					const float Weight22 = XWeight2 * YWeight2;

					const uint8* SrcCursor11 = &SourceData[PixelSize * int(X1 + Y1 * InSrcWidth)];
					const uint8* SrcCursor21 = &SourceData[PixelSize * int(X1 + Y2 * InSrcWidth)];
					const uint8* SrcCursor12 = &SourceData[PixelSize * int(X2 + Y1 * InSrcWidth)];
					const uint8* SrcCursor22 = &SourceData[PixelSize * int(X2 + Y2 * InSrcWidth)];

					const float Blue = FMath::Floor(Weight11 * (SrcCursor11[0]) + Weight12 * (SrcCursor12[0]) + Weight21 * (SrcCursor21[0]) + Weight22 * (SrcCursor22[0]));
					const float Green = FMath::Floor(Weight11 * (SrcCursor11[1]) + Weight12 * (SrcCursor12[1]) + Weight21 * (SrcCursor21[1]) + Weight22 * (SrcCursor22[1]));
					const float Red = FMath::Floor(Weight11 * (SrcCursor11[2]) + Weight12 * (SrcCursor12[2]) + Weight21 * (SrcCursor21[2]) + Weight22 * (SrcCursor22[2]));

					if (bInIsDetector)
					{
						ResizedNNInput[OutputArrayLinearPosition] = (Red - ImageMean) / ImageStd;
						ResizedNNInputGreen[OutputArrayLinearPosition] = (Green - ImageMean) / ImageStd;
						ResizedNNInputBlue[OutputArrayLinearPosition] = (Blue - ImageMean) / ImageStd;
					}
					else
					{
						ResizedNNInput[OutputArrayLinearPosition] = (((Red / 255.f) - 0.5) * Sqrt2);
						ResizedNNInputGreen[OutputArrayLinearPosition] = (((Green / 255.f) - 0.5) * Sqrt2);
						ResizedNNInputBlue[OutputArrayLinearPosition] = (((Blue / 255.f) - 0.5) * Sqrt2);
					}
				}
				else
				{
					ResizedNNInput[OutputArrayLinearPosition] = BlackPixel;
					ResizedNNInputGreen[OutputArrayLinearPosition] = BlackPixel;
					ResizedNNInputBlue[OutputArrayLinearPosition] = BlackPixel;
				}
			});
		return ResizedNNInput;
	}

	TArray<FHyprsenseUtils::Bbox> FHyprsenseUtils::HardNMS(const TArray<float>& InScores, const TArray<float>& InBoxes, float InIouThreshold, float InProbThreshold, int32 InTotalSize, int32 InTopK) const
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseUtils::HardNMS);

		TArray<Bbox> FilteredBoxes;

		for (int32 I = 0; I < InTotalSize; ++I)
		{
			int32 ScoreIdx = I * 2 + 1;
			int32 BoxIdx = I * 4;
			float Score = InScores[ScoreIdx];
			if (Score > InProbThreshold)
			{
				Bbox Box;
				Box.Score = Score;
				Box.X1 = InBoxes[BoxIdx];
				Box.Y1 = InBoxes[BoxIdx + 1];
				Box.X2 = InBoxes[BoxIdx + 2];
				Box.Y2 = InBoxes[BoxIdx + 3];
				Box.Area = (Box.X2 - Box.X1) * (Box.Y2 - Box.Y1);
				FilteredBoxes.Add(Box);
			}
		}
		FilteredBoxes.Sort([](const Bbox& a, const Bbox& b) { return a.Score > b.Score; });

		for (auto Iter(FilteredBoxes.CreateIterator()); Iter; ++Iter)
		{
			for (auto Iter2 = Iter + 1; Iter2; ++Iter2)
			{
				float Iou = IOU(*Iter, *Iter2);
				if (Iou > InIouThreshold)
				{
					Iter2.RemoveCurrent();
				}
			}
		}

		return FilteredBoxes;
	}

	float FHyprsenseUtils::IOU(const Bbox& InBox1, const Bbox& InBox2) const
	{
		MHA_CPUPROFILER_EVENT_SCOPE(FHyprsenseUtils::IOU);

		const float MaxX = (InBox1.X1 > InBox2.X1) ? InBox1.X1 : InBox2.X1;
		const float MaxY = (InBox1.Y1 > InBox2.Y1) ? InBox1.Y1 : InBox2.Y1;
		const float MinX = (InBox1.X2 < InBox2.X2) ? InBox1.X2 : InBox2.X2;
		const float MinY = (InBox1.Y2 < InBox2.Y2) ? InBox1.Y2 : InBox2.Y2;

		const float Width = ((MinX - MaxX + 0.01f) > 0) ? (MinX - MaxX + 0.01f) : 0.0f;
		const float Height = ((MinY - MaxY + 0.01f) > 0) ? (MinY - MaxY + 0.01f) : 0.0f;
		const float Overlap = Width * Height;

		return Overlap / (InBox1.Area + InBox2.Area - Overlap);
	}

}
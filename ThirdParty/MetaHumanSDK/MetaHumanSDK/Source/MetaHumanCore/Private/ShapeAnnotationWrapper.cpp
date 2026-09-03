// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShapeAnnotationWrapper.h"
#include "Polygon2.h"

#include "ShapeAnnotation.h"
#include "MetaHumanContourData.h"

using namespace ShapeAnnotation;

class FShapeAnnotationWrapper::FImpl
{
public:

	TArray<FVector2D> GetControlKeyPositionsOnCurve(const TArray<FVector2D>& InTrackingPoints, ECurveDisplayMode InMode, const FString& InCurveName) const;
	TArray<FVector2D> GetEvenlySpacedControlPoints(const TArray<FVector2D>& InTrackingPoints, const FString& InCurveName) const;
	TArray<FVector2D> GetControlPointsForSimplifiedCurve(const TArray<FVector2D>& InTrackingPoints) const;

	TArray<FVector2D> ConvertFPointToVector2DArray(const TArray<FPoint2D>& InPoints);
	TArray<FPoint2D> ConvertVector2DToFPoint2DArray(const TArray<FVector2D>& InPoints);

	/** List of start/end points that should be labeled as sharp when creating splines */
	TArray<FString> SharpPointList;

	/**Holds the number of points per curve in FTrackingContourData format. End points listed as separate entries */
	TMap<FString, int32> NDenseInternals;

	/** A helper class to convert control vertices into splines */
	ShapeAnnotation::FShapeAnnotation ShapeAnnotation;

	static TMap<FString, TArray<int32>> CurveControlPointIndexPreset;
};

TMap<FString, TArray<int32>> FShapeAnnotationWrapper::FImpl::CurveControlPointIndexPreset = {
	{ "crv_brow_upper_r", { 0, 3, 6, 9, 12, 15, 19, 21, 24, } }, { "crv_brow_lower_r", { 0, 3, 6, 10, 13, 16, 19, } },
	{ "crv_brow_intermediate_r", { 0, 2, 3, 5, } }, { "crv_brow_intermediate_l", { 0, 2, 3, 5, } },
	{ "crv_brow_upper_l", { 0, 3, 6, 9, 12, 16, 19, 21, 24, } }, { "crv_brow_lower_l", { 0, 3, 7, 10, 13, 16, 19, } },
	{ "crv_eyelid_upper_r", { 0, 4, 8, 11, 15, 19, } },	{ "crv_eyelid_lower_r", { 0, 4, 8, 12, 15, 19, } },
	{ "crv_iris_r", { 0, 2, 3, 5, 6, 7, 8, 10, 12, 13, 15, 17, 18, 19, 20, 22, 23, 25, } },	{ "crv_eyelid_upper_l", { 0, 3, 5, 8, 11, 14, 16, 19, } },
	{ "crv_eyelid_lower_l", { 0, 4, 8, 12, 15, 19, } },	{ "crv_iris_l", { 0, 2, 4, 5, 6, 7, 8, 10, 11, 13, 15, 17, 18, 19, 20, 22, 23, 25, } },
	{ "crv_lip_upper_outer_r", { 0, 8, 16, 24, } },	{ "crv_lip_philtrum_r", { 0, 2, 5, 7, } },
	{ "crv_lip_philtrum_l", { 0, 2, 5, 7, } },	{ "crv_lip_upper_outer_l", { 0, 8, 16, 24, } },
	{ "crv_lip_lower_outer_l", { 0, 6, 11, 17, 23, 28, } },	{ "crv_lip_lower_outer_r", { 0, 6, 12, 17, 23, 28, } },
	{ "crv_lip_upper_inner_r", { 0, 8, 17, 25, } },	{ "crv_lip_upper_inner_l", { 0, 8, 17, 25, } },
	{ "crv_lip_lower_inner_l", { 0, 8, 17, 25, } },	{ "crv_lip_lower_inner_r", { 0, 8, 17, 25, } }
};

TArray<FVector2D> FShapeAnnotationWrapper::FImpl::GetControlPointsForSimplifiedCurve(const TArray<FVector2D>& InTrackingPoints) const
{
	const double Tolerance = 0.00001;
	const double Distance = 0.5;
	UE::Geometry::FPolygon2d Polygon;
	Polygon.AppendVertices(InTrackingPoints);
	Polygon.Simplify(Tolerance, Distance);

	return Polygon.GetVertices();
}

TArray<FPoint2D> FShapeAnnotationWrapper::FImpl::ConvertVector2DToFPoint2DArray(const TArray<FVector2D>& InPoints)
{
	TArray<FPoint2D> Result;
	Result.Reserve(InPoints.Num());
	
	for (const FVector2D& Point : InPoints)
	{
		Result.Add({ Point.X, Point.Y });
	}

	return Result;
}

TArray<FVector2D> FShapeAnnotationWrapper::FImpl::ConvertFPointToVector2DArray(const TArray<FPoint2D>& InPoints)
{
	TArray<FVector2D> Result;
	Result.Reserve(InPoints.Num());
	
	for (const FPoint2D& Point : InPoints)
	{
		Result.Add({ Point.X, Point.Y });
	}

	return Result;
}

TArray<FVector2D> FShapeAnnotationWrapper::FImpl::GetEvenlySpacedControlPoints(const TArray<FVector2D>& InTrackingPoints, const FString& InCurveName) const
{
	TArray<FVector2D> Simplified;

	if (const TArray<int32>* CurveIndices = CurveControlPointIndexPreset.Find(InCurveName))
	{
		int32 LastIndex = CurveIndices->Last();
		if (LastIndex > InTrackingPoints.Num())
		{
			Simplified = InTrackingPoints;
		}
		else
		{
			TArray<FVector2D> ReducedPoints;
			for (const int32 Index : *CurveIndices)
			{
				ReducedPoints.Add(InTrackingPoints[Index]);
			}

			Simplified = ReducedPoints;
		}
	}

	return Simplified;
}

TArray<FVector2D> FShapeAnnotationWrapper::FImpl::GetControlKeyPositionsOnCurve(const TArray<FVector2D>& InTrackingPoints, ECurveDisplayMode InMode, const FString& InCurveName) const
{
	TArray<FVector2D> Simplified;

	if (InTrackingPoints.IsEmpty())
	{
		return Simplified;
	}
	else if (InTrackingPoints.Num() == 1)
	{
		return InTrackingPoints;
	}
	else if (InMode == ECurveDisplayMode::Editing)
	{
		Simplified = GetControlPointsForSimplifiedCurve(InTrackingPoints);
	}
	else if (InMode == ECurveDisplayMode::Visualization)
	{
		Simplified = GetEvenlySpacedControlPoints(InTrackingPoints, InCurveName);
	}

	return Simplified;
}

FShapeAnnotationWrapper::FShapeAnnotationWrapper()
{
	Impl = MakePimpl<FImpl>();

	//TODO: Move this to the config, explicitly adding smooth and sharp styles to start/end points
	Impl->SharpPointList = {
		"pt_mouth_corner_r", "pt_mouth_corner_l", "pt_naso_upper_l", "pt_naso_lower_l", "pt_naso_upper_r", "pt_naso_lower_r",
		"pt_brow_inner_r", "pt_brow_inner_l"
	};
}

FShapeAnnotationWrapper::~FShapeAnnotationWrapper()
{
	Impl = nullptr;
}

TMap<FString, TArray<FVector2D>> FShapeAnnotationWrapper::GetDrawingSplinesFromContourData(const TObjectPtr<UMetaHumanContourData> InContourData)
{
	TMap<FString, TArray<FVector2D>> DrawingSplines;
	InitializeShapeAnnotation(InContourData, false);

	auto ReducedSplineData = Impl->ShapeAnnotation.GetDrawingSplines(Impl->NDenseInternals);

	for (const auto& Spline : ReducedSplineData)
	{
		FString FeatureName = Spline.Key;

		TArray<FVector2D> Points = Impl->ConvertFPointToVector2DArray(Spline.Value);
		DrawingSplines.Add(FeatureName, Points);
	}

	return DrawingSplines;
}

TArray<FVector2D> FShapeAnnotationWrapper::GetControlVerticesForCurve(const TArray<FVector2D>& InLandmarkData, const FString& InCurveName, ECurveDisplayMode InDisplayMode) const
{
	TArray<FVector2D> ControlVerts;
	if (!InLandmarkData.IsEmpty())
	{
		if (InLandmarkData.Num() == 1)
		{
			ControlVerts = InLandmarkData;
		}
		else
		{
			TArray<FVector2D> ReducedInternalPoints = Impl->GetControlKeyPositionsOnCurve(InLandmarkData, InDisplayMode, InCurveName);

			ReducedInternalPoints.RemoveAt(0);
			ReducedInternalPoints.Pop();
			ControlVerts = ReducedInternalPoints;
		}
	}

	return ControlVerts;
}

void FShapeAnnotationWrapper::InitializeShapeAnnotation(const TObjectPtr<UMetaHumanContourData> InContourData, bool bUseDensePoints)
{
	TMap<FString, FKeypoint> KeyPoints;
	TMap<FString, FKeypointCurve> KeypointCurves;

	for (const TPair<FString, FTrackingContour>& Contour : InContourData->FrameTrackingContourData.TrackingContours)
	{
		FString FeatureName = Contour.Key;
		int32 Size = Contour.Value.DensePoints.Num();
		if (Size > 1)
		{
			Impl->NDenseInternals.Add(FeatureName, Size);

			FKeypointCurve Curve;
			Curve.StarKeypointName = Contour.Value.StartPointName;
			Curve.EndKeypointName = Contour.Value.EndPointName;
			Curve.InternalPoints = Impl->ConvertVector2DToFPoint2DArray(InContourData->GetControlVertexPositions(Contour.Key));

			KeypointCurves.Add({FeatureName, Curve});
		}
		else if (Size == 1)
		{
			FKeypoint Keypoint;
			Keypoint.Style = Impl->SharpPointList.Contains(Contour.Key) ? EVertexStyle::Sharp : EVertexStyle::Smooth;
			FVector2D LandmarkPoint = InContourData->ReducedContourData[Contour.Key].ControlVertices.Last().PointPosition;
			Keypoint.Pos = { LandmarkPoint.X, LandmarkPoint.Y };
			KeyPoints.Add({ FeatureName, Keypoint });
		}
	}
	
	Impl->ShapeAnnotation.Initialize(KeyPoints, KeypointCurves);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanContourData.h"
#include "Algo/Find.h"

void UMetaHumanContourData::SetContourDataForDrawing(const TMap<FString, TArray<FVector2D>>& InContoursDrawData)
{
	CurveDrawDataFromReducedContours = InContoursDrawData;
}

void UMetaHumanContourData::SetFullCurveContourDataForDrawing(const TMap<FString, TArray<FVector2D>>& InFullContoursForDraw)
{
	CurveDrawDataFromTrackingContours = InFullContoursForDraw;
}

void UMetaHumanContourData::ClearGeneratedDrawData()
{
	CurveDrawDataFromReducedContours.Empty();
	CurveDrawDataFromTrackingContours.Empty();
	ReducedContourData.Empty();
}

bool UMetaHumanContourData::ControlVertexIsVisible(const FControlVertex& InVertex) const
{
	bool bVisible = false;

	for (const FString& Curve : InVertex.CurveNames)
	{
		if (const FTrackingContour* Contour = FrameTrackingContourData.TrackingContours.Find(Curve))
		{
			bVisible |= Contour->State.bVisible;
		}
	}

	return bVisible;
}

bool UMetaHumanContourData::ContourIsVisible(const FString& InCurveName) const
{
	if (const FTrackingContour* Contour = FrameTrackingContourData.TrackingContours.Find(InCurveName))
	{
		return Contour->State.bVisible;
	}

	return false;
}

const TMap<FString, TArray<FVector2D>>& UMetaHumanContourData::GetReducedDataForDrawing()
{
	return CurveDrawDataFromReducedContours;
}

const TMap<FString, TArray<FVector2D>>& UMetaHumanContourData::GetTrackingContourDataForDrawing()
{
	return CurveDrawDataFromTrackingContours;
}

FControlVertex* UMetaHumanContourData::GetControlVertexFromPointId(const int32 InPointId)
{
	for (TPair<FString, FReducedContour>& ReducedContour : ReducedContourData)
	{
		FControlVertex* Contour = Algo::FindByPredicate(ReducedContour.Value.ControlVertices, [InPointId](FControlVertex& InVertex)
		{
			return InVertex.PointId == InPointId;
		});

		if (Contour)
		{
			return Contour;
		}
	}

	return nullptr;
}

TArray<FControlVertex> UMetaHumanContourData::GetControlVerticesForCurve(const FString& InCurveName)
{
	TArray<FControlVertex> ControlVerts;

	if (const FReducedContour* Contour = ReducedContourData.Find(InCurveName))
	{
		for (const FControlVertex& Vertex : Contour->ControlVertices)
		{
			ControlVerts.Add(Vertex);
		}
	}

	return ControlVerts;
}

TArray<FVector2D> UMetaHumanContourData::GetControlVertexPositions(const FString& InCurveName)
{
	TArray<FVector2D> Positions;

	if (const FReducedContour* Contour = ReducedContourData.Find(InCurveName))
	{
		for (const FControlVertex& Vertex : Contour->ControlVertices)
		{
			Positions.Add(Vertex.PointPosition);
		}
	}

	return Positions;
}

TArray<int32> UMetaHumanContourData::GetPointIdsWithEndpointsForCurve(const FString& InCurveName) const
{
	TArray<int32> Ids;
	if (const FReducedContour* Contour = ReducedContourData.Find(InCurveName))
	{
		for (const FControlVertex& Vertex : Contour->ControlVertices)
		{
			Ids.Add(Vertex.PointId);
		}

		TPair<FString, FString> EndPoints = GetStartEndNamesForCurve(InCurveName);
		if (!EndPoints.Key.IsEmpty() && !EndPoints.Value.IsEmpty())
		{
			Ids.Append(GetPointIdsForCurve(EndPoints.Key));
			Ids.Append(GetPointIdsForCurve(EndPoints.Value));
		}
	}

	return Ids;
}

TSet<FString> UMetaHumanContourData::GetSelectedCurves() const
{
	TSet<FString> Curves;
	for (const TPair<FString, FTrackingContour>& Contour : FrameTrackingContourData.TrackingContours)
	{
		if (Contour.Value.State.bSelected)
		{
			Curves.Add(Contour.Key);
		}
	}
	return Curves;
}

const TArray<int32> UMetaHumanContourData::GetPointIdsForCurve(const FString& InCurveName) const
{
	TArray<int32> Ids;

	if (const FReducedContour* Contour = ReducedContourData.Find(InCurveName))
	{
		for (const FControlVertex& Vertex : Contour->ControlVertices)
		{
			Ids.Add(Vertex.PointId);
		}
	}

	return Ids;
}

TPair<FString, FString> UMetaHumanContourData::GetStartEndNamesForCurve(const FString& InCurveName) const
{
	TPair<FString, FString> EndPointNames;

	if (const FTrackingContour* Contour = FrameTrackingContourData.TrackingContours.Find(InCurveName))
	{
		EndPointNames = { Contour->StartPointName, Contour->EndPointName };
	}
	return EndPointNames;
}

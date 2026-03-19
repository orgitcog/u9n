// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCurveDataController.h"
#include "MetaHumanContourData.h"
#include "Polygon2.h"
#include "Algo/AllOf.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "MetaHumanCurveDataController"

FMetaHumanCurveDataController::FMetaHumanCurveDataController(TObjectPtr<UMetaHumanContourData> InCurveData, ECurveDisplayMode InMode)
	:ContourData(InCurveData)
	, DisplayMode(InMode) 
{
}

void FMetaHumanCurveDataController::InitializeContoursFromConfig(const FFrameTrackingContourData& InDefaultContourData, const FString& InConfigVersion)
{
	ContourData->ContourDataConfigVersion = InConfigVersion;
	ContourData->FrameTrackingContourData = InDefaultContourData;
	GenerateCurveDataPostTrackingDataChange();
}

void FMetaHumanCurveDataController::UpdateFromContourData(const FFrameTrackingContourData& InTrackingData, bool bUpdateVisibility)
{
	for (const TPair<FString, FTrackingContour>& Contour : InTrackingData.TrackingContours)
	{
		const FString& Name = Contour.Key;
		FTrackingContour* TrackingContour = ContourData->FrameTrackingContourData.TrackingContours.Find(Name);

		if(TrackingContour)
		{
			TrackingContour->DensePoints = Contour.Value.DensePoints;
			TrackingContour->DensePointsConfidence = Contour.Value.DensePointsConfidence;

			if (bUpdateVisibility)
			{
				TrackingContour->State.bVisible = true;
				TrackingContour->State.bActive = true;
				ContourData->ManuallyModifiedCurves.Remove(Name);
			}
		}
	}

	GenerateCurveDataPostTrackingDataChange();
	UpdateContourDelegate.Broadcast();
}

void FMetaHumanCurveDataController::UpdateIndividualCurves(const FFrameTrackingContourData& InTrackingData)
{
	for (const TPair<FString, FTrackingContour>& Contour : InTrackingData.TrackingContours)
	{
		const FString& Name = Contour.Key;
		FTrackingContour* TrackingContour = ContourData->FrameTrackingContourData.TrackingContours.Find(Name);

		if (TrackingContour)
		{
			TrackingContour->DensePoints = Contour.Value.DensePoints;
			TrackingContour->DensePointsConfidence = Contour.Value.DensePointsConfidence;
		}

		if (ContourData->ReducedContourData.Contains(Name))
		{
			ContourData->ReducedContourData[Name] = GetReducedContourForTrackingContour(Contour);
		}
	}

	RecreateControlVertexIds();
	GenerateDrawDataForDensePoints();
	GenerateCurvesFromControlVertices();
	UpdateContourDelegate.Broadcast();
}

void FMetaHumanCurveDataController::RecreateCurvesFromReducedData()
{
	GenerateCurvesFromControlVertices();
	UpdateContourDelegate.Broadcast();
}

void FMetaHumanCurveDataController::SetCurveSelection(const TSet<FString>& InSelectedCurves, bool bClearPointSelection)
{
	ClearCurveSelection();

	for (const FString& SelectedContour : InSelectedCurves)
	{
		FTrackingContour* Contour = ContourData->FrameTrackingContourData.TrackingContours.Find(SelectedContour);
		if (Contour)
		{
			Contour->State.bSelected = true;
		}
	}

	OnCurvesSelectedDelegate.Broadcast(bClearPointSelection);
}

void FMetaHumanCurveDataController::ResolveCurveSelectionFromSelectedPoints(const TSet<int32>& InSelectedPoints)
{
	TSet<FString> AllCurveNamesFromSelection;
	TSet<FString> SelectedCurves;

	for (const int32 Id : InSelectedPoints)
	{
		const FControlVertex* Vert = ContourData->GetControlVertexFromPointId(Id);
		AllCurveNamesFromSelection.Append(Vert->CurveNames);
	}

	for (const FString& Name : AllCurveNamesFromSelection)
	{
		TArray<int32> Ids = ContourData->GetPointIdsWithEndpointsForCurve(Name);
		bool AllVerticesSelected = Algo::AllOf(Ids, [&InSelectedPoints](const int32& Id) 
		{
			return InSelectedPoints.Contains(Id);
		});

		if (AllVerticesSelected)
		{
			SelectedCurves.Add(Name);
		}
	}

	SetCurveSelection(SelectedCurves, false);
}

TPair<bool, bool> FMetaHumanCurveDataController::GetCurveSelectedAndActiveStatus(const FString& InCurve)
{
	TPair<bool, bool> SelectedAndActiveStatus = { false, false };
	FTrackingContour* Contour = ContourData->FrameTrackingContourData.TrackingContours.Find(InCurve);
	if (Contour)
	{
		SelectedAndActiveStatus = { Contour->State.bSelected, Contour->State.bActive };
	}

	return SelectedAndActiveStatus;
}

void FMetaHumanCurveDataController::GenerateCurvesFromControlVertices()
{
	TMap<FString, TArray<FVector2D>> ShapeAnnotationGeneratedCurves = ShapeAnnotationWrapper.GetDrawingSplinesFromContourData(ContourData);
	ContourData->SetContourDataForDrawing(ShapeAnnotationGeneratedCurves);
}

void FMetaHumanCurveDataController::GenerateDrawDataForDensePoints()
{
	TMap<FString, TArray<FVector2D>> TrackingCurves;
	for (const TPair<FString, FTrackingContour>& Contour : ContourData->FrameTrackingContourData.TrackingContours)
	{
		TArray<FVector2D> Points = Contour.Value.DensePoints;
		// End points and single point curves are drawn as control vertices
		if (Points.Num() > 1)
		{
			TrackingCurves.Add(Contour.Key, Points);
		}
	}

	ContourData->SetFullCurveContourDataForDrawing(TrackingCurves);
}

void FMetaHumanCurveDataController::ClearCurveSelection()
{
	for (TPair<FString, FTrackingContour>& Contour : ContourData->FrameTrackingContourData.TrackingContours)
	{
		Contour.Value.State.bSelected = false;
	}
}

void FMetaHumanCurveDataController::GenerateCurveDataPostTrackingDataChange()
{
	CreateControlVertices();
	GenerateDrawDataForDensePoints();

	if (DisplayMode == ECurveDisplayMode::Editing)
	{
		GenerateCurvesFromControlVertices();
	}
}

void FMetaHumanCurveDataController::ModifyViewportEndPointSelectionForCurveVisibility(const FString& InCurveName, const FString& InEndPointName)
{
	TSet<int32>& ViewportSelection = ViewportPointSelectionRetrieverDelegate().Execute();
	if (const FTrackingContour* CurveData = ContourData->FrameTrackingContourData.TrackingContours.Find(InCurveName))
	{
		FControlVertex EndPointVertex = ContourData->GetControlVerticesForCurve(InEndPointName).Last();
		TArray<FControlVertex> OtherEndpointCurvePoints;
		for (const FString& Curve : EndPointVertex.CurveNames)
		{
			const FTrackingContour* EndpointCurve = ContourData->FrameTrackingContourData.TrackingContours.Find(Curve);
			if (Curve != InCurveName && EndpointCurve && EndpointCurve->State.bVisible)
			{
				OtherEndpointCurvePoints.Append(ContourData->GetControlVerticesForCurve(Curve));
			}
		}
		
		// Check if any vertex is selected on any other curve endpoint might be on to make sure we don't de-select it
		if (Algo::NoneOf(OtherEndpointCurvePoints, [ViewportSelection](const FControlVertex& InVert) {
			return ViewportSelection.Contains(InVert.PointId);
		}))
		{
			ViewportSelection.Remove(EndPointVertex.PointId);
		}
	}
}

void FMetaHumanCurveDataController::ClearContourData()
{
	ContourData->FrameTrackingContourData.TrackingContours.Empty();
}

void FMetaHumanCurveDataController::OffsetSelectedPoints(const TSet<int32>& InSelectedPoints, const FVector2D& InOffset)
{
	for (const int32 PointId : InSelectedPoints)
	{
		FControlVertex* Vertex = ContourData->GetControlVertexFromPointId(PointId);
		if (Vertex && ContourData->ControlVertexIsVisible(*Vertex))
		{
			Vertex->PointPosition -= InOffset;
			ContourData->ManuallyModifiedCurves.Append(Vertex->CurveNames);
		}
	}

	GenerateCurvesFromControlVertices();
}

void FMetaHumanCurveDataController::MoveSelectedPoint(const FVector2D& InNewPosition, const int32 InPointId)
{
	FControlVertex* Vertex = ContourData->GetControlVertexFromPointId(InPointId);
	if (Vertex)
	{
		Vertex->PointPosition = InNewPosition;
		GenerateCurvesFromControlVertices();

		ContourData->ManuallyModifiedCurves.Append(Vertex->CurveNames);
	}
}

void FMetaHumanCurveDataController::UpdateDensePointsAfterDragging(const TSet<int32>& InDraggedIds)
{
	for (const int32 Id : InDraggedIds)
	{
		const FControlVertex* Vertex = ContourData->GetControlVertexFromPointId(Id);
		if (Vertex->bIsSinglePointCurve)
		{
			const FString CurveName = Vertex->CurveNames.Last();
			FTrackingContour* Contour = ContourData->FrameTrackingContourData.TrackingContours.Find(CurveName);
			Contour->DensePoints = { Vertex->PointPosition };
		}
	}

	TMap<FString, TArray<FVector2D>> ShapeAnnotationGeneratedCurves = ShapeAnnotationWrapper.GetDrawingSplinesFromContourData(ContourData);

	for (TPair<FString, TArray<FVector2D>>& GeneratedCurve : ShapeAnnotationGeneratedCurves)
	{
		FTrackingContour* Contour = ContourData->FrameTrackingContourData.TrackingContours.Find(GeneratedCurve.Key);
		if (Contour && Contour->DensePoints.Num() > 1)
		{
			Contour->DensePoints = GeneratedCurve.Value;
		}
	}
}

void FMetaHumanCurveDataController::HandleUndoOperation()
{
	bool bClearPoints = true;
	RecreateCurvesFromReducedData();
	GenerateDrawDataForDensePoints();
	OnCurvesSelectedDelegate.Broadcast(bClearPoints);
}

void FMetaHumanCurveDataController::ClearDrawData()
{
	ContourData->ClearGeneratedDrawData();
	UpdateContourDelegate.Broadcast();
}

void FMetaHumanCurveDataController::ResolvePointSelectionOnCurveVisibilityChanged(const TArray<FString>& InCurveNames, bool bInSingleCurve, bool bInIsHiding)
{
	if (bInIsHiding && ViewportPointSelectionRetrieverDelegate().IsBound() && !InCurveNames.IsEmpty())
	{
		TSet<int32>& ViewportSelection = ViewportPointSelectionRetrieverDelegate().Execute();
		if (bInSingleCurve)
		{
			// If hiding individual curves we need to make sure end-points remain selected if adjacent curves have selection
			FString CurveName = InCurveNames.Last();
			if (const FTrackingContour* CurveData = ContourData->FrameTrackingContourData.TrackingContours.Find(CurveName))
			{
				for (const FControlVertex& ControlVert : ContourData->GetControlVerticesForCurve(CurveName))
				{
					ViewportSelection.Remove(ControlVert.PointId);
				}

				if (!CurveData->StartPointName.IsEmpty())
				{
					ModifyViewportEndPointSelectionForCurveVisibility(CurveName, CurveData->StartPointName);
				}

				if (!CurveData->EndPointName.IsEmpty())
				{
					ModifyViewportEndPointSelectionForCurveVisibility(CurveName, CurveData->EndPointName);
				}
			}
		}
		else
		{
			// If hiding a group or entire frame just hide all control points & endpoints
			for (const FString& CurveName : InCurveNames)
			{
				for (const int32 Id : GetPointIdsWithEndPointsForCurve(CurveName))
				{
					ViewportSelection.Remove(Id);
				}
			}
		}
	}

	OnCurvesSelectedDelegate.Broadcast(false);
}

bool FMetaHumanCurveDataController::AddRemoveKey(const FVector2D& InPointPosition, const FString& InCurveName, bool bInAdd)
{
	bool bSuccess = false;
	FReducedContour* ContourKeys = ContourData->ReducedContourData.Find(InCurveName);
	int32 ClosestIndex = INDEX_NONE;
	float ToClosestPoint = GetDistanceToNearestVertex(InPointPosition, InCurveName, ClosestIndex);
	bool bProceed = bInAdd ? ToClosestPoint > CurveAddRemoveThreshold : ToClosestPoint < CurveAddRemoveThreshold;

	if (bProceed)
	{
#if WITH_EDITOR
		FScopedTransaction Transaction = FScopedTransaction(LOCTEXT("MetaHumanCurveAddRemovePoint", "Add/Remove point operation"));
#endif
		ContourData->Modify();

		if (bInAdd)
		{
			FControlVertex AddVertex;
			AddVertex.PointPosition = InPointPosition;
			AddVertex.CurveNames = { InCurveName };
			int32 Index = GetCurveInsertionIndex(InPointPosition, InCurveName);
			int32 ControlVertNumber = ContourKeys->ControlVertices.Num(); // Does not contain end points
			if (Index != INDEX_NONE && Index <= ControlVertNumber + 1)
			{
				if (Index > ControlVertNumber)
				{
					ContourKeys->ControlVertices.Add(AddVertex);
				}
				else
				{
					ContourKeys->ControlVertices.EmplaceAt(Index, AddVertex);
				}
				bSuccess = true;
			}	
		}
		else
		{
			int32 NumberOfPoints = ContourKeys->ControlVertices.Num();
			if (NumberOfPoints > 1 && ClosestIndex != INDEX_NONE)
			{
				ContourKeys->ControlVertices.RemoveAt(ClosestIndex);
				bSuccess = true;
			}
		}
	}

	if (bSuccess)
	{
		ContourData->ManuallyModifiedCurves.Add(InCurveName);
		RecreateControlVertexIds();
		GenerateCurvesFromControlVertices();
	}

	return bSuccess;
}

TArray<FString> FMetaHumanCurveDataController::GetCurveNamesForPointId(const int32 InPointId)
{
	TArray<FString> CurveNames;
	const FControlVertex* Vertex = ContourData->GetControlVertexFromPointId(InPointId);
	if (Vertex)
	{
		CurveNames = Vertex->CurveNames;
	}

	return CurveNames;
}

TArray<int32> FMetaHumanCurveDataController::GetPointIdsWithEndPointsForCurve(const FString& InCurveName) const
{
	return ContourData->GetPointIdsWithEndpointsForCurve(InCurveName);
}

bool FMetaHumanCurveDataController::CurveIsVisible(const FString& InCurveName) const
{
	FTrackingContour* TrackingContour = ContourData->FrameTrackingContourData.TrackingContours.Find(InCurveName);
	if (TrackingContour && TrackingContour->State.bVisible)
	{
		return true;
	}

	return false;
}

int32 FMetaHumanCurveDataController::GetCurveInsertionIndex(const FVector2D& InInsertionPos, const FString& InCurveName)
{
	const FTrackingContour* Contour = ContourData->FrameTrackingContourData.TrackingContours.Find(InCurveName);
	const FReducedContour* ControlVertData = ContourData->ReducedContourData.Find(InCurveName);

	if (Contour && !Contour->StartPointName.IsEmpty() && ControlVertData)
	{
		int32 NInternal = ControlVertData->ControlVertices.Num();
		TArray<FControlVertex> StartPointVertex = ContourData->GetControlVerticesForCurve(Contour->StartPointName);
		FVector2D StartPointPos = StartPointVertex.Last().PointPosition;

		FVector2D ToStartPoint = InInsertionPos - StartPointPos;
		ToStartPoint.Normalize();

		for (int32 Ctr = 0; Ctr < NInternal; ++Ctr)
		{

			FVector2D ToCurrentPoint = InInsertionPos - ControlVertData->ControlVertices[Ctr].PointPosition;
			ToCurrentPoint.Normalize();

			const float VectorAngle = acosf(FVector2D::DotProduct(ToStartPoint, ToCurrentPoint));

			if (VectorAngle > PI/2.0f)
			{
				return Ctr;
			}

			ToStartPoint = ToCurrentPoint;
		}

		return NInternal + 1;
	}

	return INDEX_NONE;
}

float FMetaHumanCurveDataController::GetDistanceToNearestVertex(const FVector2D& InPosition, const FString& InCurveName, int32& outIndex)
{
	float BestDistance = TNumericLimits<float>::Max();
	int32 IndexCounter = 0;
	if (FReducedContour* ContourKeys = ContourData->ReducedContourData.Find(InCurveName))
	{
		for (const FControlVertex& Vertex : ContourKeys->ControlVertices)
		{
			float Distance = FVector2D::Distance(InPosition, Vertex.PointPosition);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				outIndex = IndexCounter;
			}

			++IndexCounter;
		}
	}

	return BestDistance;
}

TMap<FString, TArray<FVector2D>> FMetaHumanCurveDataController::GetDensePointsForVisibleCurves() const
{
	TMap<FString, TArray<FVector2D>> DisplayData;

	if (DisplayMode == ECurveDisplayMode::Editing)
	{
		DisplayData = GetCurveDisplayDataForEditing();
	}
	else if (DisplayMode == ECurveDisplayMode::Visualization)
	{
		DisplayData = GetCurveDisplayDataForVisualization();
	}

	return DisplayData;
}

TMap<FString, TArray<FVector2D>> FMetaHumanCurveDataController::GetFullSplineDataForVisibleCurves() const
{
	TMap<FString, TArray<FVector2D>> VisibleCurves;

	for (const TPair<FString, TArray<FVector2D>>& Curve : ContourData->GetTrackingContourDataForDrawing())
	{
		if (CurveIsVisible(Curve.Key))
		{
			VisibleCurves.Add(Curve);
		}
	}

	return VisibleCurves;
}

void FMetaHumanCurveDataController::CreateControlVertices()
{
	int32 PointID = 0;
	ContourData->ReducedContourData.Empty();
	ContourData->ManuallyModifiedCurves.Empty();

	for (const TPair<FString, FTrackingContour>& Contour : ContourData->FrameTrackingContourData.TrackingContours)
	{
		const TArray<FVector2D>& DensePoints = Contour.Value.DensePoints;
		const FString& ContourName = Contour.Key;
		TArray<FControlVertex> ControlVertsForCurve;

		if (DensePoints.Num() == 1)
		{
			FControlVertex ControlVertex;
			ControlVertex.PointPosition = DensePoints.Last();
			ControlVertex.PointId = ++PointID;

			TArray<FString> CurveNamesForEndPoints = GetCurveNamesForEndPoints(Contour.Key);
			if (!CurveNamesForEndPoints.IsEmpty())
			{
				ControlVertex.CurveNames = CurveNamesForEndPoints;
			}
			else
			{
				ControlVertex.CurveNames = { Contour.Key };
				ControlVertex.bIsSinglePointCurve = true;
			}

			ControlVertsForCurve.Add(ControlVertex);
		}
		else
		{
			const TArray<FVector2D>& ControlPoints = ShapeAnnotationWrapper.GetControlVerticesForCurve(DensePoints, ContourName, DisplayMode);

			for (const FVector2D& ControlPoint : ControlPoints)
			{
				FControlVertex ControlVertex;
				ControlVertex.PointPosition = ControlPoint;
				ControlVertex.PointId = ++PointID;
				ControlVertex.CurveNames.Add(ContourName);
				ControlVertsForCurve.Add(ControlVertex);
			}
		}

		FReducedContour ReducedContour = { ControlVertsForCurve };
		ContourData->ReducedContourData.Add(ContourName, ReducedContour);
	}
}

void FMetaHumanCurveDataController::RecreateControlVertexIds()
{
	int32 IdCounter = 0;

	for (TPair<FString, FReducedContour>& ControlVertData : ContourData->ReducedContourData)
	{
		for (FControlVertex& Vertx : ControlVertData.Value.ControlVertices)
		{
			Vertx.PointId = ++IdCounter;
		}
	}
}

TArray<FControlVertex> FMetaHumanCurveDataController::GetAllVisibleControlVertices()
{
	TArray<FControlVertex> ControlVerts;
	for (const TPair<FString, FTrackingContour>& Contour : ContourData->FrameTrackingContourData.TrackingContours)
	{
		if (Contour.Value.State.bVisible)
		{
			ControlVerts.Append(ContourData->GetControlVerticesForCurve(Contour.Key));
		}
	}

	return ControlVerts;
}

TArray<FString> FMetaHumanCurveDataController::GetCurveNamesForEndPoints(const FString& InEndPointName) const
{
	TArray<FString> CurveNames;
	for (const TPair<FString, FTrackingContour>& Contour : ContourData->FrameTrackingContourData.TrackingContours)
	{
		if (Contour.Value.StartPointName == InEndPointName || Contour.Value.EndPointName == InEndPointName)
		{
			CurveNames.Add(Contour.Key);
		}
	}

	return CurveNames;
}

TMap<FString, TArray<FVector2D>> FMetaHumanCurveDataController::GetCurveDisplayDataForEditing() const
{
	TMap<FString, TArray<FVector2D>> VisibleCurves;
	const TMap<FString, TArray<FVector2D>>& ReducedData = ContourData->GetReducedDataForDrawing();

	for (const TPair<FString, TArray<FVector2D>>& Curve : ContourData->GetTrackingContourDataForDrawing())
	{
		if (CurveIsVisible(Curve.Key))
		{
			if (ContourData->ManuallyModifiedCurves.Contains(Curve.Key))
			{
				VisibleCurves.Add(Curve.Key, ReducedData[Curve.Key]);
			}
			else
			{
				VisibleCurves.Add(Curve);
			}
		}
	}

	return VisibleCurves;
}

TMap<FString, TArray<FVector2D>> FMetaHumanCurveDataController::GetCurveDisplayDataForVisualization() const
{
	TMap<FString, TArray<FVector2D>> VisibleCurves;

	for (const TPair<FString, TArray<FVector2D>>& Curve : ContourData->GetTrackingContourDataForDrawing())
	{
		if (CurveIsVisible(Curve.Key))
		{
			VisibleCurves.Add(Curve);
		}
	}

	return VisibleCurves;
}

FReducedContour FMetaHumanCurveDataController::GetReducedContourForTrackingContour(const TPair<FString, FTrackingContour>& InContour)
{
	const TArray<FVector2D>& DensePoints = InContour.Value.DensePoints;
	const FString& ContourName = InContour.Key;
	TArray<FControlVertex> ControlVertsForCurve;

	if (DensePoints.Num() == 1)
	{
		FControlVertex ControlVertex;
		ControlVertex.PointPosition = DensePoints.Last();

		TArray<FString> CurveNamesForEndPoints = GetCurveNamesForEndPoints(InContour.Key);
		if (!CurveNamesForEndPoints.IsEmpty())
		{
			ControlVertex.CurveNames = CurveNamesForEndPoints;
		}
		else
		{
			ControlVertex.CurveNames = { InContour.Key };
			ControlVertex.bIsSinglePointCurve = true;
		}

		ControlVertsForCurve.Add(ControlVertex);
	}
	else
	{
		const TArray<FVector2D>& ControlPoints = ShapeAnnotationWrapper.GetControlVerticesForCurve(DensePoints, ContourName, DisplayMode);

		for (const FVector2D& ControlPoint : ControlPoints)
		{
			FControlVertex ControlVertex;
			ControlVertex.PointPosition = ControlPoint;
			ControlVertex.CurveNames.Add(ContourName);
			ControlVertsForCurve.Add(ControlVertex);
		}
	}

	FReducedContour ReducedContour = { ControlVertsForCurve };
	return ReducedContour;
}

TArray<FVector2D> FMetaHumanCurveDataController::GetPointAtPosition(const FVector2D& InScreenPosition) const
{
	const UE::Geometry::TPolygon2<float> Poly = UE::Geometry::FPolygon2f::MakeCircle(PointSize, LinesPerCircle);
	TArray<FVector2D> Point;
	Point.Reserve(Poly.GetVertices().Num());
	for (const UE::Math::TVector2<float>& Vert : Poly.GetVertices())
	{
		Point.Add(FVector2D(Vert.X, Vert.Y) + InScreenPosition);
	}

	return Point;
}

#undef LOCTEXT_NAMESPACE

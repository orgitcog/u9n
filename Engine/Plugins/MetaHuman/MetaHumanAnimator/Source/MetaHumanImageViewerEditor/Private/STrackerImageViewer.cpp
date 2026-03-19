// Copyright Epic Games, Inc.All Rights Reserved.

#include "STrackerImageViewer.h"
#include "MetaHumanCurveDataController.h"

#include "Polygon2.h"
#include "Styling/AppStyle.h"

namespace
{
	TAutoConsoleVariable<bool> CVarShowFullCurve{
		TEXT("mh.Identity.ShowFullCurve"),
		false,
		TEXT("Show frame curves constructed with the full list of points"),
		ECVF_Default
	};
}

void STrackerImageViewer::Construct(const FArguments& InArgs)
{
	PointSize = 5;
	bShouldDrawPoints = InArgs._ShouldDrawPoints;
	bShouldDrawCurves = InArgs._ShouldDrawCurves;

	OnGeometryChanged.AddSP(this, &STrackerImageViewer::UpdateDisplayedDataForWidget);

	DefaultColor = FLinearColor::Green;
	HighlightedColor = FLinearColor::White;
	SelectedColor = FLinearColor::Yellow;
	DeactivatedColor = FLinearColor::Gray;

	SABImage::Construct(SABImage::FArguments()
							.Image(InArgs._Image));

	CVarShowFullCurve.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateSP(this, &STrackerImageViewer::UpdatePointPositionFullCurve));

	SetCursor(EMouseCursor::Default);
}

FReply STrackerImageViewer::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = FReply::Unhandled();

	if (!DragOperation.IsSet())
	{
		if (CanEditCurvesAndPoints() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !bIsPanning)
		{
			FVector2D MousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

			bool bMakingSelection = ResolveSelectionForMouseClick(InMouseEvent, MousePosition);
			bool bManipulationInitiated = SetManipulationStateForMouseClick(InMouseEvent, MousePosition);

			if (!bMakingSelection && !bManipulationInitiated)
			{
				ViewState.SelectedPointIds.Empty();
				ViewState.SelectedCurveNames.Empty();
			}

			CurveDataController->SetCurveSelection(ViewState.SelectedCurveNames, false);

			Reply = FReply::Handled();
		}
		else
		{
			Reply = SABImage::OnMouseButtonDown(InGeometry, InMouseEvent);
		}
	}

	if (Reply.IsEventHandled())
	{
		Reply.CaptureMouse(SharedThis(this));
	}

	return Reply;
}

FReply STrackerImageViewer::OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = FReply::Unhandled();

	bool bWasPanning = bIsPanning;

	if (DragOperation.IsSet())
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (DragOperation->IsDragging())
			{
				DragOperation->DragImpl->OnEndDrag();
			}

			DragOperation.Reset();

			Reply = FReply::Handled();
		}
	}
	else if (CanEditCurvesAndPoints() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !bIsPanning)
	{
		if (InMouseEvent.IsControlDown())
		{
			FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			AddRemoveKey(LocalMouse, true);
		}
		else if (InMouseEvent.IsAltDown())
		{
			FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			AddRemoveKey(LocalMouse, false);
		}

		Reply = FReply::Handled();
	}
	else
	{
		Reply = SABImage::OnMouseButtonUp(InGeometry, InMouseEvent);
	}

	// If we just stopped panning, update tracker points since they can stay in offset position
	// because OnMouseMove event is not in sync with widget paint which happens every tick.
	if (bWasPanning && !bIsPanning)
	{
		UpdateDisplayedDataForWidget();
	}

	if (Reply.IsEventHandled())
	{
		Reply.ReleaseMouseCapture();
	}

	return Reply;
}

FReply STrackerImageViewer::OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply RetVal = FReply::Handled();
	if (!bIsPanning && CanEditCurvesAndPoints())
	{
		FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		if (DragOperation.IsSet())
		{
			FVector2D InitialPosition = DragOperation->GetInitialPosition();

			if (!DragOperation->IsDragging() && DragOperation->AttemptDragStart(InMouseEvent))
			{
				DragOperation->DragImpl->OnBeginDrag(LocalMouse, InMouseEvent);
			}
			else if (DragOperation->IsDragging())
			{
				DragOperation->DragImpl->OnDrag(LocalMouse, InMouseEvent);
			}

			return RetVal;
		}
		else
		{
			ResolveHighlightingForMouseMove(LocalMouse);
		}
	}

	RetVal = SABImage::OnMouseMove(InGeometry, InMouseEvent);
	// UpdatePointPosition when panning should be called after Image Mouse Move as UV gets updated
	if (bIsPanning)
	{
		UpdateDisplayedDataForWidget();
	}

	return RetVal;
}

FReply STrackerImageViewer::OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply RetVal = SABImage::OnMouseWheel(InGeometry, InMouseEvent);

	UpdateDisplayedDataForWidget();

	return RetVal;
}

FReply STrackerImageViewer::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CanEditCurvesAndPoints())
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			ViewState.SelectedPointIds.Empty();
			return FReply::Handled();
		}
	}

	return SMetaHumanImageViewer::OnKeyDown(MyGeometry, InKeyEvent);
}

int32 STrackerImageViewer::OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry,
								   const FSlateRect& InWidgetClippingRect, FSlateWindowElementList& OutDrawElements, int32 InLayerId,
								   const FWidgetStyle& InWidgetStyle, bool bInParentEnabled) const
{
	int32 LayerId = SABImage::OnPaint(InArgs, InAllottedGeometry, InWidgetClippingRect, OutDrawElements, InLayerId, InWidgetStyle, bInParentEnabled);

	if (IsSingleView())
	{
		if (bShouldDrawPoints.Get())
		{
			DrawControlVertices(OutDrawElements, LayerId + 1, InAllottedGeometry);
		}

		if (bShouldDrawCurves.Get())
		{
			DrawTrackingCurves(OutDrawElements, LayerId + 1, InAllottedGeometry);

			if (CVarShowFullCurve->GetBool())
			{
				DrawTrackingCurvesFromAllPoints(OutDrawElements, LayerId + 1, InAllottedGeometry);
			}
		}

		if (DragOperation.IsSet())
		{
			DragOperation->DragImpl->OnDragOperationPaint(InAllottedGeometry, OutDrawElements, LayerId + 1);
		}
	}

	return LayerId;
}

void STrackerImageViewer::SetTrackerImageSize(const FIntPoint& InTrackerImageSize)
{
	TrackerImageSize = FVector2D(InTrackerImageSize.X, InTrackerImageSize.Y);
	UpdateDisplayedDataForWidget();
}

void STrackerImageViewer::ResetTrackerImageScreenRect(const FBox2D& InTrackerScreenRect)
{
	if (InTrackerScreenRect.bIsValid)
	{
		TrackerImageRect = InTrackerScreenRect;
	}
	else
	{
		TrackerImageRect = FBox2D{ FVector2D::ZeroVector, Geometry.GetLocalSize() };
	}

	UpdateDisplayedDataForWidget();
}

void STrackerImageViewer::UpdateCurveSelection(bool bClearPointSelection)
{
	ViewState.SelectedCurveNames.Empty();

	if (bClearPointSelection)
	{
		ViewState.SelectedPointIds.Empty();
	}

	if (CurveDataController.IsValid())
	{
		ViewState.SelectedCurveNames = CurveDataController->GetContourData()->GetSelectedCurves();

		for (const FString& Name : ViewState.SelectedCurveNames)
		{
			if (CurveDataController->GetContourData()->ContourIsVisible(Name))
			{
				ViewState.SelectedPointIds.Append(CurveDataController->GetPointIdsWithEndPointsForCurve(Name));
			}
		}

		UpdateDisplayedDataForWidget();
	}
}

void STrackerImageViewer::SetDataControllerForCurrentFrame(const TSharedPtr<FMetaHumanCurveDataController> InCurveDataController)
{
	if (CurveDataController.IsValid())
	{
		// Clean-up for delegates for controller of "incorrect" frame
		CurveDataController->GetCurvesSelectedDelegate().RemoveAll(this);
		CurveDataController->TriggerContourUpdate().RemoveAll(this);
		CurveDataController->ViewportPointSelectionRetrieverDelegate().Unbind();
	}

	ViewState = FViewerStatesForFrame();
	CurveDataController = InCurveDataController;

	if (CurveDataController)
	{
		CurveDataController->TriggerContourUpdate().AddSP(this, &STrackerImageViewer::UpdateDisplayedDataForWidget);
		CurveDataController->GetCurvesSelectedDelegate().AddSP(this, &STrackerImageViewer::UpdateCurveSelection);
		CurveDataController->ViewportPointSelectionRetrieverDelegate().BindSP(this, &STrackerImageViewer::GetViewportSelectedPointIds);
	}

	UpdateDisplayedDataForWidget();
}

void STrackerImageViewer::SetEditCurvesAndPointsEnabled(bool bInCanEdit)
{
	bCanEditPointsAndCurves = bInCanEdit;
}

void STrackerImageViewer::ResetView()
{
	SMetaHumanImageViewer::ResetView();
	UpdateDisplayedDataForWidget();
}

void STrackerImageViewer::UpdateDisplayedDataForWidget()
{
	if (CurveDataController.IsValid())
	{
		FBox2D UV = GetImageAttribute().Get()->GetUVRegion();
		FVector2D WidgetSize = GetPaintSpaceGeometry().GetLocalSize();

		ViewState.SplineDensePoints.Empty();
		ViewState.SplineDensePointsImageSpace = CurveDataController->GetDensePointsForVisibleCurves();
		for (TPair<FString, TArray<FVector2D>>& DensePoints : ViewState.SplineDensePointsImageSpace)
		{
			TArray<FVector2D> WidgetSpacePoints;
			for (const FVector2D& Point : DensePoints.Value)
			{
				WidgetSpacePoints.Add(GetPointPositionOnScreen(Point, UV, WidgetSize));
			}
			ViewState.SplineDensePoints.Add(DensePoints.Key, WidgetSpacePoints);
		}

		ViewState.ControlVerticesForDraw = CurveDataController->GetAllVisibleControlVertices();
		for (FControlVertex& Point : ViewState.ControlVerticesForDraw)
		{
			Point.PointPosition = GetPointPositionOnScreen(Point.PointPosition, UV, WidgetSize);
			Point.LinePoints = GetPointAtPosition(Point.PointPosition);
		}

		if (CVarShowFullCurve->GetBool())
		{
			UpdatePointPositionFullCurve(nullptr);
		}
	}
	else
	{
		ViewState.SplineDensePointsImageSpace.Empty();
		ViewState.SplineDensePoints.Empty();
		ViewState.ControlVerticesForDraw.Empty();
	}
}

// This function is only used as advanced comparison tool, when Cvar to show full curve is enabled
void STrackerImageViewer::UpdatePointPositionFullCurve(IConsoleVariable* InVar)
{
	if (CurveDataController.IsValid())
	{
		FBox2D UV = GetImageAttribute().Get()->GetUVRegion();
		FVector2D WidgetSize = GetPaintSpaceGeometry().GetLocalSize();

		ViewState.AllDensePointsForSplines.Empty();
		TMap<FString, TArray<FVector2D>> DenseSplinePoints = CurveDataController->GetFullSplineDataForVisibleCurves();
		for (const TPair<FString, TArray<FVector2D>>& DensePoints : DenseSplinePoints)
		{
			TArray<FVector2D> WidgetSpacePoints;
			for (const FVector2D& Point : DensePoints.Value)
			{
				WidgetSpacePoints.Add(GetPointPositionOnScreen(Point, UV, WidgetSize));
			}
			ViewState.AllDensePointsForSplines.Add(DensePoints.Key, WidgetSpacePoints);
		}
	}
}

void STrackerImageViewer::AddRemoveKey(const FVector2D& InMousePos, bool bAdd)
{
	TPair<FString, FVector2D> ClosestInsertionData = GetClosestInsertionPosition(InMousePos, DistanceToCurveForHighlighting);

	if (!ClosestInsertionData.Key.IsEmpty())
	{
		if (CurveDataController->AddRemoveKey(ClosestInsertionData.Value, ClosestInsertionData.Key, bAdd))
		{
			UpdateCurveSelection(false);
		}
	}
}

bool STrackerImageViewer::CanEditCurvesAndPoints() const
{
	return CurveDataController && bCanEditPointsAndCurves && IsSingleView() && GetNavigationMode() == EABImageNavigationMode::TwoD;
}

FVector2D STrackerImageViewer::GetPointPositionOnScreen(const FVector2D& InImagePosition, const FBox2D& InUV, const FVector2D& InWidgetSize) const
{
	// InImagePosition is tracker position in the coordinates of the image used to track. The aspect ratio of that
	// tracked image is not the same as the image displayed on screen. Therefore we can not simply map the top-left of the
	// tracked image to the top-left of the displayed image and similarly with bottom-right. The two images do however have the
	// same center point and horizontal field of view. We can map the center of the tracked image to the center of the displayed image
	// and scale by the ratio of the 2 images widths. 
	FVector2D OffsetPos = InImagePosition;							// Tracker position in tracker image space relative to top-left corner of tracker image
	OffsetPos -= TrackerImageSize / 2.0;							// Tracker position in tracker image space relative to center of tracker image
	OffsetPos *= TrackerImageRect.GetSize().X / TrackerImageSize.X; // Tracker position in widget space relative to center of tracker image rectangle
	OffsetPos += TrackerImageRect.GetCenter();						// Tracker position in widget space relative to top-left corner of widget
	// Account for pan/zoom
	OffsetPos /= InWidgetSize;
	OffsetPos -= InUV.Min;
	OffsetPos *= InWidgetSize / (InUV.Max - InUV.Min);
	return OffsetPos;
}

FVector2D STrackerImageViewer::GetPointPositionOnImage(const FVector2D& InScreenPosition, bool bUseImageUV) const
{
	// Inverse of what GetPointPositionOnScreen does
	FBox2D UV = FBox2D(FVector2D(0.0, 0.0), FVector2D(1.0, 1.0));

	if (bUseImageUV)
	{
		UV = GetImageAttribute().Get()->GetUVRegion();
	}

	FVector2D WidgetSize = GetPaintSpaceGeometry().GetLocalSize();
	FVector2D OffsetPos = InScreenPosition;

	OffsetPos /= WidgetSize / (UV.Max - UV.Min);
	OffsetPos += UV.Min;
	OffsetPos *= WidgetSize;

	OffsetPos -= TrackerImageRect.GetCenter();
	OffsetPos /= TrackerImageRect.GetSize().X / TrackerImageSize.X;
	OffsetPos += TrackerImageSize / 2.0;

	return OffsetPos;
}

TSet<int32>& STrackerImageViewer::GetViewportSelectedPointIds()
{
	return ViewState.SelectedPointIds;
}

FLinearColor STrackerImageViewer::GetPointColor(const int32 InPointID) const
{
	// Highlighted point should override any previously set colors
	if(InPointID == ViewState.HighlightedPointID)
	{
		return HighlightedColor;
	}
	
	FLinearColor Color = DefaultColor;
	TArray<FString> CurveNames = CurveDataController->GetCurveNamesForPointId(InPointID);
	bool bActive = false;

	for (const FString& CurveName : CurveNames)
	{
		const TPair<bool, bool> SelectedAndActiveStatus = CurveDataController->GetCurveSelectedAndActiveStatus(CurveName);
		bActive |= SelectedAndActiveStatus.Value;
	}
	
	if(ViewState.SelectedPointIds.Contains(InPointID))
	{
		Color = SelectedColor;
	}
	else if(!bActive)
	{
		Color = DeactivatedColor;
	}
	
	return Color;
}

FLinearColor STrackerImageViewer::GetCurveColor(const FString& InCurveName) const
{
	FLinearColor Color = DefaultColor;
	const TPair<bool, bool> SelectedActiveState = CurveDataController->GetCurveSelectedAndActiveStatus(InCurveName);
	
	if(InCurveName == ViewState.HighlightedCurveName)
	{
		Color = HighlightedColor;
	}
	else if(SelectedActiveState.Key)
	{
		Color = SelectedColor;
	}
	else if(!SelectedActiveState.Value)
	{
		Color = DeactivatedColor;
	}

	return Color;
}

TPair<FString, FVector2D> STrackerImageViewer::GetClosestInsertionPosition(const FVector2D& InMousePosition, const float InDistanceToCheck) const
{
	FString CurveName;
	FVector2D ClosestCurveInsertionPoint;
	float RefinedSmallestDistance = TNumericLimits<float>::Max();

	FVector2D MouseImagePosition = GetPointPositionOnImage(InMousePosition);
	TMap<FString, int32> ClosestPoints = GetClosestSplinePointOnCurves(InMousePosition, InDistanceToCheck);
	
	for (const TPair<FString, int32>& PairData : ClosestPoints)
	{
		TArray<FVector2D> Points = ViewState.SplineDensePointsImageSpace[PairData.Key];
		int32 Itr = PairData.Value;

		if (Itr > 0)
		{
			const FVector2D ClosestPointToSegment = FMath::ClosestPointOnSegment2D(MouseImagePosition, Points[Itr - 1], Points[Itr]);
			const double Distance = FVector2D::Distance(MouseImagePosition, ClosestPointToSegment);

			if (Distance < RefinedSmallestDistance && Distance < InDistanceToCheck)
			{
				RefinedSmallestDistance = Distance;
				CurveName = PairData.Key;
				ClosestCurveInsertionPoint = ClosestPointToSegment;
			}
		}
		if (Itr < Points.Num() - 1)
		{
			const FVector2D ClosestPointToSegment = FMath::ClosestPointOnSegment2D(MouseImagePosition, Points[Itr + 1], Points[Itr]);
			const double Distance = FVector2D::Distance(MouseImagePosition, ClosestPointToSegment);

			if (Distance < RefinedSmallestDistance && Distance < InDistanceToCheck)
			{
				RefinedSmallestDistance = Distance;
				CurveName = PairData.Key;
				ClosestCurveInsertionPoint = ClosestPointToSegment;
			}
		}
	}

	return { CurveName, ClosestCurveInsertionPoint };
}

TMap<FString, int32> STrackerImageViewer::GetClosestSplinePointOnCurves(const FVector2D& InMousePosition, const float InDistanceToCheck) const
{
	TMap<FString, int32> ClosestPoints;

	// Checking distance between mouse and curve data in image space 
	FVector2D MouseImagePosition = GetPointPositionOnImage(InMousePosition);
	for (const TPair<FString, TArray<FVector2D>>& DenseCurveData : ViewState.SplineDensePointsImageSpace)
	{
		float SmallestDistance = TNumericLimits<float>::Max();
		int32 ClosestPointIterator = -1;
		int32 PointIterator = -1;

		for (const FVector2D& Point : DenseCurveData.Value)
		{
			++PointIterator;
			double CalcDistance = FVector2D::Distance(MouseImagePosition, Point);
			if (CalcDistance < SmallestDistance)
			{
				SmallestDistance = CalcDistance;
				ClosestPointIterator = PointIterator;
			}
		}

		if (ClosestPointIterator != -1)
		{
			ClosestPoints.Add(DenseCurveData.Key, ClosestPointIterator);
		}
	}

	return ClosestPoints;
}

void STrackerImageViewer::DrawControlVertices(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& AllottedGeometry) const
{
	FPaintGeometry MyGeometry = AllottedGeometry.ToPaintGeometry();
	FSlateFontInfo SummaryFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	for (const FControlVertex& Point : ViewState.ControlVerticesForDraw)
	{
		FLinearColor Color = GetPointColor(Point.PointId);
		FSlateDrawElement::MakeLines(OutDrawElements, InLayerId, MyGeometry, Point.LinePoints, ESlateDrawEffect::None, Color);
	}
}

void STrackerImageViewer::DrawTrackingCurves(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& AllottedGeometry) const
{
	FPaintGeometry MyGeometry = AllottedGeometry.ToPaintGeometry();

	for(const TPair<FString, TArray<FVector2D>>& Points : ViewState.SplineDensePoints)
	{
		if(Points.Value.Num() > 1)
		{
			FLinearColor Color = GetCurveColor(Points.Key);
			FSlateDrawElement::MakeLines(OutDrawElements, InLayerId, MyGeometry, Points.Value, ESlateDrawEffect::None, Color);
		}
	}
}

// This function is only used as advanced comparison tool, when Cvar to show full curve is enabled
void STrackerImageViewer::DrawTrackingCurvesFromAllPoints(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& AllottedGeometry) const
{
	FPaintGeometry MyGeometry = AllottedGeometry.ToPaintGeometry();

	for (const TPair<FString, TArray<FVector2D>>& Points : ViewState.AllDensePointsForSplines)
	{
		if (Points.Value.Num() > 1)
		{
			FLinearColor Color = FLinearColor::Blue;
			FSlateDrawElement::MakeLines(OutDrawElements, InLayerId, MyGeometry, Points.Value, ESlateDrawEffect::None, Color);
		}
	}
}

void STrackerImageViewer::ResolveHighlightingForMouseMove(const FVector2D& InMousePosition)
{
	ViewState.HighlightedCurveName.Empty();
	ViewState.HighlightedPointID = 0;

	bool bCheckCurveHighlight = bShouldDrawCurves.Get();
	if(bShouldDrawPoints.Get())
	{
		bCheckCurveHighlight &= !SetHighlightingFromPoint(InMousePosition);
	}

	if(bCheckCurveHighlight)
	{
		SetHighlightingFromCurve(InMousePosition);
	}
}

void STrackerImageViewer::ResolveSelectionFromMarquee(const FSlateRect& InSelectionMarquee)
{
	ViewState.SelectedPointIds.Empty();
	ViewState.SelectedCurveNames.Empty();

	if (bShouldDrawCurves.Get() || bShouldDrawPoints.Get())
	{
		for (const FControlVertex& Point : ViewState.ControlVerticesForDraw)
		{
			if (InSelectionMarquee.ContainsPoint(Point.PointPosition))
			{
				ViewState.SelectedPointIds.Add(Point.PointId);
			}
		}

		CurveDataController->ResolveCurveSelectionFromSelectedPoints(ViewState.SelectedPointIds);
	}
}

void STrackerImageViewer::PopulateSelectionListForMouseClick()
{
	bool bSelectionHandledForCurves = false;
	if(ViewState.SelectedCurveNames.Contains(ViewState.HighlightedCurveName))
	{
		ViewState.SelectedCurveNames.Remove(ViewState.HighlightedCurveName);
		for(const int32 ID : CurveDataController->GetPointIdsWithEndPointsForCurve((ViewState.HighlightedCurveName)))
		{
			ViewState.SelectedPointIds.Remove(ID);
		}
		bSelectionHandledForCurves = true;
	}
	else if(!ViewState.HighlightedCurveName.IsEmpty())
	{
		ViewState.SelectedCurveNames.Add(ViewState.HighlightedCurveName);
		ViewState.SelectedPointIds.Append(CurveDataController->GetPointIdsWithEndPointsForCurve(ViewState.HighlightedCurveName));

		bSelectionHandledForCurves = true;
	}

	if(!bSelectionHandledForCurves)
	{
		if(ViewState.SelectedPointIds.Contains(ViewState.HighlightedPointID))
		{
			ViewState.SelectedPointIds.Remove(ViewState.HighlightedPointID);
			for(const FString& CurveName : CurveDataController->GetCurveNamesForPointId(ViewState.HighlightedPointID))
			{
				ViewState.SelectedCurveNames.Remove(CurveName);
			}
		}
		else if(ViewState.HighlightedPointID > 0)
		{
			ViewState.SelectedPointIds.Add(ViewState.HighlightedPointID);
			CurveDataController->ResolveCurveSelectionFromSelectedPoints(ViewState.SelectedPointIds);
		}
	}	
}

TArray<FVector2D> STrackerImageViewer::GetPointAtPosition(const FVector2D& InScreenPosition) const
{
	const UE::Geometry::TPolygon2<float> Poly = UE::Geometry::FPolygon2f::MakeCircle(PointSize, LinesPerCircle);
	TArray<FVector2D> Point;
    Point.Reserve(Poly.GetVertices().Num());
	for(const UE::Math::TVector2<float>& Vert : Poly.GetVertices())
	{
		Point.Add(FVector2D(Vert.X, Vert.Y) + InScreenPosition);
	}

	return Point;
}

bool STrackerImageViewer::SetHighlightingFromPoint(const FVector2D& InMousePos)
{
	for(const FControlVertex& Point : ViewState.ControlVerticesForDraw)
	{
		if(FVector2D::Distance(InMousePos, Point.PointPosition) < PointSize + 1)
		{
			ViewState.HighlightedPointID = Point.PointId;

			if (Point.bIsSinglePointCurve)
			{
				ViewState.HighlightedCurveName = Point.CurveNames.Last();
			}
			
			return true;
		}
	}

	return false;
}

bool STrackerImageViewer::SetHighlightingFromCurve(const FVector2D& InMousePos)
{
	bool bSuccess = false;
	TPair<FString, FVector2D> CurveInsertionData = GetClosestInsertionPosition(InMousePos, DistanceToCurveForHighlighting);

	if (!CurveInsertionData.Key.IsEmpty())
	{
		ViewState.HighlightedCurveName = CurveInsertionData.Key;
		bSuccess = true;
	}

	return bSuccess;
}

bool STrackerImageViewer::ResolveSelectionForMouseClick(const FPointerEvent& InMouseEvent, const FVector2D& InMousePos)
{
	bool bIsSelecting = false;

	if(InMouseEvent.IsShiftDown())
	{
		PopulateSelectionListForMouseClick();
		bIsSelecting = true;
	}
	else if(!ViewState.HighlightedCurveName.IsEmpty() && !ViewState.SelectedCurveNames.Contains(ViewState.HighlightedCurveName))
	{
		ViewState.SelectedCurveNames.Empty();
		ViewState.SelectedPointIds.Empty();
		
		ViewState.SelectedCurveNames.Add(ViewState.HighlightedCurveName);
		ViewState.SelectedPointIds.Append(CurveDataController->GetPointIdsWithEndPointsForCurve(ViewState.HighlightedCurveName));

		bIsSelecting = true;
	}
	else if(ViewState.HighlightedPointID > 0 && !ViewState.SelectedPointIds.Contains(ViewState.HighlightedPointID))
	{
		ViewState.SelectedCurveNames.Empty();
		ViewState.SelectedPointIds.Empty();
		ViewState.SelectedPointIds.Add(ViewState.HighlightedPointID);
		bIsSelecting = true;
	}
	
	return bIsSelecting;
}

bool STrackerImageViewer::SetManipulationStateForMouseClick(const FPointerEvent& InMouseEvent, const FVector2D& InMousePos)
{
	bool bClickedOnSelectedCurve = ViewState.SelectedCurveNames.Contains(ViewState.HighlightedCurveName);
	bool bClickedOnSelectedPoint = ViewState.SelectedPointIds.Contains(ViewState.HighlightedPointID);
	bool bMovingPoints = (bClickedOnSelectedCurve || bClickedOnSelectedPoint) && !InMouseEvent.IsControlDown() && !InMouseEvent.GetModifierKeys().AnyModifiersDown();
	bool bMovingSelectionMarque = !bMovingPoints && InMouseEvent.IsShiftDown();

	if (bMovingPoints)
	{
		TUniquePtr<FMetaHumanPointDragOperation> KeyDrag = MakeUnique<FMetaHumanPointDragOperation>();
		KeyDrag->Initialize(CurveDataController, ViewState.SelectedPointIds);
		KeyDrag->OnGetPostionOnImageDelegate.BindSP(this, &STrackerImageViewer::GetPointPositionOnImage);

		DragOperation = FMetaHumanCurveEditorDelayedDrag(InMousePos, InMouseEvent.GetEffectingButton());
		DragOperation->DragImpl = MoveTemp(KeyDrag);
	}
	else if (bMovingSelectionMarque)
	{
		TUniquePtr<FMetaHumanMarqueSelectOperation> DrawMarque = MakeUnique<FMetaHumanMarqueSelectOperation>();
		DrawMarque->Initialize(CurveDataController);
		DrawMarque->ResolveMarqueeSelectionDelegate.BindSP(this, &STrackerImageViewer::ResolveSelectionFromMarquee);

		DragOperation = FMetaHumanCurveEditorDelayedDrag(InMousePos, InMouseEvent.GetEffectingButton());
		DragOperation->DragImpl = MoveTemp(DrawMarque);
	}

	return bMovingPoints|| bMovingSelectionMarque;
}

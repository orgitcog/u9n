// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "SABImage.h"
#include "MetaHumanContourData.h"
#include "MetaHumanCurveDragOperations.h"

struct FViewerStatesForFrame
{
	/** Dense points used to draw the curves as lines between the points */
	TMap<FString, TArray<FVector2D>> SplineDensePoints;

	/** Local copy of dense points in Image Space as is original contour data */
	TMap<FString, TArray<FVector2D>> SplineDensePointsImageSpace;

	// This member is only used as advanced comparison tool, when Cvar to show full curve is enabled
	TMap<FString, TArray<FVector2D>> AllDensePointsForSplines;

	/** Local copy of control vertices in widget space for drawing */
	TArray<FControlVertex> ControlVerticesForDraw;

	/** A list of selected points on visible curves */
	TSet<int32> SelectedPointIds;

	/** A list of selected curves. Should match the outliner */
	TSet<FString> SelectedCurveNames;

	/** A curve the mouse is hovering over */
	FString HighlightedCurveName;

	/** A point the mouse is hovering over */
	int32 HighlightedPointID = 0;
};

class METAHUMANIMAGEVIEWEREDITOR_API STrackerImageViewer
	: public SABImage
{
public:

	SLATE_BEGIN_ARGS(STrackerImageViewer)
		: _ShouldDrawPoints(true)
		, _ShouldDrawCurves(true)
	{}
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)

		/** If the curves and points should be drawn */
		SLATE_ATTRIBUTE(bool, ShouldDrawPoints)

		/** */
		SLATE_ATTRIBUTE(bool, ShouldDrawCurves)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual int32 OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry,
		const FSlateRect& InWidgetClippingRect, FSlateWindowElementList& OutDrawElements,
		int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const override;

	/** Sets the size of the underlying tracker image. Used to calculate the correct placement of curves and points in the image being displayed */
	void SetTrackerImageSize(const FIntPoint& InTrackerImageSize);

	/** Set the tracker image screen rect used to place the tracking contours in the correct location on screen. By default this will be the entire widget area */
	void ResetTrackerImageScreenRect(const FBox2D& InTrackerScreenRect = FBox2D{ ForceInit });

	/** A function connected to a callback in controller when curve selection is changed */
	void UpdateCurveSelection(bool bClearPointSelection);

	void SetDataControllerForCurrentFrame(TSharedPtr<class FMetaHumanCurveDataController> InShapeAnnotation);

	/** Sets whether or not the points and curves can be edited user interaction */
	void SetEditCurvesAndPointsEnabled(bool bInCanEdit);

	/** */
	virtual void ResetView() override;

	/** Update visual data for points and curves on this widget from underlying contour data */
	void UpdateDisplayedDataForWidget();

	/** A wrapper around Update point position for CVar */
	void UpdatePointPositionFullCurve(IConsoleVariable* InVar);

	/** Returns image coordinates for specified screen position */
	FVector2D GetPointPositionOnImage(const FVector2D& InScreenPosition, bool bUseImageUV = true) const;

	/** Returns a reference to selected points ids stored in the viewport */
	TSet<int32>& GetViewportSelectedPointIds();

protected:

	/** */
	bool CanEditCurvesAndPoints() const;

	FVector2D GetPointPositionOnScreen(const FVector2D& InOriginalPosition, const FBox2D& InUV, const FVector2D& InWidgetSize) const;
	FLinearColor GetPointColor(const int32 InPointID) const;
	FLinearColor GetCurveColor(const FString& InCurveName) const;

	/** Returns a curve name and closest position on that curve to mouse pos. OutParam is the closest index on dense spline */
	TPair<FString, FVector2D> GetClosestInsertionPosition(const FVector2D& InMousePosition, const float InDistanceToCheck) const;

	/** Returns the map of curves with corresponding dense spline point ID within a check distance from mouse */
	TMap<FString, int32> GetClosestSplinePointOnCurves(const FVector2D& InMousePosition, const float InDistanceToCheck) const;

	void DrawControlVertices(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& AllottedGeometry) const;
	void DrawTrackingCurves(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& AllottedGeometry) const;
	void DrawTrackingCurvesFromAllPoints(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& AllottedGeometry) const;

	void ResolveHighlightingForMouseMove(const FVector2D& InMousePosition);
	void ResolveSelectionFromMarquee(const FSlateRect& InSelectionMarquee);
	void PopulateSelectionListForMouseClick();
	void AddRemoveKey(const FVector2D& InMousePos, bool bAdd);

	TArray<FVector2D> GetPointAtPosition(const FVector2D& InScreenPosition) const;
	bool SetHighlightingFromPoint(const FVector2D& InMousePos);
	bool SetHighlightingFromCurve(const FVector2D& InMousePos);
	bool ResolveSelectionForMouseClick(const FPointerEvent& InMouseEvent, const FVector2D& InMousePos);
	bool SetManipulationStateForMouseClick(const FPointerEvent& InMouseEvent, const FVector2D& InMousePos);

	bool bCanEditPointsAndCurves = true;
	TAttribute<bool> bShouldDrawPoints;
	TAttribute<bool> bShouldDrawCurves;

	FBox2D TrackerImageRect;
	FVector2D TrackerImageSize;

	TOptional<FMetaHumanCurveEditorDelayedDrag> DragOperation;
	TSharedPtr<class FMetaHumanCurveDataController> CurveDataController;
	FViewerStatesForFrame ViewState;

	// Set of default values for visual representation of curves and points

	FLinearColor DefaultColor;
	FLinearColor HighlightedColor;
	FLinearColor SelectedColor;
	FLinearColor DeactivatedColor;

	const int32 LinesPerCircle = 33;
	int32 PointSize;

	double MouseMoveLastTime = 0;
	double MouseMoveElapsed = 0;
	static constexpr float DistanceToCurveForHighlighting = 5.0;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetaHumanContourData.h"
#include "ShapeAnnotationWrapper.h"
#include "Framework/DelayedDrag.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCurvesSelectedDelegate, bool bClearPointSelection)
DECLARE_DELEGATE_RetVal(TSet<int32>&, FOnGetViewportPointSelectionDelegate)

class METAHUMANCORE_API FMetaHumanCurveDataController
{
public:

	FMetaHumanCurveDataController(TObjectPtr<UMetaHumanContourData> InCurveData, ECurveDisplayMode InMode = ECurveDisplayMode::Editing);

	/** Sets up the curve list from config along with default data to be displayed */
	void InitializeContoursFromConfig(const FFrameTrackingContourData& InDefaultContourData, const FString& InConfigVersion);

	/** Updates the tracking contour data present in config & relevant data to display those curves */
	void UpdateFromContourData(const FFrameTrackingContourData& InTrackingData, const bool bUpdateVisibility);

	/** Updates individual curves, keeping reduced data of other curves intact */
	void UpdateIndividualCurves(const FFrameTrackingContourData& InTrackingData);

	/** Removes all contour data, invalidating the initialization from config */
	void ClearContourData();

	/** Moves selected points by a provided offset */
	void OffsetSelectedPoints(const TSet<int32>& InSelectedPoints, const FVector2D& InOffset);

	/** Moves a single point to a mouse cursor in image space */
	void MoveSelectedPoint(const FVector2D& InNewPosition, const int32 InPointId);

	/** Update the original dense points data to represent the modified curve */
	void UpdateDensePointsAfterDragging(const TSet<int32>& InDraggedIds);

	/** Updates the selection of contour data & emits the signal for relevant updates */
	void SetCurveSelection(const TSet<FString>& InSelectedCurves, bool bClearPointSelection);

	/** Updates the selection of contour data based of individually selected points */
	void ResolveCurveSelectionFromSelectedPoints(const TSet<int32>& InSelectedPoints);

	/** Triggers relevant updates to draw data after the undo operation */
	void HandleUndoOperation();

	/** Clears displayed data but keeps controller initialization with whatever last data was set */
	void ClearDrawData();

	/** Resolves end point selection when these points belong to multiple curves */
	void ResolvePointSelectionOnCurveVisibilityChanged(const TArray<FString>& InCurveNames, bool bInSingleCurve, bool bInIsHiding);

	/** Checks if the curve is selected or active */
	TPair<bool, bool> GetCurveSelectedAndActiveStatus(const FString& InCurve);

	void GenerateCurvesFromControlVertices();
	void GenerateDrawDataForDensePoints();

	/** Scoped operation for adding or removing the key */
	bool AddRemoveKey(const FVector2D& InPointPosition, const FString& InCurveName, bool bInAdd);

	TArray<FString> GetCurveNamesForPointId(const int32 InPointId);
	TArray<int32> GetPointIdsWithEndPointsForCurve(const FString& InCurveName) const;
	TMap<FString, TArray<FVector2D>> GetDensePointsForVisibleCurves() const;
	TMap<FString, TArray<FVector2D>> GetFullSplineDataForVisibleCurves() const;
	TArray<FControlVertex> GetAllVisibleControlVertices();

	FSimpleMulticastDelegate& TriggerContourUpdate() { return UpdateContourDelegate; }
	FOnCurvesSelectedDelegate& GetCurvesSelectedDelegate() { return OnCurvesSelectedDelegate; }
	FOnGetViewportPointSelectionDelegate& ViewportPointSelectionRetrieverDelegate() { return OnGetViewportPointSelection; }

	const TObjectPtr<UMetaHumanContourData> GetContourData() { return ContourData; }

private:

	void CreateControlVertices();
	void RecreateControlVertexIds();
	void RecreateCurvesFromReducedData();
	void ClearCurveSelection();
	void GenerateCurveDataPostTrackingDataChange();
	void ModifyViewportEndPointSelectionForCurveVisibility(const FString& InCurveName, const FString& InEndPointName);

	bool CurveIsVisible(const FString& InCurveName) const;
	int32 GetCurveInsertionIndex(const FVector2D& InInsertionPos, const FString& InCurveName);
	float GetDistanceToNearestVertex(const FVector2D& InPosition, const FString& InCurveName, int32& outIndex);

	TArray<FString> GetCurveNamesForEndPoints(const FString& InEndPointName) const;
	TArray<FVector2D> GetPointAtPosition(const FVector2D& InScreenPosition) const;
	TMap<FString, TArray<FVector2D>> GetCurveDisplayDataForEditing() const;
	TMap<FString, TArray<FVector2D>> GetCurveDisplayDataForVisualization() const;
	FReducedContour GetReducedContourForTrackingContour(const TPair<FString, FTrackingContour>& InContour);

	TObjectPtr<UMetaHumanContourData> ContourData;	
	FShapeAnnotationWrapper ShapeAnnotationWrapper;

	static constexpr int32 LinesPerCircle = 33;
	static constexpr int32 PointSize = 5;
	static constexpr float CurveAddRemoveThreshold = 2.5f;
	static constexpr float SelectionCaptureRange = 40.0f;

	const ECurveDisplayMode DisplayMode;
	FSimpleMulticastDelegate UpdateContourDelegate;
	FOnCurvesSelectedDelegate OnCurvesSelectedDelegate;
	FOnGetViewportPointSelectionDelegate OnGetViewportPointSelection;
};

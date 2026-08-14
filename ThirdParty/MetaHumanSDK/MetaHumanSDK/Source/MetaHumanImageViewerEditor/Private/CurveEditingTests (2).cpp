// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "STrackerImageViewer.h"
#include "MetaHumanCurveDataController.h"

class STrackerImageViewerTest : public STrackerImageViewer
{
public:
	const TMap<FString, TArray<FVector2D>>& SplineDensePoints() { return STrackerImageViewer::ViewState.SplineDensePoints; }
	const TArray<FControlVertex>& ControlVerticesOnAllSplines() { return STrackerImageViewer::ViewState.ControlVerticesForDraw; }
	const TMap<FString, TArray<FVector2D>>& SplineDensePointsImageSpace() { return STrackerImageViewer::ViewState.SplineDensePointsImageSpace; }
	const FString& GetHighlightCurveName() { return STrackerImageViewer::ViewState.HighlightedCurveName; }
	const int32& GetHighlightPointID() { return STrackerImageViewer::ViewState.HighlightedPointID; }
	const TSet<int32>& GetSelectedPointIds() { return STrackerImageViewer::ViewState.SelectedPointIds; }
	const TSet<FString>& GetSelectedCurves() { return STrackerImageViewer::ViewState.SelectedCurveNames; }

	void SetUVRegion(FBox2d InUV) 
	{
		FSlateBrush* CurrentSlateBrush = const_cast<FSlateBrush*>(STrackerImageViewer::GetImageAttribute().Get());
		CurrentSlateBrush->SetUVRegion(InUV);
		UpdateDisplayedDataForWidget();
	}

	void ResolveHighlightingForTesting(const FVector2D& InMousePosition) { STrackerImageViewer::ResolveHighlightingForMouseMove(InMousePosition); };
	void ResolveSelectionForTesting(const FVector2D& InMousePosition) { STrackerImageViewer::ResolveSelectionForMouseClick(FPointerEvent(), InMousePosition); }
	void PopulateSelectionForTesting() { STrackerImageViewer::PopulateSelectionListForMouseClick(); }
	void ResolveAddRemoveForTesting(const FVector2D& InMousePos, bool bAdd) { STrackerImageViewer::AddRemoveKey(InMousePos, bAdd); }

	FFrameTrackingContourData GetSyntheticContourData();
};

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMetaHumanCurveEditingTest, "MetaHuman.CurveEditing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FMetaHumanCurveEditingTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	FString RandomTestName = "All";

	OutBeautifiedNames.Add(RandomTestName);
	OutTestCommands.Add(RandomTestName);
}

FFrameTrackingContourData STrackerImageViewerTest::GetSyntheticContourData()
{
	FFrameTrackingContourData SyntheticData;

	FTrackingContour ContourBrowLowerR;
	ContourBrowLowerR.DensePoints = {
		FVector2d(329.67004394531250, 577.97338867187500), FVector2d(324.19082641601562, 578.32299804687500), FVector2d(318.83956909179688, 577.81451416015625),
		FVector2d(313.38049316406250, 576.94763183593750), FVector2d(307.87203979492188, 575.93103027343750), FVector2d(302.47512817382812, 574.88092041015625),
		FVector2d(297.30847167968750, 573.66888427734375), FVector2d(292.17279052734375, 572.57336425781250), FVector2d(286.92861938476562, 571.71990966796875),
		FVector2d(281.54962158203125, 570.99481201171875), FVector2d(276.11950683593750, 570.47888183593750), FVector2d(270.63470458984375, 570.16876220703125),
		FVector2d(265.09732055664062, 570.20324707031250), FVector2d(259.65829467773438, 570.44165039062500), FVector2d(254.24856567382812, 571.12921142578125),
		FVector2d(248.93045043945312, 572.35034179687500), FVector2d(243.73986816406250, 573.93267822265625), FVector2d(238.54272460937500, 575.85516357421875),
		FVector2d(233.34692382812500, 577.90850830078125), FVector2d(228.20697021484375, 579.21087646484375) };
	ContourBrowLowerR.StartPointName = "pt_brow_intermediate_r";
	ContourBrowLowerR.EndPointName = "pt_brow_outer_r";
	ContourBrowLowerR.State.bVisible = true;

	FTrackingContour ContourBrowIntermidiateR;
	ContourBrowIntermidiateR.DensePoints = {
		FVector2D(333.50833129882812, 559.82629394531250), FVector2D(333.59500122070312, 563.65576171875000), FVector2D(333.68914794921875, 567.53808593750000),
		FVector2D(333.40612792968750, 571.44451904296875), FVector2D(332.32318115234375, 575.10467529296875), FVector2D(329.67004394531250, 577.97338867187500) };
	ContourBrowIntermidiateR.StartPointName = "pt_brow_inner_r";
	ContourBrowIntermidiateR.EndPointName = "pt_brow_intermediate_r";
	ContourBrowIntermidiateR.State.bVisible = true;

	FTrackingContour ContourBrowUpperR;
	ContourBrowUpperR.DensePoints = {
		FVector2D(333.50833129882812, 559.82629394531250), FVector2D(328.96749877929688, 558.45086669921875), FVector2D(324.16778564453125, 557.11322021484375),
		FVector2D(319.37722778320312, 555.85107421875000), FVector2D(314.59271240234375, 554.65625000000000), FVector2D(309.71328735351562, 553.53363037109375),
		FVector2D(304.74124145507812, 552.43603515625000), FVector2D(299.80822753906250, 551.30389404296875), FVector2D(294.87979125976562, 550.08843994140625),
		FVector2D(289.95217895507812, 548.88195800781250), FVector2D(284.99456787109375, 547.79992675781250), FVector2D(280.04357910156250, 546.89660644531250),
		FVector2D(275.09033203125000, 546.28887939453125), FVector2D(270.14712524414062, 546.13323974609375), FVector2D(265.25827026367188, 546.42706298828125),
		FVector2D(260.40686035156250, 547.25274658203125), FVector2D(255.74819946289062, 548.57653808593750), FVector2D(251.33450317382812, 550.45654296875000),
		FVector2D(247.16979980468750, 552.93878173828125), FVector2D(243.20419311523438, 555.89953613281250), FVector2D(239.44708251953125, 559.25854492187500),
		FVector2D(235.88970947265625, 563.16815185546875), FVector2D(232.77966308593750, 567.84234619140625), FVector2D(230.30749511718750, 573.46575927734375),
		FVector2D(228.20697021484375, 579.21087646484375) };
	ContourBrowUpperR.StartPointName = "pt_brow_inner_r";
	ContourBrowUpperR.EndPointName = "pt_brow_outer_r";
	ContourBrowUpperR.State.bVisible = true;

	FTrackingContour PtInnerR;
	PtInnerR.DensePoints = { FVector2D(333.50833129882812, 559.82629394531250) };
	PtInnerR.State.bVisible = true;
	FTrackingContour PtIntermediateR;
	PtIntermediateR.DensePoints = { FVector2D(329.67004394531250, 577.97338867187500) };
	PtIntermediateR.State.bVisible = true;
	FTrackingContour PtOuterR;
	PtOuterR.DensePoints = { FVector2D(228.20697021484375, 579.21087646484375) };
	PtOuterR.State.bVisible = true;

	SyntheticData.TrackingContours.Add("crv_brow_lower_r", ContourBrowLowerR);
	SyntheticData.TrackingContours.Add("crv_brow_intermediate_r", ContourBrowIntermidiateR);
	SyntheticData.TrackingContours.Add("crv_brow_upper_r", ContourBrowUpperR);
	SyntheticData.TrackingContours.Add("pt_brow_outer_r", PtOuterR);
	SyntheticData.TrackingContours.Add("pt_brow_intermediate_r", PtIntermediateR);
	SyntheticData.TrackingContours.Add("pt_brow_inner_r", PtInnerR);

	return SyntheticData;
}

bool FMetaHumanCurveEditingTest::RunTest(const FString& Parameters)
{
	TSharedPtr<STrackerImageViewerTest> TrackerImageViewer;
	SAssignNew(TrackerImageViewer, STrackerImageViewerTest);
	TrackerImageViewer->Setup(false);

	FFrameTrackingContourData SyntheticData = TrackerImageViewer->GetSyntheticContourData();

	TObjectPtr<UMetaHumanContourData> ContourData = NewObject<UMetaHumanContourData>();
	TSharedPtr<FMetaHumanCurveDataController> ContourDataController = MakeShared<FMetaHumanCurveDataController>(ContourData);

	const FString ConfigVersion = "0.0";
	ContourDataController->InitializeContoursFromConfig(SyntheticData, ConfigVersion);

	FBox2D FootageScreenRect;
	FootageScreenRect.Min = { 290.87261962890625, -0.56385308504104614 };
	FootageScreenRect.Max = { 595.52435302734375, 540.43609619140625 };
	FootageScreenRect.bIsValid = true;
	TrackerImageViewer->ResetTrackerImageScreenRect(FootageScreenRect);
	TrackerImageViewer->SetTrackerImageSize(FIntPoint(720, 1280));

	FDeprecateSlateVector2D& SlateGeometrySize = const_cast<FDeprecateSlateVector2D&>(TrackerImageViewer->GetPersistentState().AllottedGeometry.Size);
	SlateGeometrySize = FVector2f{ 888.58856201171875, 541.96215820312500 };
	TrackerImageViewer->SetDataControllerForCurrentFrame(ContourDataController);
	
	// Point position and number tests:

	TestEqual(TEXT("Widget space point number"), TrackerImageViewer->SplineDensePoints()["crv_brow_lower_r"].Num(), 20);
	TestEqual(TEXT("Widget space point number"), TrackerImageViewer->SplineDensePoints()["crv_brow_intermediate_r"].Num(), 6);
	TestEqual(TEXT("Widget space point number"), TrackerImageViewer->SplineDensePoints()["crv_brow_upper_r"].Num(), 25);

	TArray<FVector2D> LowerBrowPoints = TrackerImageViewer->SplineDensePoints()["crv_brow_lower_r"];
	TestEqual(TEXT("Widget space point position"), LowerBrowPoints[0], FVector2D(430.36505065318198, 243.69096230603753));
	TestEqual(TEXT("Widget space point position"), LowerBrowPoints[3], FVector2D(423.47249526180741, 243.25693647532412));
	TestEqual(TEXT("Widget space point position"), LowerBrowPoints[14], FVector2D(398.45215608251488, 240.79500333144227));

	TestEqual(TEXT("Image space point number"), TrackerImageViewer->SplineDensePointsImageSpace()["crv_brow_lower_r"].Num(), 20);
	TestEqual(TEXT("Image space point number"), TrackerImageViewer->SplineDensePointsImageSpace()["crv_brow_intermediate_r"].Num(), 6);
	TestEqual(TEXT("Image space point number"), TrackerImageViewer->SplineDensePointsImageSpace()["crv_brow_upper_r"].Num(), 25);

	TArray<FVector2D> IntermediatePoints = TrackerImageViewer->SplineDensePointsImageSpace()["crv_brow_intermediate_r"];
	TestEqual(TEXT("Image space point position"), IntermediatePoints[0], FVector2D(333.50833129882812, 559.82629394531250));
	TestEqual(TEXT("Image space point position"), IntermediatePoints[4], FVector2D(332.32318115234375, 575.10467529296875));
	TestEqual(TEXT("Image space point position"), IntermediatePoints[5], FVector2D(329.67004394531250, 577.97338867187500));

	const TArray<FControlVertex>& AllControlPoints = TrackerImageViewer->ControlVerticesOnAllSplines();
	TestEqual(TEXT("Number of control points"), AllControlPoints.Num(), 17);
	TestEqual(TEXT("Control point position"), AllControlPoints[16].PointPosition, FVector2D(431.98913523030672, 236.01242916076961));
	TestEqual(TEXT("Control point position"), AllControlPoints[6].PointPosition, FVector2D(431.48766572132911, 242.47713105116659));
	TestEqual(TEXT("Control point position"), AllControlPoints[12].PointPosition, FVector2D(392.18924288831442, 235.77219898837308));

	TestEqual(TEXT("Control Point Curves"), AllControlPoints[16].CurveNames.Num(), 2);
	TestEqual(TEXT("Control Point Curves"), AllControlPoints[16].CurveNames[0], "crv_brow_intermediate_r");
	TestEqual(TEXT("Control Point Curves"), AllControlPoints[16].CurveNames[1], "crv_brow_upper_r");

	// Highlighting tests:

	FBox2d ZoomedImage;
	ZoomedImage.Min = FVector2D(0.423648, 0.395327);
	ZoomedImage.Max = FVector2D(0.499926, 0.471605);
	TrackerImageViewer->SetUVRegion(ZoomedImage);

	TArray<FVector2D> TestHighlightPositions;
	TestHighlightPositions.Add({ 135.0, 349.2 });
	TestHighlightPositions.Add({ 249.0, 279.2 });
	TestHighlightPositions.Add({ 468.0, 194.2 });
	TestHighlightPositions.Add({ 518.0, 233.2 });
	TestHighlightPositions.Add({ 622.0, 278.2 });
	TestHighlightPositions.Add({ 730.0, 268.2 });
	TestHighlightPositions.Add({ 154.0, 380.2 });

	for (const FVector2D& HighlightPos : TestHighlightPositions)
	{
		TrackerImageViewer->ResolveHighlightingForTesting(HighlightPos);
		TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "crv_brow_upper_r");
		TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightPointID(), 0);
	}
	
	FVector2D IntermediateCurveHighlightPos1 = { 720.0, 292.2 };
	FVector2D NoHighlightPos1 = { 695.0, 306.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(IntermediateCurveHighlightPos1);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "crv_brow_intermediate_r");
	TrackerImageViewer->ResolveHighlightingForTesting(NoHighlightPos1);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "");

	FVector2D IntermediateCurveHighlightPos2 = { 728.0, 328.2 };
	FVector2D HighlightPointOnCurve = { 727.0, 349.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(IntermediateCurveHighlightPos2);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "crv_brow_intermediate_r");
	TrackerImageViewer->ResolveHighlightingForTesting(HighlightPointOnCurve);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "");

	FVector2D IntermediateCurveHighlightPos3 = { 712.0, 380.2 };
	FVector2D NoHighlightPos3 = { 616.0, 324.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(IntermediateCurveHighlightPos3);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "crv_brow_intermediate_r");
	TrackerImageViewer->ResolveHighlightingForTesting(NoHighlightPos3);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "");

	FVector2D PointHighlightPos1 = { 144.0, 394.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(PointHighlightPos1);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightPointID(), 15);
	FVector2D PointHighlightPos2 = { 288.0, 348.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(PointHighlightPos2);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightPointID(), 4);
	FVector2D PointHighlightPos3 = { 432.0, 214.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(PointHighlightPos3);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightPointID(), 9);
	FVector2D PointHighlightPos4 = { 731.0, 282.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(PointHighlightPos4);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightPointID(), 17);
	TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "");

	FBox2d ZoomedExtreme;
	ZoomedExtreme.Min = FVector2D(0.460252, 0.437207);
	ZoomedExtreme.Max = FVector2D(0.480338, 0.457293);
	TrackerImageViewer->SetUVRegion(ZoomedExtreme);

	FVector2D LowerCurveHighlightPosStart = { 60.0, 191.2 };
	FVector2D LowerCurveHighlightPosEnd = { 818.0, 330.2 };
	FVector2D DeltaIncrement = (LowerCurveHighlightPosEnd - LowerCurveHighlightPosStart) / 10.0;

	for (int32 Ctr = 0; Ctr < 10; ++Ctr)
	{
		TrackerImageViewer->ResolveHighlightingForTesting(LowerCurveHighlightPosStart);
		TestEqual(TEXT("UpperCurveHighlight"), TrackerImageViewer->GetHighlightCurveName(), "crv_brow_lower_r");
		LowerCurveHighlightPosStart += DeltaIncrement;
	}

	// Selection tests:

	// Single curve selection works based of highlighted curve (done above)
	TrackerImageViewer->ResolveSelectionForTesting({});
	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedCurves().Num(), 1);
	TestTrue(TEXT("Selection"), TrackerImageViewer->GetSelectedCurves().Contains("crv_brow_lower_r"));
	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedPointIds().Num(), 7);

	FBox2d ZoomOnBrow;
	ZoomOnBrow.Min = FVector2D(0.432912, 0.406458);
	ZoomOnBrow.Max = FVector2D(0.49022, 0.463767);
	TrackerImageViewer->SetUVRegion(ZoomOnBrow);

	FVector2D SelectionMousePos = FVector2D(536.0, 195.2);
	TrackerImageViewer->ResolveHighlightingForTesting(SelectionMousePos);
	TrackerImageViewer->ResolveSelectionForTesting(SelectionMousePos);

	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedCurves().Num(), 1);
	TestTrue(TEXT("Selection"), TrackerImageViewer->GetSelectedCurves().Contains("crv_brow_upper_r"));
	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedPointIds().Num(), 9);

	// Reset highlight and selection
	
	FDeprecateSlateVector2D DummyResetPosition;
	FPointerEvent LeftDown = FPointerEvent(0, 0, DummyResetPosition, DummyResetPosition, 0.0, true);
	TrackerImageViewer->ResolveHighlightingForTesting(FVector2D(100.0, 100.0));
	TrackerImageViewer->OnMouseButtonDown(FGeometry(), LeftDown);

	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedCurves().Num(), 0);
	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedPointIds().Num(), 0);
	// Select all points on the curve individually to check that curve gets selected

	TArray<FVector2D> AllIntermidCurvePositions = { { 796.0, 409.2 } , { 815.0, 385.2 }, { 824.0, 360.2 }, { 826.0, 274.2 } };
	for (const FVector2D& Position : AllIntermidCurvePositions)
	{
		TrackerImageViewer->ResolveHighlightingForTesting(Position);
		TrackerImageViewer->PopulateSelectionForTesting();
	}

	TestTrue(TEXT("Selection"), TrackerImageViewer->GetSelectedCurves().Contains("crv_brow_intermediate_r"));
	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedPointIds().Num(), 4);

	// De-selecting a single point on selected curve should invalidate that curve selection
	TrackerImageViewer->PopulateSelectionForTesting();
	TestTrue(TEXT("Selection"), TrackerImageViewer->GetSelectedCurves().IsEmpty());
	TestEqual(TEXT("Selection"), TrackerImageViewer->GetSelectedPointIds().Num(), 3);


	// Add/Remove keys test

	FVector2D PointForIdQuery = { 200.0, 365.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(PointForIdQuery);
	TestEqual(TEXT("AddRemovePoint"), TrackerImageViewer->GetHighlightPointID(), 5);

	FVector2D AddPointOnCurve1 = { 116.0, 396.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(AddPointOnCurve1);
	TrackerImageViewer->ResolveAddRemoveForTesting(AddPointOnCurve1, true);
	TestEqual(TEXT("Number of control points"), AllControlPoints.Num(), 18);

	TrackerImageViewer->ResolveHighlightingForTesting(PointForIdQuery);
	TestEqual(TEXT("AddRemovePoint"), TrackerImageViewer->GetHighlightPointID(), 5);

	FVector2D AddPointOnCurve2 = { 759.0, 410.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(AddPointOnCurve2);
	TrackerImageViewer->ResolveAddRemoveForTesting(AddPointOnCurve2, true);
	TestEqual(TEXT("Number of control points"), AllControlPoints.Num(), 19);

	TrackerImageViewer->ResolveHighlightingForTesting(PointForIdQuery);
	TestEqual(TEXT("AddRemovePoint"), TrackerImageViewer->GetHighlightPointID(), 6);

	FVector2D AddRemovePointUpperBrow = { 572.0, 210.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(AddRemovePointUpperBrow);
	TrackerImageViewer->ResolveAddRemoveForTesting(AddRemovePointUpperBrow, true);
	TestEqual(TEXT("Number of control points"), AllControlPoints.Num(), 20);
	TrackerImageViewer->ResolveAddRemoveForTesting(AddRemovePointUpperBrow, false);
	TestEqual(TEXT("Number of control points"), AllControlPoints.Num(), 19);

	FVector2D PointTooCloseToExisting = { 677.0, 234.2 };
	TrackerImageViewer->ResolveHighlightingForTesting(PointTooCloseToExisting);
	TrackerImageViewer->ResolveAddRemoveForTesting(PointTooCloseToExisting, true);
	TestEqual(TEXT("Number of control points"), AllControlPoints.Num(), 19);

	// TODO: Add tests for endpoint selection with hiding curves

	// Add test for neutral pose and teeth pose to contain correct curves

	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanFaceContourTrackerAsset.h"
#include "MetaHumanCurveDataController.h"

#include "Engine/EngineBaseTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanIdentityPromotedFrames)

#define LOCTEXT_NAMESPACE "MetaHumanIdentityPromotedFrame"


/////////////////////////////////////////////////////
// UMetaHumanIdentityPromotedFrame

const FIntPoint UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize{ 2048, 2048 };

UMetaHumanIdentityPromotedFrame::UMetaHumanIdentityPromotedFrame()
	: Super{}
	, bIsHeadAlignmentSet{ false }
	, bUseToSolve{ true }
	, bIsNavigationLocked{ false }
	, bTrackOnChange{ false }
	, bIsFrontView{false}
{
	ContourData = CreateDefaultSubobject<UMetaHumanContourData>(TEXT("ContourData"));
	CurveDataController = MakeShared<FMetaHumanCurveDataController>(ContourData);
}

void UMetaHumanIdentityPromotedFrame::PostLoad()
{
	Super::PostLoad();
	CurveDataController->GenerateDrawDataForDensePoints();
	CurveDataController->GenerateCurvesFromControlVertices();
}

void UMetaHumanIdentityPromotedFrame::InitializeMarkersFromParsedConfig(const FFrameTrackingContourData& InContourData, const FString& InConfigVersion)
{
	CurveDataController->InitializeContoursFromConfig(InContourData, InConfigVersion);
}

void UMetaHumanIdentityPromotedFrame::UpdateContourDataFromFrameTrackingContours(const FFrameTrackingContourData& InContourData, bool bUpdateCurveStates) const
{
	CurveDataController->UpdateFromContourData(InContourData, bUpdateCurveStates);
}

void UMetaHumanIdentityPromotedFrame::UpdateContourDataForIndividualCurves(const FFrameTrackingContourData& InContourData) const
{
	CurveDataController->UpdateIndividualCurves(InContourData);
}

bool UMetaHumanIdentityPromotedFrame::FrameContoursContainActiveData() const
{
	return ContourData && ContourData->FrameTrackingContourData.ContainsActiveData();
}

bool UMetaHumanIdentityPromotedFrame::AreActiveCurvesValidForConforming(const FBox2D& InTexCanvas) const
{
	for (const TPair<FString, FTrackingContour>& Contour : ContourData->FrameTrackingContourData.TrackingContours)
	{
		if (Contour.Value.State.bActive)
		{
			for (const FVector2D& Point : Contour.Value.DensePoints)
			{
				if (!InTexCanvas.IsInside(Point))
				{
					return false;
				}
			}
		}
	}

	return true;
}


bool UMetaHumanIdentityPromotedFrame::DiagnosticsIndicatesProcessingIssue(float InMinimumDepthMapFaceCoverage, float InMinimumDepthMapFaceWidth, FText& OutDiagnosticsWarningMessage) const
{
	bool bDiagnosticsIndicatesIssue = false;

	// check we got some tracking results
	if (GetFrameTrackingContourData() == nullptr || !GetFrameTrackingContourData()->ContainsActiveData())
	{
		OutDiagnosticsWarningMessage = FText::Format(LOCTEXT("PromotedFrameFaceTrackingWarning", "Failed to track facial contours in the promoted frame."),
			InMinimumDepthMapFaceCoverage);
		bDiagnosticsIndicatesIssue = true;
	}
	else
	{

		// depthmap face coverage
		if (DepthMapDiagnostics.NumFacePixels == 0 || static_cast<float>(DepthMapDiagnostics.NumFaceValidDepthMapPixels) /
			DepthMapDiagnostics.NumFacePixels * 100 < InMinimumDepthMapFaceCoverage)
		{
			OutDiagnosticsWarningMessage = FText::Format(LOCTEXT("PromotedFrameFaceDepthMapDiagnosticsWarning1", "The promoted frame contained less than {0}% valid depth-map pixels in the region of the face.\nPlease check the depth-map and ensure that there is adequate coverage in the region of the face; you may need to re-ingest your capture data with better Min Distance and/or Max Distance properties set in the CaptureSource asset in order to fix this."),
				InMinimumDepthMapFaceCoverage);
			bDiagnosticsIndicatesIssue = true;
		}

		// depthmap face width
		if (DepthMapDiagnostics.FaceWidthInPixels < InMinimumDepthMapFaceWidth)
		{
			bDiagnosticsIndicatesIssue = true;
			FText FaceWidthDiagnosticsWarningMessage = FText::Format(LOCTEXT("PromotedFrameFaceWidthDiagnosticsWarningMessage", "The promoted frame contained a face of width less than {0} pixels in the depth-map.\nPlease ensure that the face covers a larger area of the image in order to obtain good animation results."),
				InMinimumDepthMapFaceWidth);

			if (OutDiagnosticsWarningMessage.ToString().Len() > 0)
			{
				OutDiagnosticsWarningMessage = FText::FromString(OutDiagnosticsWarningMessage.ToString() + TEXT("\n\n") + FaceWidthDiagnosticsWarningMessage.ToString());
			}
			else
			{
				OutDiagnosticsWarningMessage = FaceWidthDiagnosticsWarningMessage;
			}
		}
	}

	return bDiagnosticsIndicatesIssue;
}


const FFrameTrackingContourData* UMetaHumanIdentityPromotedFrame::GetFrameTrackingContourData() const
{
	return &ContourData->FrameTrackingContourData;
}

TSharedPtr<FMetaHumanCurveDataController> UMetaHumanIdentityPromotedFrame::GetCurveDataController() const
{
	return CurveDataController;
}

bool UMetaHumanIdentityPromotedFrame::CanTrack() const
{
	return ContourTracker != nullptr && ContourTracker->CanProcess();
}

bool UMetaHumanIdentityPromotedFrame::IsTrackingOnChange() const
{
	return bTrackOnChange;
}

bool UMetaHumanIdentityPromotedFrame::IsTrackingManually() const
{
	return !bTrackOnChange;
}

bool UMetaHumanIdentityPromotedFrame::IsNavigationLocked() const
{
	return bIsNavigationLocked;
}

void UMetaHumanIdentityPromotedFrame::SetNavigationLocked(bool bIsLocked)
{
	bIsNavigationLocked = bIsLocked;

	if (IsNavigationLocked())
	{
		bTrackOnChange = false;
	}
}

void UMetaHumanIdentityPromotedFrame::ToggleNavigationLocked()
{
	SetNavigationLocked(!bIsNavigationLocked);
}

/////////////////////////////////////////////////////
// UMetaHumanIdentityCameraFrame

UMetaHumanIdentityCameraFrame::UMetaHumanIdentityCameraFrame()
	: Super{}
	, ViewLocation{ 0.0, 1024.0, 512.0 }
	, ViewRotation{ -15.0, 90.0, 0.0 }
	, LookAtLocation{ FVector::ZeroVector }
	, CameraViewFOV{ 90.0f }
	, ViewMode{ EViewModeIndex::VMI_Lit }
{
	// The default values for the camera transform come from Viewports.h, which is an editor only header so it can't be used here

#if WITH_EDITOR
	if (FProperty* ViewModeProperty = StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanIdentityCameraFrame, ViewMode)))
	{
		static const FName ValidEnumValuesName(TEXT("ValidEnumValues"));
		if (!ViewModeProperty->HasMetaData(ValidEnumValuesName))
		{
			// Only allow certain modes to be selected from the details panel where this property is displayed
			// This can be done by setting the ValidEnumValues as the property metadata
			FString ValidEnumValuesStr;
			for (const EViewModeIndex AllowViewMode : { EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Unlit, EViewModeIndex::VMI_LightingOnly })
			{
				ValidEnumValuesStr += StaticEnum<EViewModeIndex>()->GetNameStringByValue(AllowViewMode) + TEXT(",");
			}
			ViewModeProperty->SetMetaData(ValidEnumValuesName, *ValidEnumValuesStr);
		}
	}
#endif
}

#if WITH_EDITOR

void UMetaHumanIdentityCameraFrame::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	if (FProperty* Property = InPropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = *Property->GetName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, ViewLocation) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, ViewRotation) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, LookAtLocation) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, CameraViewFOV) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, ViewMode) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, FixedEV100))
		{
			OnCameraTransformChangedDelegate.ExecuteIfBound();
		}
	}
}

#endif

FTransform UMetaHumanIdentityCameraFrame::GetCameraTransform() const
{
	return FTransform{ ViewRotation, ViewLocation };
}

FMinimalViewInfo UMetaHumanIdentityCameraFrame::GetMinimalViewInfo() const
{
	FMinimalViewInfo ViewInfo;
	ViewInfo.Location = ViewLocation;
	ViewInfo.Rotation = ViewRotation;
	ViewInfo.FOV = CameraViewFOV;
	ViewInfo.AspectRatio = UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.X / UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.Y;
	ViewInfo.ProjectionMode = ECameraProjectionMode::Perspective;

	return ViewInfo;
}

#undef LOCTEXT_NAMESPACE
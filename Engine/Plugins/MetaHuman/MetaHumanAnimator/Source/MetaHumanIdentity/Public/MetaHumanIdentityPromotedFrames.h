// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetaHumanContourData.h"
#include "Camera/CameraTypes.h"
#include "DepthMapDiagnosticsResult.h"

#include "MetaHumanIdentityPromotedFrames.generated.h"

/////////////////////////////////////////////////////
// UMetaHumanIdentityPromotedFrame

UCLASS(Abstract, BlueprintType)
class METAHUMANIDENTITY_API UMetaHumanIdentityPromotedFrame
	: public UObject
{
	GENERATED_BODY()

public:

	/** The default size to be used when capturing a promoted frame for tracking */
	static const FIntPoint DefaultTrackerImageSize;

	UMetaHumanIdentityPromotedFrame();

public:

	//~ UObject interface
	virtual void PostLoad() override;

public:
	/** Initialize the contour data for this frame with curves, specified in the config */
	void InitializeMarkersFromParsedConfig(const FFrameTrackingContourData& InContourData, const FString& InConfigVersion);

	/** Updates the ContourData for curves initialized from the config. Bool controls if visibility and "curveModified" status should be updated*/
	void UpdateContourDataFromFrameTrackingContours(const FFrameTrackingContourData& InContourData, bool bUpdateCurveStates = false) const;

	/** Updates the individual curves in contour data */
	void UpdateContourDataForIndividualCurves(const FFrameTrackingContourData& InContourData) const;

	/** Returns true if all active curves are inside the viewport */
	bool AreActiveCurvesValidForConforming(const FBox2D& InTexCanvas) const;

	const FFrameTrackingContourData* GetFrameTrackingContourData() const;

	/** Return the Curve Data Controller for this frame */
	TSharedPtr<class FMetaHumanCurveDataController> GetCurveDataController() const;

	/** Returns true if ContourData in ShapeAnnotation contains any curves that are active */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Tracking")
	bool FrameContoursContainActiveData() const;

	/** Returns true if this Promoted Frame has all the required information to track */
	UFUNCTION(BlueprintPure, Category = "MetaHuman|Tracking")
	bool CanTrack() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Tracking")
	bool IsTrackingOnChange() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Tracking")
	bool IsTrackingManually() const;

	UFUNCTION(BlueprintPure, Category = "MetaHuman|Navigation")
	bool IsNavigationLocked() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Navigation")
	void SetNavigationLocked(bool bIsLocked);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Navigation")
	void ToggleNavigationLocked();

	/** Returns true if the diagnostics associated with the promoted frame indicate a potential issue, and returns a warning message if so. Note that the thresholds
	supplied for checking whether an issue exists are properties of UMetaHumanIdentityFace and can be obtained from there */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Diagnostics")
	bool DiagnosticsIndicatesProcessingIssue(float InMinimumDepthMapFaceCoverage, float InMinimumDepthMapFaceWidth, FText& OutDiagnosticsWarningMessage) const;

public:

	/** The alignment of the conformal mesh associated with this promoted frame */
	UPROPERTY(VisibleAnywhere, Category = "Annotations")
	FTransform HeadAlignment;

	/** The name of frame as given by the user */
	UPROPERTY(EditAnywhere, Category = "Frame")
	FText FrameName;

	/** Do we have a valid alignment of the conformal mesh associated with this promoted frame */
	UPROPERTY(VisibleAnywhere, Category = "Annotations")
	uint8 bIsHeadAlignmentSet : 1;

	/** Whether or not the markers (landmarks) of this Promoted Frame are active */
	UPROPERTY(EditAnywhere, Category = "Frame")
	uint8 bUseToSolve : 1;

	/** Whether or not the navigation is locked for this Promoted frame */
	UPROPERTY(EditAnywhere, Category = "Frame")
	uint8 bIsNavigationLocked : 1;

	/** Whether or not track on change is enabled */
	UPROPERTY(Transient)
	uint8 bTrackOnChange : 1;

	/** Whether this promoted frame is front view */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frame")
	uint8 bIsFrontView : 1;

	/** The tracker that can be used to track landmarks on the data represented by this Promoted Frame */
	UPROPERTY(EditAnywhere, Category = "Trackers")
	TObjectPtr<class UMetaHumanFaceContourTrackerAsset> ContourTracker;

	/** Contour data class that holds all the data for curves and control vertices */
	UPROPERTY(EditAnywhere, Category = "Annotations")
	TObjectPtr<class UMetaHumanContourData> ContourData;

	/** Data controller for MetaHumanContourData. Any data manipulation is done in the controller */
	TSharedPtr<class FMetaHumanCurveDataController> CurveDataController;

	/** The depth-map diagnostics result for the frame */
	UPROPERTY(VisibleAnywhere, Category = "Diagnostics")
	FDepthMapDiagnosticsResult DepthMapDiagnostics;

};

/////////////////////////////////////////////////////
// UMetaHumanIdentityCameraFrame

enum EViewModeIndex : int;

UCLASS()
class METAHUMANIDENTITY_API UMetaHumanIdentityCameraFrame
	: public UMetaHumanIdentityPromotedFrame
{
	GENERATED_BODY()

public:

	UMetaHumanIdentityCameraFrame();

#if WITH_EDITOR
	//~ UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif

public:

	/** Returns the viewport transform as a FTransform object */
	FTransform GetCameraTransform() const;

	FSimpleDelegate& OnCameraTransformChanged()
	{
		return OnCameraTransformChangedDelegate;
	}

	/** Returns the minimal view info for this camera frame */
	FMinimalViewInfo GetMinimalViewInfo() const;

public:

	/** The current camera location for this Promoted Frame */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (EditCondition = "!bIsNavigationLocked"))
	FVector ViewLocation;

	/** The current camera rotation for this Promoted Frame */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (EditCondition = "!bIsNavigationLocked"))
	FRotator ViewRotation;

	/** The current camera LookAt position for this Promoted Frame */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (EditCondition = "!bIsNavigationLocked"))
	FVector LookAtLocation;

	/** The Camera FoV from when the view was promoted */
	UPROPERTY(EditAnywhere, Category = "Camera", DisplayName = "Field Of View", meta = (ClampMin = "0", ClampMax = "170", EditCondition = "!bIsNavigationLocked"))
	float CameraViewFOV;

	/** The View Mode from when the frame was promoted */
	UPROPERTY(EditAnywhere, Category = "Rendering", DisplayName = "Lighting Mode", meta = (EditCondition = "!bIsNavigationLocked"))
	TEnumAsByte<EViewModeIndex> ViewMode;

	/** The EV100 value to used for this promoted frame */
	UPROPERTY(EditAnywhere, Category = "Rendering", DisplayName = "EV100", meta = (EditCondition = "!bIsNavigationLocked", UIMin=-10, UIMax=20))
	float FixedEV100;

private:

	/** Delegate called when one of the camera transform parameters changes */
	FSimpleDelegate OnCameraTransformChangedDelegate;
};

/////////////////////////////////////////////////////
// UMetaHumanIdentityFootageFrame

UCLASS()
class METAHUMANIDENTITY_API UMetaHumanIdentityFootageFrame
	: public UMetaHumanIdentityPromotedFrame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Frame")
	int32 FrameNumber;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Curves/RealCurve.h"
#include "Containers/EnumAsByte.h"
#include "GameFramework/Actor.h"

#include "FrameAnimationData.h"

#include "MetaHumanPerformanceExportUtils.generated.h"

UENUM()
enum class EPerformanceExportRange : uint8
{
	ProcessingRange        UMETA(DisplayName = "Processing Range"),
	WholeSequence          UMETA(DisplayName = "Whole Sequence")
};

/////////////////////////////////////////////////////
// UMetaHumanPerformanceExportAnimationSettings

UCLASS(BlueprintType)
class METAHUMANPERFORMANCE_API UMetaHumanPerformanceExportAnimationSettings
	: public UObject
{
	GENERATED_BODY()

public:

	UMetaHumanPerformanceExportAnimationSettings()
		: bEnableHeadMovement(true)
		, bShowExportDialog(true)
		, bAutoSaveAnimSequence(true)
		, bFortniteCompatibility(true)
		, ExportRange(EPerformanceExportRange::WholeSequence)
	{}

	// Whether or not to enable the Head Movement in the exported Animation Sequence, default to true if head pose available
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings")
	uint8 bEnableHeadMovement : 1;

	// Whether or not to show the export dialog allowing the user to select where to place the animation sequence, default to true
	UPROPERTY(BlueprintReadWrite, Category = "Export Settings")
	uint8 bShowExportDialog : 1;

	// Whether or not to auto save the generated animation sequence, default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings")
	uint8 bAutoSaveAnimSequence : 1;

	// Whether or not to set the metadata tags required to make the generated animation sequence compatible with Fortnite characters, default to true
	// Currently this parameter is not exposed but could be in future.
	UPROPERTY()
	uint8 bFortniteCompatibility : 1;

	// The export range that will be used to generate the animation sequence, defaults to EPerformanceExportRange::WholeSequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings")
	EPerformanceExportRange ExportRange;

	// The Skeleton or Skeletal Mesh to be used when recording the Animation Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings", meta = (AllowedClasses = "/Script/Engine.Skeleton, /Script/Engine.SkeletalMesh"))
	TObjectPtr<UObject> TargetSkeletonOrSkeletalMesh;

	// This defines how values between keys are calculated for curves
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings");
	TEnumAsByte<ERichCurveInterpMode> CurveInterpolation = ERichCurveInterpMode::RCIM_Linear;

	// The name of the level sequence. If bShowExportDialog is true the user will be able to select this value
	UPROPERTY(BlueprintReadWrite, Category = "Asset Placement")
	FString AssetName;

	// The package path where the animation sequence will be placed, if bShowExportDialog is true this option is ignored
	UPROPERTY(BlueprintReadWrite, Category = "Asset Placement")
	FString PackagePath;

public:

	/** Returns the target Skeleton derived from TargetSkeletonOrSkeletalMesh */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Compatibility")
	USkeleton* GetTargetSkeleton() const;

	/**
	 * Checks if the Target Skeleton set in TargetSkeletonOrSkeletalMesh may have compatibility issues.
	 * The compatibility test will check if the the skeleton has all the requested curves and will return the missing curves in OutMissingCurvesInSkeleton.
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Compatibility")
	bool IsTargetSkeletonCompatible(const TSet<FString>& InCurves, TArray<FString>& OutMissingCurvesInSkeleton) const;
};

/////////////////////////////////////////////////////
// UMetaHumanPerformanceExportLevelSequenceSettings

UCLASS(BlueprintType)
class METAHUMANPERFORMANCE_API UMetaHumanPerformanceExportLevelSequenceSettings
	: public UObject
{
	GENERATED_BODY()

public:

	UMetaHumanPerformanceExportLevelSequenceSettings()
		: bShowExportDialog(true)
		, bExportVideoTrack(true)
		, bExportDepthTrack(false)
		, bExportAudioTrack(true)
		, bExportImagePlane(true)
		, bExportDepthMesh(false)
		, bExportCamera(true)
		, bApplyLensDistortion(false)
		, bExportIdentity(true)
		, bExportControlRigTrack(true)
		, bEnableControlRigHeadMovement(true)
		, bExportTransformTrack(true)
		, bKeepFrameRange(true)
		, bEnableMetaHumanHeadMovement(true)
	{}


	// The package path where the level sequence will be placed. If bShowExportDialog is true the user will be able to able to select this value
	UPROPERTY(BlueprintReadWrite, Category = "Asset Placement")
	FString PackagePath;

	// The name of the level sequence. If bShowExportDialog is true the user will be able to select this value
	UPROPERTY(BlueprintReadWrite, Category = "Asset Placement")
	FString AssetName;

	// Whether or not to display to display a dialog to the user where the export options and path can be selected. Default to true
	UPROPERTY(BlueprintReadWrite, Category = "Export Settings")
	uint8 bShowExportDialog : 1;

	// Whether or not to export the video track. Default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media Tracks")
	uint8 bExportVideoTrack : 1;

	// Whether or not to export the depth track, default to false
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media Tracks")
	uint8 bExportDepthTrack : 1;

	// Whether or not to export the audio track, default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media Tracks")
	uint8 bExportAudioTrack : 1;

	// Whether or not to export the image plane. Ignored if bExportVideoTrack is false. Default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings", meta = (EditCondition = "bExportVideoTrack"))
	uint8 bExportImagePlane : 1;

	// Whether or not to export the depth mesh. Ignored if bExportDepthTrack is false. Default to false
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings", meta = (EditCondition = "bExportDepthTrack"))
	uint8 bExportDepthMesh : 1;

	// Whether or not to export a camera that matches the one used in the Performance. Default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings")
	uint8 bExportCamera : 1;

	// Whether or not camera lens distortion should be applied to exported camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings", meta = (EditCondition = "bExportCamera"))
	uint8 bApplyLensDistortion : 1;

	// Whether or not export the Identity mesh. Default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Export MetaHuman Identity", Category = "MetaHuman Identity Tracks")
	uint8 bExportIdentity : 1;

	// Whether or not to export the Control Rig track with baked data. Default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman Identity Tracks", meta = (EditCondition = "bExportIdentity"))
	uint8 bExportControlRigTrack : 1;

	// Whether or not to enable Head Movement using Control Rig.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman Identity Tracks", meta = (EditCondition = "bExportIdentity && bExportControlRigTrack"))
	uint8 bEnableControlRigHeadMovement : 1;

	// Whether or not to bake the animation data into the rigid transform track for the Identity actor. Default to true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman Identity Tracks", meta = (EditCondition = "bExportIdentity"))
	uint8 bExportTransformTrack : 1;

	// Whether or not to keep the frame range defined by the Processing Range. Disabling this will force the Level Sequence tracks to start at frame 0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Range", meta = (EditCondition = "ExportRange == EPerformanceExportRange::ProcessingRange", DisplayAfter = "ExportRange"))
	uint8 bKeepFrameRange : 1;

	// Whether or not to enable the head movement switch in the Target MetaHuman
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman Tracks", DisplayName = "Enable MetaHuman Head Movement",
			  meta = (EditCondition = "TargetMetaHumanClass != nullptr", DisplayAfter = "TargetMetaHumanClass"))
	uint8 bEnableMetaHumanHeadMovement : 1;

	// Optional MetaHuman created as a spawnable in the exported Level Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHuman Tracks", DisplayName = "Target MetaHuman Class")
	TObjectPtr<class UBlueprint> TargetMetaHumanClass;

	// The export range that will be used to generate the Level Sequence, defaults to EPerformanceExportRange::WholeSequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Range")
	EPerformanceExportRange ExportRange = EPerformanceExportRange::WholeSequence;

	// This defines how values between keys are calculated for curves
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export Settings")
	TEnumAsByte<ERichCurveInterpMode> CurveInterpolation = ERichCurveInterpMode::RCIM_Linear;
};

/////////////////////////////////////////////////////
// UMetaHumanPerformanceExportUtils

/**
 * Utility functions to export data from a Performance
 */
UCLASS()
class METAHUMANPERFORMANCE_API UMetaHumanPerformanceExportUtils
	: public UObject
{
	GENERATED_BODY()

public:

	/** Returns a UMetaHumanPerformanceExportAnimationSettings configured based on the given Performance */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Export Settings")
	static UMetaHumanPerformanceExportAnimationSettings* GetExportAnimationSequenceSettings(const class UMetaHumanPerformance* InPerformance);

	/** Returns a UMetaHumanPerformanceExportLevelSequenceSettings configured based on the given Performance */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Export Settings")
	static UMetaHumanPerformanceExportLevelSequenceSettings* GetExportLevelSequenceSettings(const class UMetaHumanPerformance* InPerformance);

	/** Exports an animation sequence from a Performance using the given settings */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Export")
	static class UAnimSequence* ExportAnimationSequence(class UMetaHumanPerformance* InPerformance, UMetaHumanPerformanceExportAnimationSettings* InExportSettings = nullptr);

	/** Exports a Level Sequence from a Performance using the given settings object */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Export")
	static class ULevelSequence* ExportLevelSequence(class UMetaHumanPerformance* InPerformance, UMetaHumanPerformanceExportLevelSequenceSettings* InExportSettings = nullptr);

	/** Bake Control Rig animation data of a particular frame from a Performance in the given Control Rig section */
	static void BakeControlRigAnimationData(class UMetaHumanPerformance* InPerformance, class UMovieSceneSequence* InSequence, int32 InFrameNumber, class UMovieSceneControlRigParameterSection* InControlRigSection, const FTransform& InReferenceFrameRootPose, ERichCurveInterpMode InCurveInterpolation = RCIM_Constant, class UControlRig* InRecordControlRig = nullptr, const FVector& InVisualizeMeshHeightOffset = FVector::ZeroVector);

	/** Bake Transform animation data of a particular frame from a Performance in the given 3D Transform section */
	static void BakeTransformAnimationData(class UMetaHumanPerformance* InPerformance, class UMovieSceneSequence* InSequence, int32 InFrameNumber, class UMovieScene3DTransformSection* InTransformSection, ERichCurveInterpMode InCurveInterpolation = RCIM_Constant, const FTransform& InOffsetTransform = FTransform::Identity, const FVector& InVisualizeMeshHeightOffset = FVector::ZeroVector);

	/** Enables or disables the HeadControlSwitch in the given control rig track */
	static void SetHeadControlSwitchEnabled(class UMovieSceneControlRigParameterTrack* InControlRigTrack, bool bInEnableHeadControl);

	static bool CanExportHeadMovement(const class UMetaHumanPerformance* InPerformance);
	static bool CanExportVideoTrack(const class UMetaHumanPerformance* InPerformance);
	static bool CanExportDepthTrack(const class UMetaHumanPerformance* InPerformance);
	static bool CanExportAudioTrack(const class UMetaHumanPerformance* InPerformance);
	static bool CanExportIdentity(const class UMetaHumanPerformance* InPerformance);
	static bool CanExportLensDistortion(const class UMetaHumanPerformance* InPerformance);

	/** Calculates the global transform of a bone in a given reference skeleton. Returns false if the requested bone was not found in the Skeleton */
	static bool GetBoneGlobalTransform(const class USkeleton* InSkeleton, const FName& InBoneName, FTransform& OutTransform);

private:

	/** Function used to record the given ControlRig controls in the section for the given frame number */
	static void RecordControlRigKeys(class UMovieSceneControlRigParameterSection* InSection, FFrameNumber InFrameNumber, class UControlRig* InControlRig, ERichCurveInterpMode InCurveInterpolation = RCIM_Constant);

	/** Function used to record the animation sequence once the new Animation Sequence asset is created */
	static bool RecordAnimationSequence(const TArray<UObject*>& InNewAssets, class UMetaHumanPerformance* InPerformance, UMetaHumanPerformanceExportAnimationSettings* InExportSettings);

	struct FBakeControlRigTrackParams
	{
		class UMetaHumanPerformance* Performance = nullptr;
		class UMetaHumanPerformanceExportLevelSequenceSettings* ExportSettings = nullptr;
		TRange<FFrameNumber> ProcessingRange;
		ULevelSequence* LevelSequence = nullptr;
		UClass* ControlRigClass = nullptr;
		FGuid Binding;
		UObject* ObjectToBind = nullptr;
		bool bEnableHeadMovementSwitch = false;
	};

	/** Internal helper function to bake the Animation data from a MetaHuman Performance into a Control Rig Track */
	static void BakeControlRigTrack(const FBakeControlRigTrackParams& InParams);
	
	/** Apply neutral pose calibration */
	static bool ApplyNeutralPoseCalibration(const UMetaHumanPerformance* InPerformance, const int32 InFrameNumber, FFrameAnimationData& InOutAnimationFrame);
};
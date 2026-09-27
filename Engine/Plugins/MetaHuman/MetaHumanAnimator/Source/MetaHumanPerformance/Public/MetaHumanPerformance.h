// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pipeline/Pipeline.h"
#include "Pipeline/PipelineData.h"
#include "Nodes/TongueTrackerNode.h"
#include "Nodes/ControlUtilNodes.h"
#include "Nodes/HyprsenseRealtimeNode.h"
#include "Nodes/RealtimeSpeechToAnimNode.h"
#include "CaptureData.h"
#include "FrameRange.h"
#ifdef WITH_EDITOR
#include "FrameRangeArrayBuilder.h"
#endif
#include "MetaHumanRealtimeSmoothing.h"
#include "MetaHumanRealtimeCalibration.h"

#include "SequencedImageTrackInfo.h"

#include "Rigs/RigHierarchyElements.h"
#include "MetaHumanPerformance.generated.h"

/////////////////////////////////////////////////////
// UMetaHumanPerformance

enum class EPerformanceExportRange : uint8;

UENUM(BlueprintType)
enum class EDataInputType : uint8
{
	DepthFootage	UMETA(DisplayName = "Depth Footage", ToolTip = "Process depth enabled footage and an identity into animation"),
	Audio			UMETA(DisplayName = "Audio", ToolTip = "Process audio into animation"),
	MonoFootage		UMETA(DisplayName = "Monocular Footage", ToolTip = "Process single view footage into animation")
};

UENUM()
enum class ESolveType : uint8
{
	Preview				UMETA(DisplayName = "Preview"),
	Standard			UMETA(DisplayName = "Standard"),
	AdditionalTweakers	UMETA(DisplayName = "Additional Tweakers"),
};

UENUM()
enum class EPerformanceHeadMovementMode : uint8
{
	/** Use a transform track to move the Skeletal Mesh based on its pivot point (root bone) */
	TransformTrack,

	/** Enables the Head Control Switch in the Control Rig to use control rig for the Head Movement */
	ControlRig,

	/** No head movement */
	Disabled
};

UENUM()
enum class EStartPipelineErrorType : uint8
{
	None,
	NoFrames,
	Disabled,
};

/** MetaHuman Performance Asset
* 
*   Produces an Animation Sequence for MetaHuman Control Rig by tracking
*   facial expressions in video-footage from a Capture Source, imported
*   through Capture Manager, using a SkeletalMesh obtained through
*   MetaHuman Identity asset toolkit.
* 
*/
UCLASS(BlueprintType)
class METAHUMANPERFORMANCE_API UMetaHumanPerformance : public UObject
{
	GENERATED_BODY()

public:

	UMetaHumanPerformance();

	//~Begin UObject interface
	virtual void BeginDestroy() override;
	virtual void PreEditChange(FEditPropertyChain& InPropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~End UObject interface

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProcessingFinishedDynamic);

	// Dynamic delegate called when the pipeline finishes running
	UPROPERTY(BlueprintAssignable, Category = "Processing")
	FOnProcessingFinishedDynamic OnProcessingFinishedDynamic;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Export")
	bool CanExportAnimation() const;

	UE_DEPRECATED(5.1, "ExportAnimation has been deprecated, please use UMetaHumanPerformanceExportUtils::ExportAnimation instead")
	/**
	 * (DEPRECATED: use UMetaHumanPerformanceExportUtils::ExportAnimation instead)
	 * Export an animation sequence targeting the face skeleton. This will ask the user where to place the new animation sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Export")
	void ExportAnimation(EPerformanceExportRange InExportRange);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Diagnostics")
	bool DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const;

	// Event delegates
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDataInputTypeChanged, EDataInputType InProcessingType);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSourceDataChanged, class UFootageCaptureData* InFootageCaptureData, class USoundWave* InAudio, bool InResetRanges);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnIdentityChanged, class UMetaHumanIdentity* InIdentity);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnVisualizeMeshChanged, class USkeletalMesh* InVisualizeMesh);
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnDepthChanged, float InDepthDataNear, float InDepthDataFar, float InDepthMeshNear, float InDepthMeshFar);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFrameRangeChanged, int32 InStartFrame, int32 InEndFrame);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRealtimeAudioChanged, bool bInRealtimeAudio);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFrameProcessed, int32 InFrame);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnProcessingFinished, TSharedPtr<const UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);
	DECLARE_MULTICAST_DELEGATE(FOnStage1ProcessingFinished);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnControlRigClassChanged, TSubclassOf<class UControlRig> InControlRig);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHeadMovementModeChanged, EPerformanceHeadMovementMode InHeadMovementMode);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHeadMovementReferenceFrameChanged, bool bInAutoChooseHeadMovementReferenceFrame, uint32 InHeadMovementReferenceFrame);
	DECLARE_MULTICAST_DELEGATE(FOnNeutralPoseCalibrationChanged);
	DECLARE_MULTICAST_DELEGATE(FOnExcludedFramesChanged);

	FOnDataInputTypeChanged& OnDataInputTypeChanged() { return OnDataInputTypeChangedDelegate; }
	FOnSourceDataChanged& OnSourceDataChanged() { return OnSourceDataChangedDelegate; }
	FOnIdentityChanged& OnIdentityChanged() { return OnIdentityChangedDelegate; }
	FOnVisualizeMeshChanged& OnVisualizeMeshChanged() { return OnVisualizeMeshChangedDelegate; }
	FOnFrameRangeChanged& OnFrameRangeChanged() { return OnFrameRangeChangedDelegate; }
	FOnRealtimeAudioChanged& OnRealtimeAudioChanged() { return OnRealtimeAudioChangedDelegate; }
	FOnFrameProcessed& OnFrameProcessed() { return OnFrameProcessedDelegate; }
	FOnProcessingFinished& OnProcessingFinished() { return OnProcessingFinishedDelegate; }
	FOnStage1ProcessingFinished& OnStage1ProcessingFinished() { return OnStage1ProcessingFinishedDelegate; }
	FOnControlRigClassChanged& OnControlRigClassChanged() { return OnControlRigClassChangedDelegate; }
	FOnHeadMovementModeChanged& OnHeadMovementModeChanged() { return OnHeadMovementModeChangedDelegate; }
	FOnHeadMovementReferenceFrameChanged& OnHeadMovementReferenceFrameChanged() { return OnHeadMovementReferenceFrameChangedDelegate; }
	FOnNeutralPoseCalibrationChanged& OnNeutralPoseCalibrationChanged() { return OnNeutralPoseCalibrationChangedDelegate; }
	FOnExcludedFramesChanged& OnExcludedFramesChanged() { return OnExcludedFramesChangedDelegate; }
	FFrameRangeArrayBuilder::FOnGetCurrentFrame& OnGetCurrentFrame() { return OnGetCurrentFrameDelegate; }

	FFrameRate GetFrameRate() const;

	FString GetHashedPerformanceAssetID();

#endif

	/** Enum to indicate which data input type is being used for the performance */
	UPROPERTY(EditAnywhere, Category = "Data")
	EDataInputType InputType = EDataInputType::DepthFootage;

	/** Real-world footage data with the performance */
	UPROPERTY(EditAnywhere, Category = "Data", meta = (EditCondition = "InputType != EDataInputType::Audio", EditConditionHides))
	TObjectPtr<class UFootageCaptureData> FootageCaptureData;

	/** Audio of performance used with the Audio data input type */
	UPROPERTY(EditAnywhere, Category = "Data", meta = (EditCondition = "InputType == EDataInputType::Audio", EditConditionHides))
	TObjectPtr<class USoundWave> Audio;

	/** Getter for audio to use for processing. Either audio in FootageCaptureData or overridden by Audio */
	TObjectPtr<class USoundWave> GetAudioForProcessing() const;

	/** Getter for timecode from audio media */
	FTimecode GetAudioMediaTimecode() const;

	/** Getter for timecode rate from audio media */
	FFrameRate GetAudioMediaTimecodeRate() const;

	/** Display name of the config to use with the capture data */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Data", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	FString CaptureDataConfig;

	/** Name of camera (view) in the footage capture data calibration to use for display and processing */
	UPROPERTY(EditAnywhere, Category = "Data", meta = (EditCondition = "InputType != EDataInputType::Audio", EditConditionHides))
	FString Camera;

	/** Timecode alignment type */
	UPROPERTY(EditAnywhere, Category = "Data")
	ETimecodeAlignment TimecodeAlignment = ETimecodeAlignment::Relative;

	/** A digital double of the person performing in the footage, captured in the MetaHuman Identity asset */
	UPROPERTY(EditAnywhere, Category = "Data", DisplayName = "MetaHuman Identity", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	TObjectPtr<class UMetaHumanIdentity> Identity;

	/** Control Rig used to drive the animation */
	UPROPERTY()
	TObjectPtr<class UControlRigBlueprint> ControlRig_DEPRECATED;

	UPROPERTY(EditAnywhere, Category = "Visualization", DisplayName = "Control Rig")
	TSubclassOf<class UControlRig> ControlRigClass;

	/** Set a different Skeletal Mesh (e.g. MetaHuman head) for visualizing the final animation */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	TObjectPtr<class USkeletalMesh> VisualizationMesh;

	/** Head movement type */
	UPROPERTY(EditAnywhere, Category = "Visualization")
	EPerformanceHeadMovementMode HeadMovementMode = EPerformanceHeadMovementMode::TransformTrack;

	/** Which frame to use as reference frame for head pose (if Auto Choose Head Movement Reference Frame is not selected), default to first processed frame. Changing this will cause a re-bake of Control Rig data */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Visualization", meta = (EditCondition = "!bAutoChooseHeadMovementReferenceFrame"));
	uint32 HeadMovementReferenceFrame;

	/* If set to true, automatically pick the most front - facing frame as the reference frame for control-rig head movement calculation, default to true. Changing this will cause a re-bake of Control Rig data */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Visualization")
	uint8 bAutoChooseHeadMovementReferenceFrame : 1;

	/** Head reference frame, calculated from the two properties above. If set to -1, indicates it has not been calculated*/
	UPROPERTY(Transient)
	int32 HeadMovementReferenceFrameCalculated;

	/* If set to true perform neutral pose calibration for mono solve, default to false. Changing this will cause a re-bake of Control Rig data */
	UPROPERTY(EditAnywhere, AdvancedDisplay, DisplayName = "Enable Neutral Pose Calibration", Category = "Visualization")
	bool bNeutralPoseCalibrationEnabled = false;

	/* Which frame to use as the neutral pose calibration for mono solve (if Enable Neutral Pose Calibration is selected), default to first processed frame. Changing this will cause a re-bake of Control Rig data */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Visualization")
	uint32 NeutralPoseCalibrationFrame = 0;

	/* Neutral pose calibration alpha parameter, defaults to 1. Changing this will cause a re-bake of Control Rig data */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Visualization", meta = (ClampMin = 0.0, ClampMax = 1.0))
	double NeutralPoseCalibrationAlpha = 1.0;

	/* Set of curve names to apply neutral pose calibration to. Changing this will cause a re-bake of Control Rig data */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Visualization")
	TArray<FName> NeutralPoseCalibrationCurves = FMetaHumanRealtimeCalibration::GetDefaultProperties();

	/* Tracker parameters for processing the footage */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	TObjectPtr<class UMetaHumanFaceContourTrackerAsset> DefaultTracker;

	/* Solver parameters for processing the footage */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	TObjectPtr<class UMetaHumanFaceAnimationSolver> DefaultSolver;

	/* The frame to start processing from */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters")
	uint32 StartFrameToProcess;

	/* The frame to end processing with */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters")
	uint32 EndFrameToProcess;

	/* Enum to indicate which type of solve to perform */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	ESolveType SolveType = ESolveType::AdditionalTweakers;

	/* Flag indicating if performance predictive solver preview should be skipped */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	bool bSkipPreview = false;

	/* Flag indicating if filtering should be skipped */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	bool bSkipFiltering = false;

	/* Flag indicating if tongue solving should be skipped */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	bool bSkipTongueSolve = false;

	/* Flag indicating if per-vertex solve (which is slow to process but gives slightly better animation results) should be skipped */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	bool bSkipPerVertexSolve = true;

	/* Flag indicating if we should use realtime audio solve */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::Audio", EditConditionHides))
	bool bRealtimeAudio = false;

	/* Downmix multi channel audio before solving into animation */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::Audio && !bRealtimeAudio", EditConditionHides))
	bool bDownmixChannels = true;

	/* Specify the audio channel used to solve into animation */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (ClampMin = 0, ClampMax = 64))
	uint32 AudioChannelIndex = 0;

	/* Flag indicating if we should generate blinks */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::Audio && !bRealtimeAudio", EditConditionHides))
	bool bGenerateBlinks = true;

	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (DisplayName = "Process Mask", EditCondition = "InputType == EDataInputType::Audio && !bRealtimeAudio", EditConditionHides))
	EAudioDrivenAnimationOutputControls AudioDrivenAnimationOutputControls = EAudioDrivenAnimationOutputControls::FullFace;

	/* The models to be used by audio driven animation */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Processing Parameters", meta = (DisplayName = "Models", EditCondition = "InputType == EDataInputType::Audio && !bRealtimeAudio", EditConditionHides))
	FAudioDrivenAnimationModels AudioDrivenAnimationModels;

	/* The estimated focal length of the footage */
	UPROPERTY(VisibleAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::MonoFootage", EditConditionHides))
	float FocalLength = -1;

	/* Reduces noise in head position and orientation. */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (EditCondition = "InputType == EDataInputType::MonoFootage", EditConditionHides))
	bool bHeadStabilization = true;

	/* Smoothing parameters to use for mono video processing */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", DisplayName = "Smoothing", meta = (EditCondition = "InputType == EDataInputType::MonoFootage", EditConditionHides))
	TObjectPtr<UMetaHumanRealtimeSmoothingParams> MonoSmoothingParams;

	/* Flag indicating if editor updates current frame to show the results as frames are processed */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters")
	bool bShowFramesAsTheyAreProcessed = true;

	/* Settings to change the behavior of the audio driven animation solve */
	UPROPERTY(EditAnywhere, Category = "Processing Parameters", meta = (DisplayName = "Solve Overrides", ExpandByDefault, EditCondition = "InputType == EDataInputType::Audio && !bRealtimeAudio", EditConditionHides))
	FAudioDrivenAnimationSolveOverrides AudioDrivenAnimationSolveOverrides;

	/* Flag indicating whether processing diagnostics should be calculated during processing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing Diagnostics", meta = (EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	bool bSkipDiagnostics = false;

	/* The minimum percentage of the face region which should have valid depth-map pixels. Below this value a diagnostic warning will be flagged. */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics", meta = (UIMin = 0.0, UIMax = 100.0, ClampMin = 0.0, ClampMax = 100.0, EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	float MinimumDepthMapFaceCoverage = 80.0f;

	/* The minimum required width of the face region on the depth-map in pixels. Below this value a diagnostic warning will be flagged. */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics", meta = (ClampMin = 0.0, ClampMax = 10000.0, EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	float MinimumDepthMapFaceWidth = 120.0f;

	/* The maximum allowed percentage difference in stereo baseline between Identity and Performance CaptureData camera calibrations. Above this value a diagnostic warning will be flagged. */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics", meta = (ClampMin = 0.0, ClampMax = 100.0, EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	float MaximumStereoBaselineDifferenceFromIdentity = 10.0f;

	/* The maximum allowed percentage difference in estimated head scale between Identity and Performance. Above this value a diagnostic warning will be flagged. */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics", meta = (ClampMin = 0.0, ClampMax = 100.0, EditCondition = "InputType == EDataInputType::DepthFootage", EditConditionHides))
	float MaximumScaleDifferenceFromIdentity = 7.5f;

	/* Frames that the user has identified which are to be excluded from the processing, eg part of the footage where the face goes out of frame */
	UPROPERTY(EditAnywhere, Category = "Excluded frames")
	TArray<FFrameRange> UserExcludedFrames;

	/* Frames that the processing has identified as producing bad results and should not be exported */
	UPROPERTY(VisibleAnywhere, Category = "Excluded frames")
	TArray<FFrameRange> ProcessingExcludedFrames;

	/** Stores the viewport settings used in the Performance asset editor */
	UPROPERTY()
	TObjectPtr<class UMetaHumanPerformanceViewportSettings> ViewportSettings;

	// Export options
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Processing")
	EStartPipelineErrorType StartPipeline(bool bInIsScriptedProcessing = true);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Processing")
	void CancelPipeline();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Processing")
	bool IsProcessing() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Processing")
	bool CanProcess() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Processing")
	void SetBlockingProcessing(bool bInBlockingProcessing);

	// Outputs
	UPROPERTY()
	TArray<FDepthMapDiagnosticsResult> DepthMapDiagnosticResults;

	UPROPERTY()
	float ScaleEstimate = 1.0f;

	// A 64 bit version of Contour Data array to support serialization of longer takes
	TArray64<FFrameTrackingContourData> ContourTrackingResults;

	// A 64 bit version of Animation Data array to support serialization of longer takes
	TArray64<struct FFrameAnimationData> AnimationData;
	
	/** Returns true if there is at least one animation frame with valid data, false otherwise */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|AnimationData")
	bool ContainsAnimationData() const;

	/** Returns animation data (frame numbers are animation frame index not sequencer frame numbers) */
	/** Caller is responsible to ensure data will fit into 32bit TArray */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|AnimationData")
	TArray<struct FFrameAnimationData> GetAnimationData(int32 InStartFrameNumber = 0, int32 InEndFrameNumber = -1) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|AnimationData")
	int32 GetNumberOfProcessedFrames() const;

	const TRange<FFrameNumber>& GetProcessingLimitFrameRange() const;
	const TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>>& GetMediaFrameRanges() const;
	FFrameNumber GetMediaStartFrame() const;
	TRange<FFrameNumber> GetExportFrameRange(EPerformanceExportRange InExportRange) const;

	int32 GetPipelineStage() const { return PipelineStage; }

	/**
	 * Returns the effective Skeletal Mesh used for visualization.
	 * This will return the VisualizationMesh if valid or the MetaHuman Identity skeletal mesh.
	 * Returns nullptr if no valid Skeletal Mesh was found
	 */
	USkeletalMesh* GetVisualizationMesh() const;

	/** Calculate the head pose from the AnimationData, either using specified reference frame, or auto-selecting the best one */
	FTransform CalculateReferenceFramePose();

	/** Returns if any frame has valid pose in the AnimationData*/
	bool HasValidAnimationPose() const;

	/** Returns the first valid pose in the AnimationData*/
	FTransform GetFirstValidAnimationPose() const;

	/** Returns a list of animation curves used by this Performance */
	TSet<FString> GetAnimationCurveNames() const;

	/** Returns tooltip text with a reason why processing cant be started */
	FText GetCannotProcessTooltipText() const;

	/** List of all RGB cameras (views) in the footage capture data */
	TArray<TSharedPtr<FString>> CameraNames;

	/** Returns true if the depth camera location is consistent with the RGB camera location, or diagnostics are not enabled */
	bool DepthCameraConsistentWithRGBCameraOrDiagnosticsNotEnabled() const;

	EFrameRangeType GetExcludedFrame(int32 InFrameNumber) const;

	/** Returns a bone position in the reference skeleton - used to account for variations in the heights of different MetaHumans */
	static FVector GetSkelMeshReferenceBoneLocation(USkeletalMeshComponent* InSkelMeshComponent, const FName& InBoneName);

	/** Estimate the focal length of the footage */
	bool EstimateFocalLength(FString &OutErrorMessage);

	UE_INTERNAL FTransform AudioDrivenHeadPoseTransform(const FTransform& InHeadBonePose) const;
	UE_INTERNAL FTransform AudioDrivenHeadPoseTransformInverse(const FTransform& InRootBonePose) const;

private:
#if WITH_EDITOR
	FOnDataInputTypeChanged OnDataInputTypeChangedDelegate;
	FOnSourceDataChanged OnSourceDataChangedDelegate;
	FOnIdentityChanged OnIdentityChangedDelegate;
	FOnVisualizeMeshChanged OnVisualizeMeshChangedDelegate;
	FOnFrameRangeChanged OnFrameRangeChangedDelegate;
	FOnRealtimeAudioChanged OnRealtimeAudioChangedDelegate;
	FOnFrameProcessed OnFrameProcessedDelegate;
	FOnProcessingFinished OnProcessingFinishedDelegate;
	FOnStage1ProcessingFinished OnStage1ProcessingFinishedDelegate;
	FOnControlRigClassChanged OnControlRigClassChangedDelegate;
	FOnHeadMovementModeChanged OnHeadMovementModeChangedDelegate;
	FOnHeadMovementReferenceFrameChanged OnHeadMovementReferenceFrameChangedDelegate;
	FOnNeutralPoseCalibrationChanged OnNeutralPoseCalibrationChangedDelegate;
	FOnExcludedFramesChanged OnExcludedFramesChangedDelegate;
	FFrameRangeArrayBuilder::FOnGetCurrentFrame OnGetCurrentFrameDelegate;

	TArray<TSharedPtr<UE::MetaHuman::Pipeline::FPipeline>> Pipelines;
	TArray<FFrameRange> PipelineFrameRanges;
	TArray<FFrameRange> PipelineExcludedFrames;
	TArray<FFrameRange> RateMatchingExcludedFrames;
	int32 PipelineFrameRangesIndex = 0;
	int32 PipelineStage = 0;
	double PipelineStageStartTime = 0.0;
	FString SolverConfigData;
	FString SolverTemplateData;
	FString SolverDefinitionsData;
	FString SolverHierarchicalDefinitionsData;
	TSharedPtr<UE::MetaHuman::Pipeline::FTongueTrackerNode> TongueSolver;
	TSharedPtr<UE::MetaHuman::Pipeline::FSpeechToAnimNode> SpeechToAnimSolver;
	TSharedPtr<UE::MetaHuman::Pipeline::FHyprsenseRealtimeNode> RealtimeMonoSolver;
	TSharedPtr<UE::MetaHuman::Pipeline::FRealtimeSpeechToAnimNode> RealtimeSpeechToAnimSolver;

	void StartPipelineStage();
	void SendTelemetryForProcessFootageRequest(TSharedPtr<const UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);

	FString TrackingResultsPinName;
	FString AnimationResultsPinName;
	FString DepthMapDiagnosticsResultsPinName;
	FString ScaleDiagnosticsResultsPinName;

	void FrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);
	void ProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);

	void AddSpeechToAnimSolveToPipeline(UE::MetaHuman::Pipeline::FPipeline& InPipeline, TSharedPtr<UE::MetaHuman::Pipeline::FSpeechToAnimNode> InSpeechAnimNode, FString& OutAnimationResultsPinName);
	void AddTongueSolveToPipeline(UE::MetaHuman::Pipeline::FPipeline& InPipeline, TSharedPtr<UE::MetaHuman::Pipeline::FSpeechToAnimNode> InTongueSolveNode, TSharedPtr<UE::MetaHuman::Pipeline::FNode> InInputNode, TSharedPtr<UE::MetaHuman::Pipeline::FDropFrameNode> InDropFrameNode, FString& OutAnimationResultsPinName);
#endif

	void ResetOutput(bool bInWholeSequence);

	bool bBlockingProcessing = false;

	/** Only one performance asset can be processed at the time */
	static TWeakObjectPtr<UMetaHumanPerformance> CurrentlyProcessedPerformance;

	void UpdateFrameRanges();
	float CalculateAudioProcessingOffset();

	TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;
	TRange<FFrameNumber> ProcessingLimitFrameRange = TRange<FFrameNumber>(0, 0);

	void LoadDefaultTracker();
	void LoadDefaultSolver();
	void LoadDefaultControlRig();

	void UpdateCaptureDataConfigName();
	TArray<UE::MetaHuman::FSequencedImageTrackInfo> CreateSequencedImageTrackInfos();
	bool FootageCaptureDataViewLookupsAreValid() const;

	ETimecodeAlignment PreviousTimecodeAlignment = ETimecodeAlignment::None;

	UPROPERTY()
	TArray<FFrameTrackingContourData> ContourTrackingResults_DEPRECATED;

	UPROPERTY()
	TArray<struct FFrameAnimationData> AnimationData_DEPRECATED;

	UPROPERTY()
	TObjectPtr<class USkeletalMesh> OverrideVisualizationMesh_DEPCRECATED;

	bool bMetaHumanAuthoringObjectsPresent = false;

	bool bIsScriptedProcessing = false;
	double ProcessingStartTime = 0.f;

	FString EstimateFocalLengthErrorMessage;
	bool bEstimateFocalLengthOK = false;
	void EstimateFocalLengthFrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);
	void EstimateFocalLengthProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData);

	// Rotation and push back needed so things appear correctly in the viewport
	const FTransform AudioDrivenAnimationViewportTransform = FTransform(FRotator(0, 90, 0), FVector(40.0, 0.0, 0.0));
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceLog.h"
#include "MetaHumanPerformanceExportUtils.h"
#include "MetaHumanPerformanceViewportSettings.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanConformer.h"
#include "MetaHumanFaceContourTrackerAsset.h"
#include "MetaHumanCoreEditorModule.h"
#include "MetaHumanFaceAnimationSolver.h"
#include "MetaHumanTrace.h"
#include "MetaHumanSupportedRHI.h"
#include "MetaHumanAuthoringObjects.h"
#include "MetaHumanFaceTrackerInterface.h"
#include "MetaHumanHeadTransform.h"

#include "CaptureData.h"
#include "CameraCalibration.h"
#include "ImageSequenceUtils.h"
#include "TrackingPathUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "SkelMeshDNAUtils.h"
#include "ImgMediaSource.h"
#include "Algo/AnyOf.h"
#include "Animation/AnimSequence.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/IPluginManager.h"
#include "Nodes/ImageUtilNodes.h"
#include "Nodes/AudioUtilNodes.h"
#include "Nodes/HyprsenseNode.h"
#include "Nodes/HyprsenseRealtimeSmoothingNode.h"
#include "Nodes/NeutralFrameNode.h"
#include "Nodes/DepthMapDiagnosticsNode.h"
#include "Nodes/FaceTrackerNode.h"
#include "Nodes/FaceTrackerPostProcessingNode.h"
#include "Nodes/FaceTrackerPostProcessingFilterNode.h"
#include "Nodes/AnimationUtilNodes.h"
#include "Misc/ScopedSlowTask.h"
#include "Async/Async.h"
#include "DNAUtils.h"
#include "DNAReader.h"
#include "ControlRigBlueprint.h"
#include "Sound/SoundWave.h"
#include "HAL/IConsoleManager.h"
#include "EngineAnalytics.h"
#include "Engine/AssetManager.h"
#include "Dialogs/Dialogs.h"
#include "DoesNNEAssetExist.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SoundWaveTimecodeUtils.h"
#include "Features/IModularFeatures.h"

#include "FramePathResolver.h"

TWeakObjectPtr<UMetaHumanPerformance> UMetaHumanPerformance::CurrentlyProcessedPerformance = nullptr;

#define LOCTEXT_NAMESPACE "MetaHumanPerformance"

namespace
{
	TAutoConsoleVariable<bool> CVarEnableExportTrackingDataSolverPass1
	{
		TEXT("mh.Performance.ExportTrackingDataSolverPass1"),
		false,
		TEXT("Enables exporting MetaHuman Performance tracking data required for debugging for the first pass of the solver"),
		ECVF_Default
	};

	TAutoConsoleVariable<bool> CVarEnableExportTrackingDataSolverPass2
	{
		TEXT("mh.Performance.ExportTrackingDataSolverPass2"),
		false,
		TEXT("Enables exporting MetaHuman Performance tracking data required for debugging for the second pass of the solver"),
		ECVF_Default
	};

	TAutoConsoleVariable<bool> CVarEnableDebugAnimation
	{
		TEXT("mh.Performance.EnableDebugAnimation"),
		false,
		TEXT("Enables writing values that range from 0 to 1 in the animation curves for debugging purposes"),
		ECVF_Default
	};

}

UMetaHumanPerformance::UMetaHumanPerformance()
{
	HeadMovementReferenceFrame = 0;
	bAutoChooseHeadMovementReferenceFrame = 1;

	ViewportSettings = CreateDefaultSubobject<UMetaHumanPerformanceViewportSettings>(TEXT("MetaHuman Performance Viewport Settings"));

	HeadMovementReferenceFrameCalculated = -1;

	bMetaHumanAuthoringObjectsPresent = FMetaHumanAuthoringObjects::ArePresent();

	static constexpr const TCHAR* SmoothingPath = TEXT("/MetaHumanCoreTech/RealtimeMono/DefaultSmoothing.DefaultSmoothing");
	MonoSmoothingParams = LoadObject<UMetaHumanRealtimeSmoothingParams>(GetTransientPackage(), SmoothingPath);

#if WITH_EDITOR
	OnProcessingFinishedDelegate.AddUObject(this, &UMetaHumanPerformance::SendTelemetryForProcessFootageRequest);
#endif
}

void UMetaHumanPerformance::BeginDestroy()
{
	for (TSharedPtr<UE::MetaHuman::Pipeline::FPipeline> Pipeline : Pipelines)
	{
		Pipeline->Reset();
	}

	Super::BeginDestroy();
}

FPrimaryAssetId UMetaHumanPerformance::GetPrimaryAssetId() const
{
	// Check if we are an asset or a blueprint CDO
	if (FCoreUObjectDelegates::GetPrimaryAssetIdForObject.IsBound() &&
		(IsAsset() || (HasAnyFlags(RF_ClassDefaultObject) && !GetClass()->HasAnyClassFlags(CLASS_Native)))
		)
	{
		// Call global callback if bound
		return FCoreUObjectDelegates::GetPrimaryAssetIdForObject.Execute(this);
	}

	return FPrimaryAssetId(GetClass()->GetFName(), GetFName());
}

void UMetaHumanPerformance::PreEditChange(FEditPropertyChain& InPropertyAboutToChange)
{
	Super::PreEditChange(InPropertyAboutToChange);

	PreviousTimecodeAlignment = TimecodeAlignment;
}

void UMetaHumanPerformance::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	if (FProperty* Property = InPropertyChangedEvent.Property)
	{
		const FName PropertyName = *Property->GetName();

		const bool bDataInputTypeChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, InputType);
		const bool bFootageCaptureDataChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, FootageCaptureData);
		const bool bAudioChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, Audio);
		const bool bCameraChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, Camera);
		bool bTimecodeAlignmentChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, TimecodeAlignment);
		const bool bIdentityChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, Identity);
		const bool bVisualizeMeshChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, VisualizationMesh);
		const bool bStartFrameChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, StartFrameToProcess);
		const bool bEndFrameChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, EndFrameToProcess);
		const bool bRealtimeAudioChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bRealtimeAudio);
		const bool bControlRigClassChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, ControlRigClass);
		const bool bDefaultSolverChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, DefaultSolver);
		const bool bHeadMovementModeChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, HeadMovementMode);
		const bool bAutoChooseHeadMovementReferenceFrameChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bAutoChooseHeadMovementReferenceFrame);
		const bool bHeadMovementReferenceFrameChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, HeadMovementReferenceFrame);
		const bool bNeutralPoseCalibrationEnabledChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bNeutralPoseCalibrationEnabled);
		const bool bNeutralPoseCalibrationFrameChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, NeutralPoseCalibrationFrame);
		const bool bNeutralPoseCalibrationAlphaChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, NeutralPoseCalibrationAlpha);
		const bool bNeutralPoseCalibrationCurvesChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, NeutralPoseCalibrationCurves);
		const bool bExcludedFrameChanged = PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, UserExcludedFrames);

		if (bTimecodeAlignmentChanged && ContainsAnimationData() && (TimecodeAlignment == ETimecodeAlignment::None || PreviousTimecodeAlignment == ETimecodeAlignment::None))
		{
			FSuppressableWarningDialog::FSetupInfo Info(
						LOCTEXT("ChangePerformanceTimecodeAlignment", "Changing the timecode alignment will delete processed results"),
						LOCTEXT("ChangePerformanceTimecodeAlignmentTitle", "Change timecode alignment"),
						TEXT("ChangePerformanceTimecodeAlignment"));

			Info.ConfirmText = LOCTEXT("ChangePerformanceTimecode_ConfirmText", "Ok");
			Info.CancelText = LOCTEXT("ChangePerformanceTimecode_CancelText", "Cancel");

			FSuppressableWarningDialog ShouldChangeTimecodeAlignmentDialog(Info);
			FSuppressableWarningDialog::EResult UserInput = ShouldChangeTimecodeAlignmentDialog.ShowModal();

			if (UserInput == FSuppressableWarningDialog::EResult::Cancel)
			{
				TimecodeAlignment = PreviousTimecodeAlignment;

				bTimecodeAlignmentChanged = false;
			}
		}

		if (bFootageCaptureDataChanged)
		{
			Camera.Reset();
			UFootageCaptureData::PopulateCameraNames(FootageCaptureData, Camera, CameraNames);
			FocalLength = -1;
		}

		if (bControlRigClassChanged && !ControlRigClass) // Covers the case where ControlRigClass is reset to default value
		{
			LoadDefaultControlRig();
		}

		if (bDataInputTypeChanged|| bFootageCaptureDataChanged || bAudioChanged || (bTimecodeAlignmentChanged && (TimecodeAlignment == ETimecodeAlignment::None || PreviousTimecodeAlignment == ETimecodeAlignment::None)))
		{
			UpdateFrameRanges();

			StartFrameToProcess = ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			EndFrameToProcess = ProcessingLimitFrameRange.GetUpperBoundValue().Value;
			HeadMovementReferenceFrame = FMath::Clamp(HeadMovementReferenceFrame, StartFrameToProcess, EndFrameToProcess);
			NeutralPoseCalibrationFrame = FMath::Clamp(NeutralPoseCalibrationFrame, StartFrameToProcess, EndFrameToProcess);

			ResetOutput(true);
			UserExcludedFrames.Reset();

			const bool bResetRanges = true;
			OnDataInputTypeChangedDelegate.Broadcast(InputType);
			OnSourceDataChangedDelegate.Broadcast(FootageCaptureData, GetAudioForProcessing(), bResetRanges);
			OnFrameRangeChangedDelegate.Broadcast(StartFrameToProcess, EndFrameToProcess);
			OnIdentityChangedDelegate.Broadcast(Identity); // to wipe sequencer keys

			UpdateCaptureDataConfigName();

			if (FootageCaptureData)
			{
				FootageCaptureData->OnCaptureDataInternalsChanged().AddUObject(this, &UMetaHumanPerformance::UpdateCaptureDataConfigName);
			}
		}
		else if (bCameraChanged)
		{
			UFootageCaptureData::PopulateCameraNames(FootageCaptureData, Camera, CameraNames);
			FocalLength = -1;

			const bool bResetRanges = false;
			OnSourceDataChangedDelegate.Broadcast(FootageCaptureData, GetAudioForProcessing(), bResetRanges);
			OnFrameRangeChangedDelegate.Broadcast(StartFrameToProcess, EndFrameToProcess);
			OnVisualizeMeshChangedDelegate.Broadcast(VisualizationMesh); // to regenerate sequencer keys
		}
		else if (bTimecodeAlignmentChanged)
		{
			const int32 StartFrameToProcessOffset = StartFrameToProcess - ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			const int32 EndFrameToProcessOffset = EndFrameToProcess - ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			const int32 HeadMovementReferenceFrameOffset = HeadMovementReferenceFrame - ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			const int32 NeutralPoseCalibrationFrameOffset = NeutralPoseCalibrationFrame - ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			int32 ExcludedFramesShift = ProcessingLimitFrameRange.GetLowerBoundValue().Value;

			UpdateFrameRanges();

			StartFrameToProcess = StartFrameToProcessOffset + ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			EndFrameToProcess = EndFrameToProcessOffset + ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			HeadMovementReferenceFrame = HeadMovementReferenceFrameOffset + ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			NeutralPoseCalibrationFrame = NeutralPoseCalibrationFrameOffset + ProcessingLimitFrameRange.GetLowerBoundValue().Value;
			ExcludedFramesShift = ProcessingLimitFrameRange.GetLowerBoundValue().Value - ExcludedFramesShift;

			for (TArray<FFrameRange>* ExcludedFrames : { &UserExcludedFrames, &ProcessingExcludedFrames })
			{
				for (int32 Index = 0; Index < ExcludedFrames->Num(); ++Index)
				{
					if ((*ExcludedFrames)[Index].StartFrame >= 0)
					{
						(*ExcludedFrames)[Index].StartFrame += ExcludedFramesShift;
					}

					if ((*ExcludedFrames)[Index].EndFrame >= 0)
					{
						(*ExcludedFrames)[Index].EndFrame += ExcludedFramesShift;
					}
				}
			}

			const bool bResetRanges = true;
			OnSourceDataChangedDelegate.Broadcast(FootageCaptureData, GetAudioForProcessing(), bResetRanges);
			OnFrameRangeChangedDelegate.Broadcast(StartFrameToProcess, EndFrameToProcess);
			OnVisualizeMeshChangedDelegate.Broadcast(VisualizationMesh); // to regenerate sequencer keys
		}
		else if (bIdentityChanged)
		{
			ResetOutput(true);

			OnIdentityChangedDelegate.Broadcast(Identity);
		}
		else if (bVisualizeMeshChanged)
		{
			OnVisualizeMeshChangedDelegate.Broadcast(VisualizationMesh);
		}
		else if (bStartFrameChanged || bEndFrameChanged)
		{
			if (bStartFrameChanged)
			{
				StartFrameToProcess = FMath::Clamp(StartFrameToProcess, ProcessingLimitFrameRange.GetLowerBoundValue().Value, EndFrameToProcess);
			}
			else
			{
				EndFrameToProcess = FMath::Clamp(EndFrameToProcess, StartFrameToProcess, ProcessingLimitFrameRange.GetUpperBoundValue().Value);
			}

			OnFrameRangeChangedDelegate.Broadcast(StartFrameToProcess, EndFrameToProcess);
			
			uint32 NewHeadMovementReferenceFrame = FMath::Clamp(HeadMovementReferenceFrame, StartFrameToProcess, EndFrameToProcess);
			if (NewHeadMovementReferenceFrame != HeadMovementReferenceFrame)
			{
				HeadMovementReferenceFrame = NewHeadMovementReferenceFrame;
				OnHeadMovementReferenceFrameChangedDelegate.Broadcast(bAutoChooseHeadMovementReferenceFrame, NewHeadMovementReferenceFrame);
			}

			uint32 NewNeutralPoseCalibrationFrame = FMath::Clamp(NeutralPoseCalibrationFrame, StartFrameToProcess, EndFrameToProcess);
			if (NewNeutralPoseCalibrationFrame != NeutralPoseCalibrationFrame)
			{
				NeutralPoseCalibrationFrame = NewNeutralPoseCalibrationFrame;
				OnNeutralPoseCalibrationChangedDelegate.Broadcast();
			}
		}
		else if (bRealtimeAudioChanged)
		{
			OnRealtimeAudioChangedDelegate.Broadcast(bRealtimeAudio);
		}
		else if (bControlRigClassChanged)
		{
			OnControlRigClassChangedDelegate.Broadcast(ControlRigClass); 
		}
		else if (bDefaultSolverChanged)
		{
			UpdateCaptureDataConfigName();

			if (DefaultSolver)
			{
				DefaultSolver->OnInternalsChanged().AddUObject(this, &UMetaHumanPerformance::UpdateCaptureDataConfigName);
			}
		}
		else if (bHeadMovementModeChanged) 
		{
			OnHeadMovementModeChangedDelegate.Broadcast(HeadMovementMode);
			// make sure we update the control rig head reference frame if needed
			if (HeadMovementMode == EPerformanceHeadMovementMode::ControlRig)
			{
				OnHeadMovementReferenceFrameChangedDelegate.Broadcast(bAutoChooseHeadMovementReferenceFrame, HeadMovementReferenceFrame);
			}
		}
		else if (bAutoChooseHeadMovementReferenceFrameChanged || bHeadMovementReferenceFrameChanged)
		{
			HeadMovementReferenceFrame = FMath::Clamp(HeadMovementReferenceFrame, StartFrameToProcess, EndFrameToProcess);
			OnHeadMovementReferenceFrameChangedDelegate.Broadcast(bAutoChooseHeadMovementReferenceFrame, HeadMovementReferenceFrame);
		}
		else if (bNeutralPoseCalibrationEnabledChanged || bNeutralPoseCalibrationFrameChanged || bNeutralPoseCalibrationAlphaChanged || bNeutralPoseCalibrationCurvesChanged)
		{
			NeutralPoseCalibrationFrame = FMath::Clamp(NeutralPoseCalibrationFrame, StartFrameToProcess, EndFrameToProcess);
			OnNeutralPoseCalibrationChangedDelegate.Broadcast();
		}
		else if (bExcludedFrameChanged)
		{
			OnExcludedFramesChangedDelegate.Broadcast();
		}
	}
}

void UMetaHumanPerformance::PostEditUndo()
{
	Super::PostEditUndo();

	UpdateFrameRanges();

	UFootageCaptureData::PopulateCameraNames(FootageCaptureData, Camera, CameraNames);
}

void UMetaHumanPerformance::PostInitProperties()
{
	Super::PostInitProperties();

	LoadDefaultTracker();
	LoadDefaultSolver();
	LoadDefaultControlRig();
}

void UMetaHumanPerformance::PostLoad()
{
	Super::PostLoad();

	if (ControlRig_DEPRECATED)
	{
		ControlRigClass = ControlRig_DEPRECATED->GetControlRigClass();
		ControlRig_DEPRECATED = nullptr;
	}

	if (OverrideVisualizationMesh_DEPCRECATED)
	{
		VisualizationMesh = OverrideVisualizationMesh_DEPCRECATED;
		OverrideVisualizationMesh_DEPCRECATED = nullptr;
	}

	LoadDefaultTracker();
	LoadDefaultSolver();
	LoadDefaultControlRig();
	UpdateFrameRanges();

	UpdateCaptureDataConfigName();

	if (FootageCaptureData)
	{
		FootageCaptureData->OnCaptureDataInternalsChanged().AddUObject(this, &UMetaHumanPerformance::UpdateCaptureDataConfigName);
	}

	if (DefaultSolver)
	{
		DefaultSolver->OnInternalsChanged().AddUObject(this, &UMetaHumanPerformance::UpdateCaptureDataConfigName);
	}

	UFootageCaptureData::PopulateCameraNames(FootageCaptureData, Camera, CameraNames);
}

void UMetaHumanPerformance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!ContourTrackingResults_DEPRECATED.IsEmpty())
	{
		ContourTrackingResults = ContourTrackingResults_DEPRECATED;
		ContourTrackingResults_DEPRECATED.Empty();
	}
	else
	{
		Ar << ContourTrackingResults;
	}

	if (!AnimationData_DEPRECATED.IsEmpty())
	{
		AnimationData = AnimationData_DEPRECATED;
		AnimationData_DEPRECATED.Empty();
	}
	else
	{
		Ar << AnimationData;
	}
}

bool UMetaHumanPerformance::CanExportAnimation() const
{
	// Animation data must be present
	return ContainsAnimationData();
}

void UMetaHumanPerformance::ExportAnimation(EPerformanceExportRange InExportRange)
{
	if (!CanExportAnimation())
	{
		return;
	}

	UMetaHumanPerformanceExportUtils::ExportAnimationSequence(this);
}

FFrameRate UMetaHumanPerformance::GetFrameRate() const
{
	if (FootageCaptureData && !FootageCaptureData->ImageSequences.IsEmpty() && FootageCaptureData->ImageSequences[0])
	{
		return FootageCaptureData->ImageSequences[0]->FrameRateOverride;
	}
	else
	{
		return FFrameRate(30, 1); // Default frame rate if no image sequence
	}
}

TArray<UE::MetaHuman::FSequencedImageTrackInfo> UMetaHumanPerformance::CreateSequencedImageTrackInfos()
{
	using namespace UE::MetaHuman;

	if (!IsValid(FootageCaptureData))
	{
		return {};
	}

	TArray<const UImgMediaSource*> ImageMediaSources;
	ImageMediaSources.Append(FootageCaptureData->ImageSequences);
	ImageMediaSources.Append(FootageCaptureData->DepthSequences);

	TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos;
	SequencedImageTrackInfos.Reserve(ImageMediaSources.Num());

	for (const UImgMediaSource* ImageMediaSource : ImageMediaSources)
	{
		if (IsValid(ImageMediaSource))
		{
			if (const TRange<FFrameNumber>* MediaRange = MediaFrameRanges.Find(ImageMediaSource))
			{
				SequencedImageTrackInfos.Emplace(ImageMediaSource->FrameRateOverride, *MediaRange);
			}
		}
	}

	return SequencedImageTrackInfos;
}

void UMetaHumanPerformance::UpdateFrameRanges()
{
	MediaFrameRanges.Reset();
	ProcessingLimitFrameRange = TRange<FFrameNumber>(0, 0);
	TRange<FFrameNumber> MaxFrameRange(0, 0);

	FFrameRate FrameRate = GetFrameRate();

	if (InputType == EDataInputType::Audio)
	{
		if (TObjectPtr<class USoundWave> AudioForProcessing = GetAudioForProcessing())
		{
			TRange<FFrameNumber> AudioFrameRange = UFootageCaptureData::GetAudioFrameRange(FrameRate, TimecodeAlignment, AudioForProcessing, GetAudioMediaTimecode(), GetAudioMediaTimecodeRate());

			MaxFrameRange = AudioFrameRange;
			ProcessingLimitFrameRange = AudioFrameRange;
			MediaFrameRanges.Add(AudioForProcessing, AudioFrameRange);
		}
	}
	else if (FootageCaptureData && FootageCaptureData->IsInitialized(UFootageCaptureData::EInitializedCheck::ImageSequencesOnly))
	{
		FootageCaptureData->GetFrameRanges(FrameRate, TimecodeAlignment, true, MediaFrameRanges, ProcessingLimitFrameRange, MaxFrameRange);
	}
}

float UMetaHumanPerformance::CalculateAudioProcessingOffset()
{
	uint32 OffsetInFrames = StartFrameToProcess - MediaFrameRanges[GetAudioForProcessing()].GetLowerBoundValue().Value;
	return OffsetInFrames / GetFrameRate().AsDecimal();
}

EStartPipelineErrorType UMetaHumanPerformance::StartPipeline(bool bInIsScriptedProcessing)
{
	Pipelines.Reset();
	PipelineFrameRangesIndex = 0;
	PipelineFrameRanges.Reset();
	PipelineExcludedFrames.Reset();
	PipelineStage = 0;

	if (!CanProcess())
	{
		return EStartPipelineErrorType::Disabled;
	}

	if (InputType == EDataInputType::DepthFootage || InputType == EDataInputType::MonoFootage)
	{
		if (!FootageCaptureDataViewLookupsAreValid())
		{
			return EStartPipelineErrorType::Disabled;
		}
	}

	if (InputType == EDataInputType::DepthFootage)
	{
		using namespace UE::MetaHuman;

		TArray<FSequencedImageTrackInfo> SequencedImageTrackInfos = CreateSequencedImageTrackInfos();

		if (!SequencedImageTrackInfos.IsEmpty())
		{
			const bool bTracksHaveDifferentFrameRates = TracksHaveDifferentFrameRates(SequencedImageTrackInfos);
			const bool bTracksHaveCompatibleFrameRates = TracksHaveCompatibleFrameRates(SequencedImageTrackInfos);

			if (bTracksHaveDifferentFrameRates && bTracksHaveCompatibleFrameRates)
			{
				const FFrameRate TargetFrameRate = GetFrameRate();
				const TArray<FFrameNumber> RateMatchingDropFrames = CalculateRateMatchingDropFrames(
					TargetFrameRate, 
					MoveTemp(SequencedImageTrackInfos), 
					ProcessingLimitFrameRange
				);

				RateMatchingExcludedFrames = PackIntoFrameRanges(RateMatchingDropFrames);

				UE_LOG(
					LogMetaHumanPerformance,
					Warning,
					TEXT(
						"Detected mismatch in image media frame rates. We need to exclude some frames from processing to make sure everything "
						"is paired up correctly (%s)"
					),
					*GetPathName()
				);
			}
		}
	}

	// Calculate the frames ranges to process. The logic here is first to find the excluded frame ranges in 
	// the processing frame range merging contiguous frame ranges. Then treat these excluded frame ranges as follows:
	// 1. if only excluding a single frame... skip this frame when processing 
	// 2. if excluding more than a single frame... split processing into multiple, independent, runs.

	TArray<FFrameRange> CombinedExcludedFrames;
	FFrameRange CurrentExcludedFrameRange;
	for (uint32 Frame = StartFrameToProcess; Frame < EndFrameToProcess; ++Frame)
	{
		if (FFrameRange::ContainsFrame(Frame, UserExcludedFrames) || FFrameRange::ContainsFrame(Frame, RateMatchingExcludedFrames) ||
			(InputType != EDataInputType::Audio && FootageCaptureData && FFrameRange::ContainsFrame(Frame - GetMediaStartFrame().Value, FootageCaptureData->CaptureExcludedFrames)))
		{
			if (CurrentExcludedFrameRange.StartFrame == -1)
			{
				CurrentExcludedFrameRange.StartFrame = Frame;
			}
			else if (CurrentExcludedFrameRange.EndFrame != Frame - 1)
			{
				CombinedExcludedFrames.Add(CurrentExcludedFrameRange);

				CurrentExcludedFrameRange.StartFrame = Frame;
			}

			CurrentExcludedFrameRange.EndFrame = Frame;
		}
	}

	if (CurrentExcludedFrameRange.StartFrame != -1)
	{
		CombinedExcludedFrames.Add(CurrentExcludedFrameRange);
	}

	for (int32 Index = CombinedExcludedFrames.Num() - 1; Index >= 0; --Index)
	{
		if (CombinedExcludedFrames[Index].StartFrame == CombinedExcludedFrames[Index].EndFrame)
		{
			PipelineExcludedFrames.Add(CombinedExcludedFrames[Index]);
			CombinedExcludedFrames.RemoveAt(Index);
		}
	}

	FFrameRange CurrentIncludedFrameRange;
	for (uint32 Frame = StartFrameToProcess; Frame < EndFrameToProcess; ++Frame)
	{
		if (!FFrameRange::ContainsFrame(Frame, CombinedExcludedFrames))
		{
			if (CurrentIncludedFrameRange.StartFrame == -1)
			{
				CurrentIncludedFrameRange.StartFrame = Frame;
			}
			else if (CurrentIncludedFrameRange.EndFrame != Frame - 1)
			{
				CurrentIncludedFrameRange.EndFrame++;
				PipelineFrameRanges.Add(CurrentIncludedFrameRange);

				CurrentIncludedFrameRange.StartFrame = Frame;
			}

			CurrentIncludedFrameRange.EndFrame = Frame;
		}
	}

	if (CurrentIncludedFrameRange.StartFrame != -1)
	{
		CurrentIncludedFrameRange.EndFrame++;
		PipelineFrameRanges.Add(CurrentIncludedFrameRange);
	}

	if (PipelineFrameRanges.IsEmpty())
	{
		UE_LOG(LogMetaHumanPerformance, Warning, TEXT("No frame to process!"));
		return EStartPipelineErrorType::NoFrames;
	}

	// End of calculating frame ranges

	// Note obtaining the solver data will cause a LoadObject which is only valid in the game thread
	// and while asset saving and garbage collection is not running. Doing it once here
	// prevents problems.
	SolverConfigData = DefaultSolver->GetSolverConfigData(FootageCaptureData);
	SolverTemplateData = DefaultSolver->GetSolverTemplateData(FootageCaptureData);
	SolverDefinitionsData = DefaultSolver->GetSolverDefinitionsData(FootageCaptureData);
	SolverHierarchicalDefinitionsData = DefaultSolver->GetSolverHierarchicalDefinitionsData(FootageCaptureData);

	// Setup speech to anim or tongue solver node here since, like the above, it does a LoadObject so we need to
	// avoid doing it in later, when the node is actually required, since asset saving and garbage
	// collection maybe active.
	if (InputType == EDataInputType::Audio)
	{
		if (bRealtimeAudio)
		{
			RealtimeSpeechToAnimSolver = MakeShared<UE::MetaHuman::Pipeline::FRealtimeSpeechToAnimNode>("RealtimeSpeechToAnimSolver");
			RealtimeSpeechToAnimSolver->LoadModels();
		}
		else
		{
			SpeechToAnimSolver = MakeShared<UE::MetaHuman::Pipeline::FSpeechToAnimNode>("SpeechToAnimSolver");
			SpeechToAnimSolver->LoadModels(AudioDrivenAnimationModels);

			SpeechToAnimSolver->SetMood(AudioDrivenAnimationSolveOverrides.Mood);
			SpeechToAnimSolver->SetMoodIntensity(AudioDrivenAnimationSolveOverrides.MoodIntensity);
			SpeechToAnimSolver->SetOutputControls(AudioDrivenAnimationOutputControls);
		}
	}
	else if (InputType == EDataInputType::DepthFootage && !bSkipTongueSolve && GetAudioForProcessing() != nullptr)
	{
		TongueSolver = MakeShared<UE::MetaHuman::Pipeline::FTongueTrackerNode>("TongueSolver");
		TongueSolver->LoadModels();
	}

	// Setup realtime mono solver node here since, like the above, it does a LoadObject
	if (InputType == EDataInputType::MonoFootage)
	{
		RealtimeMonoSolver = MakeShared<UE::MetaHuman::Pipeline::FHyprsenseRealtimeNode>("RealtimeMonoSolver");
		RealtimeMonoSolver->LoadModels();
		RealtimeMonoSolver->SetHeadStabilization(bHeadStabilization);
		RealtimeMonoSolver->SetFocalLength(FocalLength);
	}

	CurrentlyProcessedPerformance = this;
	HeadMovementReferenceFrameCalculated = -1;
	ScaleEstimate = -1;
	ResetOutput(false);

	ProcessingStartTime = FPlatformTime::Seconds();
	bIsScriptedProcessing = bInIsScriptedProcessing;

	if (InputType == EDataInputType::Audio || InputType == EDataInputType::MonoFootage)
	{
		StartPipelineStage();
	}
	else if (bBlockingProcessing)
	{
		bool bTrackersLoaded = DefaultTracker->LoadTrackersSynchronous();

		check(bTrackersLoaded);

		StartPipelineStage();
	}
	else
	{
		DefaultTracker->LoadTrackers(true, [this](bool bTrackersLoaded)
		{
			check(bTrackersLoaded);

#if WITH_EDITOR
			GEditor->GetTimerManager()->SetTimerForNextTick([this]()
			{
				StartPipelineStage();
			});
#endif
		});
	}

	return EStartPipelineErrorType::None;
}

static void WriteRigDNA(TWeakObjectPtr<UDNAAsset> InDNAAsset, const FString & InDebuggingFolder)
{
	if (InDNAAsset.Get())
	{
		TArray<uint8> DNABuffer = UE::Wrappers::FMetaHumanConformer::DNAToBuffer(InDNAAsset.Get());
		TSharedPtr<class IDNAReader> DNAReader = ReadDNAFromBuffer(&DNABuffer);
		const FString PathToDNAFile = InDebuggingFolder / FString(TEXT("tracking_rig.dna"));
		WriteDNAToFile(DNAReader.Get(), EDNADataLayer::All, PathToDNAFile);
	}
}

void UMetaHumanPerformance::StartPipelineStage()
{
	Pipelines.Add(MakeShared<UE::MetaHuman::Pipeline::FPipeline>());
	int32 PipelineIndex = Pipelines.Num() - 1;
	UE::MetaHuman::Pipeline::FPipeline& Pipeline = *(Pipelines[PipelineIndex].Get());
	check(!Pipeline.IsRunning());
	Pipeline.Reset();

	PipelineStageStartTime = FPlatformTime::Seconds();

	TSharedPtr<UE::MetaHuman::Pipeline::FDropFrameNode> DropFrame = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FDropFrameNode>("DropFrame");
	DropFrame->ExcludedFrames = PipelineExcludedFrames;

	if (PipelineStage == 0 && InputType == EDataInputType::Audio) // Audio only pipeline
	{
		check(GetAudioForProcessing());
		
		if (bRealtimeAudio)
		{
			TSharedPtr<UE::MetaHuman::Pipeline::FAudioLoadNode> AudioInput = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FAudioLoadNode>("Audio");
			AudioInput->Load(GetAudioForProcessing());
			AudioInput->FrameRate = GetFrameRate().AsDecimal();
			AudioInput->FrameOffset = StartFrameToProcess - MediaFrameRanges[GetAudioForProcessing()].GetLowerBoundValue().Value;

			TSharedPtr<UE::MetaHuman::Pipeline::FAudioConvertNode> Convert = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FAudioConvertNode>("Convert");
			Convert->NumChannels = 1;
			Convert->SampleRate = 16000;

			Pipeline.AddNode(RealtimeSpeechToAnimSolver);

			Pipeline.MakeConnection(AudioInput, Convert);
			Pipeline.MakeConnection(Convert, RealtimeSpeechToAnimSolver);

			AnimationResultsPinName = RealtimeSpeechToAnimSolver->Name + ".Animation Out";
		}
		else
		{
			AddSpeechToAnimSolveToPipeline(Pipeline, SpeechToAnimSolver, AnimationResultsPinName);
		}
	}
	else if (PipelineStage == 0 && InputType == EDataInputType::MonoFootage) // Realtime pipeline
	{
		check(FootageCaptureData);
		check(FootageCaptureData->ImageSequences.Num() != 0);
		const int32 ViewIndex = FootageCaptureData->GetViewIndexByCameraName(Camera);
		check(ViewIndex >= 0 && ViewIndex < FootageCaptureData->ImageSequences.Num());

		const UImgMediaSource* ImageSequence = FootageCaptureData->ImageSequences[ViewIndex];
		check(ImageSequence);

		TSharedPtr<UE::MetaHuman::Pipeline::FUEImageLoadNode> Color = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FUEImageLoadNode>("Color");
		Color->bFailOnMissingFile = true;

		FString ColorFilePath;
		int32 ColorFrameOffset = 0;
		int32 ColorNumFrames = 0;

		if (!FTrackingPathUtils::GetTrackingFilePathAndInfo(ImageSequence, ColorFilePath, ColorFrameOffset, ColorNumFrames))
		{
			TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> FailPipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
			FailPipelineData->SetExitStatus(UE::MetaHuman::Pipeline::EPipelineExitStatus::ProcessError);
			FailPipelineData->SetErrorMessage(FString::Printf(TEXT("Failed to find image sequence in file path %s"), *ImageSequence->GetFullPath()));
			FailPipelineData->SetErrorNodeName(Color->Name);
			FailPipelineData->SetErrorNodeCode(UE::MetaHuman::Pipeline::FUEImageLoadNode::ErrorCode::BadFilePath);
			CurrentlyProcessedPerformance.Reset();
			OnProcessingFinishedDelegate.Broadcast(FailPipelineData);
			return;
		}

		const int32 FrameNumberOffset = ColorFrameOffset - MediaFrameRanges[ImageSequence].GetLowerBoundValue().Value;
		UE::MetaHuman::FFrameNumberTransformer FrameNumberTransformer(FrameNumberOffset);
		Color->FramePathResolver = MakeUnique<UE::MetaHuman::FFramePathResolver>(ColorFilePath, MoveTemp(FrameNumberTransformer));

		Pipeline.MakeConnection(DropFrame, Color);

		TSharedPtr<UE::MetaHuman::Pipeline::FNeutralFrameNode> NeutralFrame = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FNeutralFrameNode>("Neutral Frame");

		Pipeline.MakeConnection(Color, NeutralFrame);

		Pipeline.AddNode(RealtimeMonoSolver);

		Pipeline.MakeConnection(NeutralFrame, RealtimeMonoSolver);

		if (MonoSmoothingParams)
		{
			TSharedPtr<UE::MetaHuman::Pipeline::FHyprsenseRealtimeSmoothingNode> Smoothing = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FHyprsenseRealtimeSmoothingNode>("Smoothing");
			Smoothing->Parameters = MonoSmoothingParams->Parameters;
			Smoothing->DeltaTime = GetFrameRate().AsInterval();

			Pipeline.MakeConnection(RealtimeMonoSolver, Smoothing);

			AnimationResultsPinName = Smoothing->Name + ".Animation Out";
		}
		else
		{
			AnimationResultsPinName = RealtimeMonoSolver->Name + ".Animation Out";
		}
	}
	else if (PipelineStage == 0)
	{
		check(FootageCaptureData);
		check(FootageCaptureData->ImageSequences.Num() != 0);
		check(!FootageCaptureData->CameraCalibrations.IsEmpty());
		check(!FootageCaptureData->CameraCalibrations[0]->CameraCalibrations.IsEmpty());

		const int32 ViewIndex = FootageCaptureData->GetViewIndexByCameraName(Camera);
		check(ViewIndex >= 0 && ViewIndex < FootageCaptureData->ImageSequences.Num() && ViewIndex < FootageCaptureData->DepthSequences.Num());

		const UImgMediaSource* ImageSequence = FootageCaptureData->ImageSequences[ViewIndex];
		check(ImageSequence);

		const UImgMediaSource* DepthSequence = FootageCaptureData->DepthSequences[ViewIndex];
		check(DepthSequence);

		const FFrameRate TargetFrameRate = GetFrameRate();

		FString ColorFilePath;
		int32 ColorFrameOffset = 0;
		int32 ColorNumFrames = 0;
		FTrackingPathUtils::GetTrackingFilePathAndInfo(ImageSequence, ColorFilePath, ColorFrameOffset, ColorNumFrames);

		FString DepthFilePath;
		int32 DepthFrameOffset = 0;
		int32 DepthNumFrames = 0;
		FTrackingPathUtils::GetTrackingFilePathAndInfo(DepthSequence, DepthFilePath, DepthFrameOffset, DepthNumFrames);

		TSharedPtr<UE::MetaHuman::Pipeline::FUEImageLoadNode> Color = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FUEImageLoadNode>("Color");
		Color->bFailOnMissingFile = true;

		if (UE::MetaHuman::FrameRatesAreCompatible(ImageSequence->FrameRateOverride, TargetFrameRate))
		{
			const int32 FrameNumberOffset = ColorFrameOffset - MediaFrameRanges[ImageSequence].GetLowerBoundValue().Value;
			UE::MetaHuman::FFrameNumberTransformer FrameNumberTransformer(ImageSequence->FrameRateOverride, TargetFrameRate, FrameNumberOffset);
			Color->FramePathResolver = MakeUnique<UE::MetaHuman::FFramePathResolver>(ColorFilePath, MoveTemp(FrameNumberTransformer));
		}
		else
		{ 
			TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> FailPipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
			FailPipelineData->SetExitStatus(UE::MetaHuman::Pipeline::EPipelineExitStatus::ProcessError);
			FailPipelineData->SetErrorMessage(
				FString::Printf(
					TEXT("Failed to create the frame path resolver for the image load node. The image frame rate (%.2f) is incompatible with the target frame rate (%.2f)"),
					ImageSequence->FrameRateOverride.AsDecimal(),
					TargetFrameRate.AsDecimal()
				)
			);
			FailPipelineData->SetErrorNodeName(Color->Name);
			FailPipelineData->SetErrorNodeCode(UE::MetaHuman::Pipeline::FUEImageLoadNode::ErrorCode::NoFramePathResolver);
			CurrentlyProcessedPerformance.Reset();
			OnProcessingFinishedDelegate.Broadcast(FailPipelineData);
			return;
		}

		Pipeline.MakeConnection(DropFrame, Color);

		TSharedPtr<UE::MetaHuman::Pipeline::FNode> GenericTracker = nullptr;

		TSharedPtr<UE::MetaHuman::Pipeline::FHyprsenseNode> OfflineTracker = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FHyprsenseNode>("GenericTracker");
		GenericTracker = OfflineTracker;

		bool bSetTrackersSuccessfully = OfflineTracker->SetTrackers(DefaultTracker->FullFaceTracker,
			DefaultTracker->FaceDetector,
			DefaultTracker->BrowsDenseTracker,
			DefaultTracker->EyesDenseTracker,
			DefaultTracker->MouthDenseTracker,
			DefaultTracker->LipzipDenseTracker,
			DefaultTracker->NasioLabialsDenseTracker,
			DefaultTracker->ChinDenseTracker,
			DefaultTracker->TeethDenseTracker,
			DefaultTracker->TeethConfidenceTracker);
		if (!bSetTrackersSuccessfully)
		{
			// a standard pipeline 'Failed to start' error will be triggered but we display this information in the log 
			// so that the user can act (for example if a custom tracker asset has not been set up correctly)
			UE_LOG(LogMetaHumanPerformance, Error, TEXT("%s"), *OfflineTracker->GetErrorMessage());
		}

		Pipeline.MakeConnection(Color, GenericTracker);

		TSharedPtr<UE::MetaHuman::Pipeline::FNode> Tracker = GenericTracker;

		TrackingResultsPinName = Tracker->Name + ".Contours Out";

		TSharedPtr<UE::MetaHuman::Pipeline::FDepthLoadNode> Depth = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FDepthLoadNode>("Depth");
		Depth->bFailOnMissingFile = true;

		if (UE::MetaHuman::FrameRatesAreCompatible(DepthSequence->FrameRateOverride, TargetFrameRate))
		{
			const int32 FrameNumberOffset = DepthFrameOffset - MediaFrameRanges[DepthSequence].GetLowerBoundValue().Value;
			UE::MetaHuman::FFrameNumberTransformer FrameNumberTransformer(DepthSequence->FrameRateOverride, TargetFrameRate, FrameNumberOffset);
			Depth->FramePathResolver = MakeUnique<UE::MetaHuman::FFramePathResolver>(DepthFilePath, MoveTemp(FrameNumberTransformer));
		}
		else
		{
			TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> FailPipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
			FailPipelineData->SetExitStatus(UE::MetaHuman::Pipeline::EPipelineExitStatus::ProcessError);
			FailPipelineData->SetErrorMessage(
				FString::Printf(
					TEXT("Failed to create the frame path resolver for the depth node. The depth frame rate (%.2f) is incompatible with the target frame rate (%.2f)"),
					DepthSequence->FrameRateOverride.AsDecimal(),
					TargetFrameRate.AsDecimal()
				)
			);
			FailPipelineData->SetErrorNodeName(Color->Name);
			FailPipelineData->SetErrorNodeCode(UE::MetaHuman::Pipeline::FDepthLoadNode::ErrorCode::NoFramePathResolver);
			CurrentlyProcessedPerformance.Reset();
			OnProcessingFinishedDelegate.Broadcast(FailPipelineData);
			return;
		}

		Pipeline.MakeConnection(DropFrame, Depth);

		TSharedPtr<UE::MetaHuman::Pipeline::FFlowNode> Flow = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FFlowNode>("Flow");
		Flow->SolverConfigData = SolverConfigData;
		Pipeline.MakeConnection(Color, Flow);

		TSharedPtr<UE::MetaHuman::Pipeline::FFaceTrackerIPhoneManagedNode> Solver = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FFaceTrackerIPhoneManagedNode>("Solver");
		Solver->NumberOfFrames = PipelineFrameRanges[PipelineFrameRangesIndex].EndFrame - PipelineFrameRanges[PipelineFrameRangesIndex].StartFrame; // Could be an overestimate
		Solver->SolverTemplateData = SolverTemplateData;
		Solver->SolverConfigData = SolverConfigData;
		Solver->bSkipPredictiveSolver = SolveType != ESolveType::Preview && bSkipPreview;
		Solver->bSkipDiagnostics = bSkipDiagnostics;

		if (SolveType == ESolveType::Preview)
		{
			Solver->bSkipPerVertexSolve = true; // per-vertex solve makes no sense to apply in the case of the preview solve
		}
		else
		{
			Solver->bSkipPerVertexSolve = bSkipPerVertexSolve;
		}

		TArray<TPair<FString, FString>> StereoReconstructionPairs;
		FootageCaptureData->CameraCalibrations[0]->ConvertToTrackerNodeCameraModels(Solver->Calibrations, StereoReconstructionPairs);
		Solver->Camera = Camera;
		Flow->Calibrations = Solver->Calibrations;
		Flow->Camera = Camera;
		Pipeline.MakeConnection(Flow, Solver);

		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->RigComponent)
			{
				Solver->DNAAsset = USkelMeshDNAUtils::GetMeshDNA(Face->RigComponent->GetSkeletalMeshAsset());
				Solver->BrowJSONData = Face->GetBrowsBuffer();
				Solver->PCARigMemoryBuffer = Face->GetPCARig();

				if ((!Face->HasPredictiveSolvers() && !Solver->bSkipPredictiveSolver) || !Face->HasPredictiveWithoutTeethSolver())
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Predictive solvers are not trained"));

					// Ensure registered listeners (toolkit) is notified and gracefully handles any cancellation by the solver dialog
					TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> FailPipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
					FailPipelineData->SetExitStatus(UE::MetaHuman::Pipeline::EPipelineExitStatus::ProcessError);
					FailPipelineData->SetErrorMessage("Predictive solvers are not trained");
					FailPipelineData->SetErrorNodeName(Solver->Name);
					FailPipelineData->SetErrorNodeCode(UE::MetaHuman::Pipeline::FFaceTrackerIPhoneManagedNode::ErrorCode::UntrainedSolvers);
					CurrentlyProcessedPerformance.Reset();
					OnProcessingFinishedDelegate.Broadcast(FailPipelineData);

					// Mark predictive solver to be skipped.
					Solver->bSkipPredictiveSolver = true;

					return;
				}

				// Predictive solvers are already trained in the Identity parts (if enabled).
				Solver->PredictiveSolvers = Face->GetPredictiveSolvers();
				Solver->PredictiveWithoutTeethSolver = Face->GetPredictiveWithoutTeethSolver();
			}
		}

		if (CVarEnableExportTrackingDataSolverPass1.GetValueOnAnyThread())
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			FString Name = FPaths::GetCleanFilename(GetOuter()->GetName());
			Solver->DebuggingFolder = FPaths::ProjectSavedDir() / Name;
			if (!PlatformFile.DirectoryExists(*Solver->DebuggingFolder))
			{
				bool bCreatedFolder = PlatformFile.CreateDirectory(*Solver->DebuggingFolder);
				if (!bCreatedFolder)
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to create folder to save debugging data during tracking"));
				}
			}

			// save the rig DNA file
			WriteRigDNA(Solver->DNAAsset, *Solver->DebuggingFolder);
		}

		if (!bSkipDiagnostics)
		{
			TSharedPtr<UE::MetaHuman::Pipeline::FDepthMapDiagnosticsNode> DepthMapDiagnostics = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FDepthMapDiagnosticsNode>("DepthMapDiagnostics");
			DepthMapDiagnostics->Calibrations = Solver->Calibrations;
			DepthMapDiagnostics->Camera = Camera;
			DepthMapDiagnosticsResultsPinName = DepthMapDiagnostics->Name + ".DepthMap Diagnostics Out";

			Pipeline.MakeConnection(Tracker, DepthMapDiagnostics);
			Pipeline.MakeConnection(Depth, DepthMapDiagnostics);
			Pipeline.MakeConnection(DepthMapDiagnostics, Solver);
		}
		else
		{
			Pipeline.MakeConnection(Tracker, Solver);
			Pipeline.MakeConnection(Depth, Solver);
		}

		AnimationResultsPinName = Solver->Name + ".Animation Out";
		ScaleDiagnosticsResultsPinName = Solver->Name + ".Scale Diagnostics Out";

		if (SolveType == ESolveType::Preview && bSkipFiltering) // No more stages so do tongue here
		{
			AddTongueSolveToPipeline(Pipeline, TongueSolver, Solver, DropFrame, AnimationResultsPinName);
		}
	}
	else if (PipelineStage == 1)
	{
		TSharedPtr<UE::MetaHuman::Pipeline::FFaceTrackerPostProcessingManagedNode> PostProcessing = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FFaceTrackerPostProcessingManagedNode>("PostProcessing");
		PostProcessing->TemplateData = SolverTemplateData;
		PostProcessing->ConfigData = SolverConfigData;
		PostProcessing->DefinitionsData = SolverDefinitionsData;
		PostProcessing->HierarchicalDefinitionsData = SolverHierarchicalDefinitionsData;
		PostProcessing->bSolveForTweakers = SolveType == ESolveType::AdditionalTweakers;

		if (CVarEnableExportTrackingDataSolverPass2.GetValueOnAnyThread())
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			FString Name = FPaths::GetCleanFilename(GetOuter()->GetName());
			PostProcessing->DebuggingFolder = FPaths::ProjectSavedDir() / Name;
			if (!PlatformFile.DirectoryExists(*PostProcessing->DebuggingFolder))
			{
				bool bCreatedFolder = PlatformFile.CreateDirectory(*PostProcessing->DebuggingFolder);
				if (!bCreatedFolder)
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Failed to create folder to save debugging data during tracking"));
				}
			}
		}

		TArray<TPair<FString, FString>> StereoReconstructionPairs;
		FootageCaptureData->CameraCalibrations[0]->ConvertToTrackerNodeCameraModels(PostProcessing->Calibrations, StereoReconstructionPairs);
		PostProcessing->Camera = Camera;

		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->RigComponent)
			{
				PostProcessing->DNAAsset = USkelMeshDNAUtils::GetMeshDNA(Face->RigComponent->GetSkeletalMeshAsset());
				PostProcessing->PredictiveWithoutTeethSolver = Face->GetPredictiveWithoutTeethSolver();

				for (int32 Frame = PipelineFrameRanges[PipelineFrameRangesIndex].StartFrame; Frame < PipelineFrameRanges[PipelineFrameRangesIndex].EndFrame; ++Frame)
				{
					int32 AnimationFrame = Frame - ProcessingLimitFrameRange.GetLowerBoundValue().Value;
					if (AnimationData[AnimationFrame].ContainsData())
					{
						PostProcessing->TrackingData.Add(ContourTrackingResults[AnimationFrame]);
						PostProcessing->FrameData.Add(AnimationData[AnimationFrame]);
					}
				}
			}
		}

		Pipeline.MakeConnection(DropFrame, PostProcessing);

		if (CVarEnableExportTrackingDataSolverPass2.GetValueOnAnyThread())
		{
			// save the rig DNA file
			WriteRigDNA(PostProcessing->DNAAsset, *PostProcessing->DebuggingFolder);
		}

		AnimationResultsPinName = PostProcessing->Name + ".Animation Out";

		if (bSkipFiltering) // No more stages so do tongue here
		{
			AddTongueSolveToPipeline(Pipeline, TongueSolver, PostProcessing, DropFrame, AnimationResultsPinName);
		}
	}
	else if (PipelineStage == 2)
	{
		TSharedPtr<UE::MetaHuman::Pipeline::FFaceTrackerPostProcessingFilterManagedNode> PostProcessingFilter = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FFaceTrackerPostProcessingFilterManagedNode>("PostProcessingFiltering");
		PostProcessingFilter->TemplateData = SolverTemplateData;
		PostProcessingFilter->ConfigData = SolverConfigData;
		PostProcessingFilter->DefinitionsData = SolverDefinitionsData;
		PostProcessingFilter->HierarchicalDefinitionsData = SolverHierarchicalDefinitionsData;
		PostProcessingFilter->bSolveForTweakers = SolveType == ESolveType::AdditionalTweakers;

		if (CVarEnableExportTrackingDataSolverPass2.GetValueOnAnyThread())
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			FString Name = FPaths::GetCleanFilename(GetOuter()->GetName());
			PostProcessingFilter->DebuggingFolder = FPaths::ProjectSavedDir() / Name;
		}

		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->RigComponent)
			{
				PostProcessingFilter->DNAAsset = USkelMeshDNAUtils::GetMeshDNA(Face->RigComponent->GetSkeletalMeshAsset());

				for (int32 Frame = PipelineFrameRanges[PipelineFrameRangesIndex].StartFrame; Frame < PipelineFrameRanges[PipelineFrameRangesIndex].EndFrame; ++Frame)
				{
					int32 AnimationFrame = Frame - ProcessingLimitFrameRange.GetLowerBoundValue().Value;
					if (AnimationData[AnimationFrame].ContainsData())
					{
						PostProcessingFilter->FrameData.Add(AnimationData[AnimationFrame]);
					}
				}
			}
		}

		Pipeline.MakeConnection(DropFrame, PostProcessingFilter);

		AnimationResultsPinName = PostProcessingFilter->Name + ".Animation Out";

		AddTongueSolveToPipeline(Pipeline, TongueSolver, PostProcessingFilter, DropFrame, AnimationResultsPinName);
	}

	UE::MetaHuman::Pipeline::FFrameComplete OnFrameComplete;
	UE::MetaHuman::Pipeline::FProcessComplete OnProcessComplete;

	OnFrameComplete.AddUObject(this, &UMetaHumanPerformance::FrameComplete);
	OnProcessComplete.AddUObject(this, &UMetaHumanPerformance::ProcessComplete);

	UE::MetaHuman::Pipeline::FPipelineRunParameters PipelineRunParameters;
	PipelineRunParameters.SetStartFrame(PipelineFrameRanges[PipelineFrameRangesIndex].StartFrame);
	PipelineRunParameters.SetEndFrame(PipelineFrameRanges[PipelineFrameRangesIndex].EndFrame);
	PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);
	PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);
	PipelineRunParameters.SetGpuToUse(UE::MetaHuman::Pipeline::FPipeline::PickPhysicalDevice());
	PipelineRunParameters.SetMode(bBlockingProcessing ? UE::MetaHuman::Pipeline::EPipelineMode::PushSyncNodes : UE::MetaHuman::Pipeline::EPipelineMode::PushAsyncNodes);
	//	PipelineRunParameters.SetVerbosity(ELogVerbosity::VeryVerbose); // uncomment for full logging

	Pipeline.Run(PipelineRunParameters);

	if (bBlockingProcessing)
	{
		Pipelines[PipelineIndex]->Reset();

		if (PipelineIndex == 0)
		{
			Pipelines.Reset();
			TongueSolver = nullptr;
		}
	}
}

static FString MetaHumanFrameRangesToString(const TArray<FFrameRange>& InFrameRanges)
{
	FString FrameRangesAsString;

	for (const FFrameRange& FrameRange : InFrameRanges)
	{
		if (!FrameRangesAsString.IsEmpty())
		{
			FrameRangesAsString += TEXT(", ");
		}

		FrameRangesAsString += FString::Printf(TEXT("%i - %i"), FrameRange.StartFrame, FrameRange.EndFrame);
	}

	return FrameRangesAsString;
}

void UMetaHumanPerformance::SendTelemetryForProcessFootageRequest(TSharedPtr<const UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	if (!GEngine->AreEditorAnalyticsEnabled() || !FEngineAnalytics::IsAvailable())
	{
		return;
	}

	/**
	  * @EventName <Editor.MetaHumanPlugin.ProcessFootage>
	  * @Trigger <the user has started processing of the footage in MetaHuman Performance toolkit>
	  * @Type <Client>
	  * @EventParam <IdentityID> <SHA1 hashed GUID of Identity asset, formed as PrimaryAssetType/PrimaryAssetName>
	  * @EventParam <PerformanceID> <SHA1 hashed GUID of Performance asset, formed as PrimaryAssetType/PrimaryAssetName>
	  * @EventParam <DataInputType> <"Depth Footage", "Speech Audio", "Monocular Footage">
	  * @EventParam <DeviceType> <"iPhone 11 or earlier", "iPhone 12", "iPhone 13", "iPhone 14 or later", "Other iOS device", "Stereo HMC">
	  * @EventParam <LengthInFrames> <int32>
	  * @EventParam <Framerate> <double>
	  * @EventParam <RangeStart> <int32>
	  * @EventParam <RangeEnd> <int32>
	  * @EventParam <SolveType> <string>
	  * @EventParam <SkipPreview> <bool>
	  * @EventParam <SkipFiltering> <bool>
	  * @EventParam <SkipTongueSolve> <bool>
	  * @EventParam <SkipPerVertexSolve> <bool>
	  * @EventParam <SkipDiagnostics> <bool>
	  * @EventParam <MinimumDepthMapFaceCoverage> <float>
	  * @EventParam <MinimumDepthMapFaceWidth> <float>
	  * @EventParam <MaximumStereoBaselineDifferenceFromIdentity> <float>
	  * @EventParam <MaximumScaleDifferenceFromIdentity> <float>
	  * @EventParam <HeadStabilization> <bool>
	  * @EventParam <FocalLength> <float>
	  * @EventParam <AudioSampleRate> <int32>
	  * @EventParam <AudioNumChannels> <int32>
	  * @EventParam <AudioDuration> <float>
	  * @EventParam <RealtimeAudioSolve> <bool>
	  * @EventParam <FrameRanges> <string>
	  * @EventParam <ExcludedFrames> <string>
	  * @EventParam <TimeTaken> <double>
	  * @EventParam <ExitStatus> <string>
	  * @EventParam <ErrorNodeName> <string>
	  * @EventParam <ErrorNodeCode> <int32>
	  * @EventParam <DiagnosticsWarningMessage> <string>
	  * @EventParam <ScriptedProcessing> <bool>
	  * @Comments <->
	  * @Owner <jon.cook>
	  *
	  * Do not include ErrorMessage since this could potentially contain EGPI data such as a local file path.
	  */

	TArray< FAnalyticsEventAttribute > EventAttributes;

	if (Identity)
	{
		//IdentityID
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("IdentityID"), Identity->GetHashedIdentityAssetID()));
	}	

	//PerformanceID
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PerformanceID"), GetHashedPerformanceAssetID()));

	// Data input type
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DataInputType"), UEnum::GetDisplayValueAsText(InputType).ToString()));

	//Device type	
	if (InputType != EDataInputType::Audio)
	{
		FString DeviceType = "unspecified";
		DeviceType = UEnum::GetDisplayValueAsText(this->FootageCaptureData->Metadata.DeviceClass).ToString();
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceType"), DeviceType));
	}

	// Length in frames
	int32 LengthInFrames = 0;
	if (InputType == EDataInputType::Audio)
	{
		TObjectPtr<USoundWave> AudioForProcessing = GetAudioForProcessing();
		LengthInFrames = MediaFrameRanges[AudioForProcessing].GetUpperBoundValue().Value - MediaFrameRanges[AudioForProcessing].GetLowerBoundValue().Value;
	}
	else if (TObjectPtr<UImgMediaSource> ImageSequence = FootageCaptureData->ImageSequences[0])
	{
		LengthInFrames = MediaFrameRanges[ImageSequence].GetUpperBoundValue().Value - MediaFrameRanges[ImageSequence].GetLowerBoundValue().Value;
	}

	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("LengthInFrames"), LengthInFrames));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Framerate"), GetFrameRate().AsDecimal()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("RangeStart"), StartFrameToProcess));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("RangeEnd"), EndFrameToProcess));

	if (InputType == EDataInputType::DepthFootage)
	{
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SolveType"), UEnum::GetDisplayValueAsText(SolveType).ToString()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SkipPreview"), bSkipPreview));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SkipFiltering"), bSkipFiltering));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SkipTongueSolve"), bSkipTongueSolve));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SkipPerVertexSolve"), bSkipPerVertexSolve));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SkipDiagnostics"), bSkipDiagnostics));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MinimumDepthMapFaceCoverage"), MinimumDepthMapFaceCoverage));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MinimumDepthMapFaceWidth"), MinimumDepthMapFaceWidth));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MaximumStereoBaselineDifferenceFromIdentity"), MaximumStereoBaselineDifferenceFromIdentity));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MaximumScaleDifferenceFromIdentity"), MaximumScaleDifferenceFromIdentity));
	}

	if (InputType == EDataInputType::MonoFootage)
	{
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HeadStabilization"), bHeadStabilization));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FocalLength"), FocalLength));
	}

	if (TObjectPtr<class USoundWave> AudioForProcessing = GetAudioForProcessing())
	{
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AudioSampleRate"), AudioForProcessing->GetSampleRateForCurrentPlatform()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AudioNumChannels"), AudioForProcessing->NumChannels));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AudioDuration"), AudioForProcessing->GetDuration()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("RealtimeAudioSolve"), bRealtimeAudio));
	}

	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FrameRanges"), MetaHumanFrameRangesToString(PipelineFrameRanges)));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ExcludedFrames"), MetaHumanFrameRangesToString(PipelineExcludedFrames)));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("TimeTaken"), FPlatformTime::Seconds() - ProcessingStartTime));

	FString ExitStatus;
	FText DiagnosticsWarningMessage;
	switch (InPipelineData->GetExitStatus()) // Unfortunately EPipelineExitStatus is not an UENUM so cant use UEnum::GetDisplayValueAsText
	{
	case UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok:
		ExitStatus = TEXT("Ok");
		DiagnosticsIndicatesProcessingIssue(DiagnosticsWarningMessage);
		break;

	case UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted:
		ExitStatus = TEXT("Aborted");
		break;

	default: // Common exit status dealt with above, anything else use the enum's int value
		ExitStatus = FString::Printf(TEXT("Code %i"), (int32)InPipelineData->GetExitStatus());
	}
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ExitStatus"), ExitStatus));

	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ErrorNodeName"), InPipelineData->GetErrorNodeName()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ErrorNodeCode"), InPipelineData->GetErrorNodeCode()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DiagnosticsWarningMessage"), DiagnosticsWarningMessage.ToString()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ScriptedProcessing"), bIsScriptedProcessing));

	FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.MetaHumanPlugin.ProcessFootage"), EventAttributes);
}

FString UMetaHumanPerformance::GetHashedPerformanceAssetID()
{
	FPrimaryAssetId PerformanceAssetID = this->GetPrimaryAssetId();
	FString PerformanceAssetIDStr = PerformanceAssetID.PrimaryAssetType.GetName().ToString() / PerformanceAssetID.PrimaryAssetName.ToString();
	FSHA1 PerformanceIDSha1;
	PerformanceIDSha1.UpdateWithString(*PerformanceAssetIDStr, PerformanceAssetIDStr.Len());
	FSHAHash PerformanceIDHash = PerformanceIDSha1.Finalize();	
	return PerformanceIDHash.ToString();
}

TObjectPtr<class USoundWave> UMetaHumanPerformance::GetAudioForProcessing() const
{
	if (InputType == EDataInputType::Audio)
	{
		return Audio;
	}
	else if (FootageCaptureData && !FootageCaptureData->AudioTracks.IsEmpty())
	{
		return FootageCaptureData->AudioTracks[0];
	}

	return nullptr;
}

FTimecode UMetaHumanPerformance::GetAudioMediaTimecode() const
{
	FTimecode AudioTimecode;

	if (InputType == EDataInputType::Audio)
	{
		FSoundWaveTimecodeInfo TimecodeInfo = Audio->TimecodeInfo;
		const double NumSecondsSinceMidnight = TimecodeInfo.GetNumSecondsSinceMidnight();
		AudioTimecode = FTimecode(NumSecondsSinceMidnight, TimecodeInfo.TimecodeRate, TimecodeInfo.bTimecodeIsDropFrame, /* InbRollover = */ true);
	}
	else if (FootageCaptureData)
	{
		if (FootageCaptureData->AudioTracks.Num() > 0)
		{
			TOptional<FTimecode> TimecodeOpt = USoundWaveTimecodeUtils::GetTimecode(FootageCaptureData->AudioTracks[0].Get());
			if (TimecodeOpt.IsSet())
			{
				AudioTimecode = TimecodeOpt.GetValue();
			}
		}
	}

	return AudioTimecode;
}

FFrameRate UMetaHumanPerformance::GetAudioMediaTimecodeRate() const
{
	FFrameRate MediaFrameRate = GetFrameRate();

	if (InputType == EDataInputType::Audio && Audio)
	{
		if(Audio->GetTimecodeInfo())
		{
			MediaFrameRate = Audio->GetTimecodeInfo()->TimecodeRate;
		}
	}
	else if (FootageCaptureData)
	{
		if (FootageCaptureData->AudioTracks.Num() > 0)
		{
			TOptional<FFrameRate> FrameRateOpt = USoundWaveTimecodeUtils::GetFrameRate(FootageCaptureData->AudioTracks[0].Get());
			if (FrameRateOpt.IsSet())
			{
				MediaFrameRate = FrameRateOpt.GetValue();
			}
		}
	}
	return MediaFrameRate;
}

void UMetaHumanPerformance::AddSpeechToAnimSolveToPipeline(UE::MetaHuman::Pipeline::FPipeline& InPipeline, TSharedPtr<UE::MetaHuman::Pipeline::FSpeechToAnimNode> InSpeechAnimNode, FString& OutAnimationResultsPinName)
{
	TObjectPtr<class USoundWave> AudioForProcessing = GetAudioForProcessing();
	if (AudioForProcessing)
	{
		InPipeline.AddNode(InSpeechAnimNode);

		InSpeechAnimNode->Audio = AudioForProcessing;
		InSpeechAnimNode->bDownmixChannels = bDownmixChannels;
		InSpeechAnimNode->AudioChannelIndex = AudioChannelIndex;
		InSpeechAnimNode->OffsetSec = CalculateAudioProcessingOffset();
		InSpeechAnimNode->FrameRate = GetFrameRate().AsDecimal();
		InSpeechAnimNode->ProcessingStartFrameOffset = StartFrameToProcess;
		InSpeechAnimNode->bGenerateBlinks = bGenerateBlinks && InputType == EDataInputType::Audio;
	
		AnimationResultsPinName = InSpeechAnimNode->Name + ".Animation Out";
	}
}

void UMetaHumanPerformance::AddTongueSolveToPipeline(UE::MetaHuman::Pipeline::FPipeline& InPipeline, TSharedPtr<UE::MetaHuman::Pipeline::FSpeechToAnimNode> InTongueSolveNode, TSharedPtr<UE::MetaHuman::Pipeline::FNode> InInputNode, TSharedPtr<UE::MetaHuman::Pipeline::FDropFrameNode> InDropFrameNode, FString& OutAnimationResultsPinName)
{
	if (!bSkipTongueSolve && GetAudioForProcessing() != nullptr)
	{
		AddSpeechToAnimSolveToPipeline(InPipeline, InTongueSolveNode, OutAnimationResultsPinName);

		InPipeline.MakeConnection(InDropFrameNode, InTongueSolveNode);

		TSharedPtr<UE::MetaHuman::Pipeline::FAnimationMergeNode> AnimationMerge = InPipeline.MakeNode<UE::MetaHuman::Pipeline::FAnimationMergeNode>("AnimationMerge");

		InPipeline.MakeConnection(InInputNode, AnimationMerge, 0, 0);
		InPipeline.MakeConnection(InTongueSolveNode, AnimationMerge, 0, 1);

		OutAnimationResultsPinName = AnimationMerge->Name + ".Animation Out";
	}
}

void UMetaHumanPerformance::CancelPipeline()
{
	if (IsProcessing())
	{
		if (SpeechToAnimSolver)
		{
			SpeechToAnimSolver->CancelModelSolve();
		}

		if (TongueSolver)
		{
			TongueSolver->CancelModelSolve();
		}

		for (TSharedPtr<UE::MetaHuman::Pipeline::FPipeline> Pipeline : Pipelines)
		{
			Pipeline->Cancel();
		}

		if (DefaultTracker && DefaultTracker->IsLoadingTrackers())
		{
			if (!bBlockingProcessing)
			{
				DefaultTracker->CancelLoadTrackers();
			}
			CurrentlyProcessedPerformance.Reset();

			// Notify editor toolkit that the tracker loading was canceled (pipeline wasn't started at this point).
			TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> FailPipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
			FailPipelineData->SetExitStatus(UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted);
			FailPipelineData->SetErrorMessage("Tracker loading canceled");

			OnProcessingFinishedDelegate.Broadcast(FailPipelineData);
		}
	}
}

bool UMetaHumanPerformance::IsProcessing() const
{
	return CurrentlyProcessedPerformance.IsValid() && CurrentlyProcessedPerformance.Get() == this;
}

bool UMetaHumanPerformance::CanProcess() const
{
	//NOTE: if you are changing this method, please also change GetCannotProcessTooltipText method below it, as it should follow the same structure of conditions

	if (IsProcessing())
	{
		return false;
	}

	if (CurrentlyProcessedPerformance.IsValid() && CurrentlyProcessedPerformance.Get() != this)
	{
		return false;
	}

	if (InputType == EDataInputType::Audio)
	{
		if (GetAudioForProcessing() == nullptr)
		{
			return false;
		}
	}
	else
	{
		if (!FootageCaptureData)
		{
			return false;
		}	

		if (InputType == EDataInputType::MonoFootage)
		{
			if (!FootageCaptureData->IsInitialized(UCaptureData::EInitializedCheck::ImageSequencesOnly))
			{
				return false;
			}
		}
		// Trying to process Depth Footage
		else
		{
			if (!IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
			{
				return false;
			}

			if (!FootageCaptureData->IsInitialized(UCaptureData::EInitializedCheck::Full))
			{
				return false;
			}

			if (!Identity)
			{
				return false;
			}

			if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
			{
				if (!Face->RigComponent)
				{
					return false;
				}

				if (Face->IsAsyncPredictiveSolverTrainingActive())
				{
					return false;
				}

				if (!Face->bIsAutoRigged)
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			if (!DefaultTracker || DefaultTracker->IsLoadingTrackers() || !DefaultTracker->CanProcess())
			{
				return false;
			}

			if (!DefaultSolver || !DefaultSolver->CanProcess())
			{
				return false;
			}
		}

		if (!FMetaHumanSupportedRHI::IsSupported())
		{
			return false;
		}

		if (!bMetaHumanAuthoringObjectsPresent)
		{
			return false;
		}
	}

	if (ProcessingLimitFrameRange.GetUpperBoundValue() <= ProcessingLimitFrameRange.GetLowerBoundValue())
	{
		return false;
	}

	return true;
}

FText UMetaHumanPerformance::GetCannotProcessTooltipText() const
{
	FText ToEnableThisOption = LOCTEXT("CannotProcessThisOptionIsDisabled", "To enable this option");
	if (IsProcessing())
	{
		return FText::Format(LOCTEXT("CannotProcessAlreadyProcessingTooltipText", "{0} stop the processing of the current Performance."), ToEnableThisOption);
	}

	if (CurrentlyProcessedPerformance.IsValid() && CurrentlyProcessedPerformance.Get() != this)
	{
		return LOCTEXT("CannotProcessProcessingAnotherPerformanceTooltipText", "Another Performance is currently being processed. To enable this option, first stop the processing of that one.");
	}

	if (InputType == EDataInputType::Audio)
	{
		if (GetAudioForProcessing() == nullptr)
		{
			return FText::Format(LOCTEXT("CannotProcessAudioNotSetTooltipText", "{0} set the Audio property of this Performance in the Details panel."), ToEnableThisOption);
		}
	}
	else
	{
		if (!FootageCaptureData)
		{
			return FText::Format(LOCTEXT("CannotProcessFootageDataNotInitializedTooltipText", "{0} set the Footage Capture Data property of this Performance in the Details panel."), ToEnableThisOption);
		}

		if (InputType == EDataInputType::MonoFootage)
		{
			UFootageCaptureData::FVerifyResult VerifyResult = FootageCaptureData->VerifyData(UCaptureData::EInitializedCheck::ImageSequencesOnly);
			if (VerifyResult.HasError())
			{
				return FText::Format(LOCTEXT("CannotProcessMonoFootageDataNotVerifiedTooltipText", "{0} set a valid Footage Capture Data property of this Performance in the Details panel. {1}."), ToEnableThisOption, FText::FromString(*VerifyResult.StealError()));
			}
		}
		else
		{
			if (!IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
			{
				return FText::Format(LOCTEXT("CannotProcessFootagePluginDisabledTooltipText", "{0} please make sure the Depth Processing plugin is enabled. (Available on Fab)"), ToEnableThisOption);
			}

			UFootageCaptureData::FVerifyResult VerifyResult = FootageCaptureData->VerifyData(UCaptureData::EInitializedCheck::Full);
			if (VerifyResult.HasError())
			{
				return FText::Format(LOCTEXT("CannotProcessFootageDataNotVerifiedTooltipText", "{0} set a valid Footage Capture Data property of this Performance in the Details panel. {1}."), ToEnableThisOption, FText::FromString(*VerifyResult.StealError()));
			}

			if (!Identity)
			{
				return FText::Format(LOCTEXT("CannotProcessIdentityNotSetTooltipText", "{0} set the MetaHuman Identity property of this Performance in the Details panel."), ToEnableThisOption);
			}

			if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
			{
				if (!Face->RigComponent)
				{
					//this one is a sanity check and should never happen
					return LOCTEXT("CannotProcessNoRigComponentTooltipText", "This option is disabled because the Rig Component of the Face is missing.");
				}

				if (Face->IsAsyncPredictiveSolverTrainingActive())
				{
					return FText::Format(LOCTEXT("CannotProcessAsyncPredictiveSolverActiveTooltipText", "{0} please wait for the Prepare for Performance\nstep in MetaHuman Identity to complete."), ToEnableThisOption);
				}

				if (!Face->bIsAutoRigged)
				{
					return FText::Format(LOCTEXT("CannotProcessAsyncUseM2MHOnceTooltipText", "{0} first use Mesh to MetaHuman option in MetaHuman Identity at least once."), ToEnableThisOption);
				}
			}
			else
			{
				return FText::Format(LOCTEXT("CannotProcessFaceMissingTooltipText", "{0} first add Face Part to MetaHuman Identity."), ToEnableThisOption);
			}


			if (!DefaultTracker)
			{
				return FText::Format(LOCTEXT("CannotProcessDefaultTrackerMissingTooltipText", "{0} please ensure that the Default Tracker property\nin the Details panel is set, and correctly configured."), ToEnableThisOption);
			}
			else if (DefaultTracker->IsLoadingTrackers())
			{
				return FText::Format(LOCTEXT("CannotProcessLoadingTrackersTooltipText", "{0} please wait for the trackers to finish loading."), ToEnableThisOption);
			}
			else if (!DefaultTracker->CanProcess())
			{
				return FText::Format(LOCTEXT("CannotProcessMisconfiguredTrackerTooltipText", "{0} please ensure all models specified in the Tracker asset are correct."), ToEnableThisOption);
			}


			if (!DefaultSolver)
			{
				return FText::Format(LOCTEXT("CannotProcessMissingDefaultSolverTooltipText", "{0} please ensure that the Default Solver property\nin the Details panel is set, and correctly configured."), ToEnableThisOption);
			}
			else if (!DefaultSolver->CanProcess())
			{
				return FText::Format(LOCTEXT("CannotProcessDefaultSolverCantProcessTooltipText", "{0} please ensure that the Default Solver is correctly configured."), ToEnableThisOption);
			}
		}

		if (!FMetaHumanSupportedRHI::IsSupported())
		{
			return FText::Format(LOCTEXT("CannotProcessInvalidRHI", "{0} please ensure that the RHI is set to {1}."), ToEnableThisOption, FMetaHumanSupportedRHI::GetSupportedRHINames());
		}

		if (!bMetaHumanAuthoringObjectsPresent)
		{
			return LOCTEXT("MissingAuthoringObjects", "MetaHuman authoring objects are not present");
		}
	}

	if (ProcessingLimitFrameRange.GetUpperBoundValue() <= ProcessingLimitFrameRange.GetLowerBoundValue())
	{
		return FText::Format(LOCTEXT("CannotProcessInvalidRangeTooltipText", "{0} please ensure that the processing range is valid."), ToEnableThisOption);
	}

	return FText();
}

void UMetaHumanPerformance::SetBlockingProcessing(bool bInBlockingProcessing)
{
	bBlockingProcessing = bInBlockingProcessing;
}

inline FVector CalcCameraLocation(const FMatrix& InCameraTransform)
{
	// the camera location is -R'T from the camera transform
	FVector T = InCameraTransform.GetOrigin();
	FRotator R = InCameraTransform.Rotator();
	FMatrix RMat = UE::Math::TRotationMatrix<double>::Make(R);
	return -RMat.GetTransposed().TransformFVector4(FVector4(T, 1.f));
}

bool UMetaHumanPerformance::DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const
{
	if (bSkipDiagnostics || InputType != EDataInputType::DepthFootage)
	{
		return false;
	}

	int32 NumBadDepthMapFrames = 0;

	// depthmap face coverage
	const int32 ProcessingLimitStartFrame = ProcessingLimitFrameRange.GetLowerBoundValue().Value;
	for (uint32 FrameNumber = StartFrameToProcess - ProcessingLimitStartFrame; FrameNumber < EndFrameToProcess - ProcessingLimitStartFrame; ++FrameNumber)
	{
		if (AnimationData[FrameNumber].ContainsData() && (DepthMapDiagnosticResults[FrameNumber].NumFacePixels == 0 || static_cast<float>(DepthMapDiagnosticResults[FrameNumber].NumFaceValidDepthMapPixels) /
			DepthMapDiagnosticResults[FrameNumber].NumFacePixels*100 < MinimumDepthMapFaceCoverage))
		{
			NumBadDepthMapFrames++;
		}
	}

	bool bDiagnosticsIndicatesIssue = false;
	if (NumBadDepthMapFrames > 0)
	{ 
		OutDiagnosticsWarningMessage = FText::Format(LOCTEXT("ProcessingDiagnosticsWarning1", "{0} frames contained less than {1}% valid depth-map pixels in the region of the face.\nPlease check the depth-maps for the shot and ensure that there is adequate coverage in the region of the face; you may need to re-ingest your capture data with better Min Distance and/or Max Distance properties set in the CaptureSource asset in order to fix this."),
			NumBadDepthMapFrames, MinimumDepthMapFaceCoverage);
		bDiagnosticsIndicatesIssue = true;
	}

	// depthmap face width
	int32 NumBadFaceWidthFrames = 0;
	for (uint32 FrameNumber = StartFrameToProcess - ProcessingLimitStartFrame; FrameNumber < EndFrameToProcess - ProcessingLimitStartFrame; ++FrameNumber)
	{
		if (AnimationData[FrameNumber].ContainsData() && DepthMapDiagnosticResults[FrameNumber].FaceWidthInPixels < MinimumDepthMapFaceWidth)
		{
			NumBadFaceWidthFrames++;
		}
	}

	if (NumBadFaceWidthFrames > 0)
	{
		bDiagnosticsIndicatesIssue = true;
		FText FaceWidthDiagnosticsWarningMessage = FText::Format(LOCTEXT("FaceWidthDiagnosticsWarningMessage", "{0} frames contained a face of width less than {1} pixels in the depth-map.\nPlease ensure that the face covers a larger area of the image in order to obtain good animation results."),
			NumBadFaceWidthFrames, MinimumDepthMapFaceWidth);

		if (OutDiagnosticsWarningMessage.ToString().Len() > 0)
		{
			OutDiagnosticsWarningMessage = FText::FromString(OutDiagnosticsWarningMessage.ToString() + TEXT("\n\n") + FaceWidthDiagnosticsWarningMessage.ToString());
		}
		else
		{
			OutDiagnosticsWarningMessage = FaceWidthDiagnosticsWarningMessage;
		}
	}

	float ScaleDiff = 100.0f * FMath::Abs(1.0f - ScaleEstimate);
	if (ScaleDiff > MaximumScaleDifferenceFromIdentity)
	{
		bDiagnosticsIndicatesIssue = true;
		FText ScaleDiagnosticsWarningMessage = FText::Format(LOCTEXT("ScaleDiagnosticsWarningMessage", "Difference between estimated Performance head scale and Identity head-scale is {0}%, which is more than the {1}% threshold.\nThis may indicate an issue with the camera calibration for the CaptureData for the Identity or Performance."),
			ScaleDiff, MaximumScaleDifferenceFromIdentity);

		if (OutDiagnosticsWarningMessage.ToString().Len() > 0)
		{
			OutDiagnosticsWarningMessage = FText::FromString(OutDiagnosticsWarningMessage.ToString() + TEXT("\n\n") + ScaleDiagnosticsWarningMessage.ToString());
		}
		else
		{
			OutDiagnosticsWarningMessage = ScaleDiagnosticsWarningMessage;
		}

	}

	// camera calibration difference from Identity
	if (Identity != nullptr)
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			for (int32 Pose = 0; Pose < Face->GetPoses().Num(); ++Pose)
			{
				UMetaHumanIdentityPose* CurPose = Face->GetPoses()[Pose];
				if (CurPose)
				{
					if (UFootageCaptureData* IdentityFootageCaptureData = Cast<UFootageCaptureData>(CurPose->GetCaptureData()))
					{
						if (!IdentityFootageCaptureData->CameraCalibrations.IsEmpty() && FootageCaptureData != nullptr && !FootageCaptureData->CameraCalibrations.IsEmpty())
						{
							auto& IdentityFootageCameraCalibration = IdentityFootageCaptureData->CameraCalibrations[0];
							auto& FootageCameraCalibration = FootageCaptureData->CameraCalibrations[0];
							// compare the two camera calibrations
							if (IdentityFootageCameraCalibration->StereoPairs.Num() == FootageCameraCalibration->StereoPairs.Num())
							{
								TArray<TPair<FString, FString>> StereoReconstructionPairs;
								TArray<FCameraCalibration> PerformanceCameraCalibrations, IdentityCameraCalibrations;
								FootageCameraCalibration->ConvertToTrackerNodeCameraModels(PerformanceCameraCalibrations, StereoReconstructionPairs);
								IdentityFootageCameraCalibration->ConvertToTrackerNodeCameraModels(IdentityCameraCalibrations, StereoReconstructionPairs);

								for (int32 Pair = 0; Pair < IdentityFootageCameraCalibration->StereoPairs.Num(); ++Pair)
								{
									// we can only do this if we have the full stereo calibration, not just a single RGB view and depth view
									if (!FootageCameraCalibration->CameraCalibrations[FootageCameraCalibration->StereoPairs[Pair].CameraIndex1].IsDepthCamera &&
										!FootageCameraCalibration->CameraCalibrations[FootageCameraCalibration->StereoPairs[Pair].CameraIndex2].IsDepthCamera &&
										!IdentityFootageCameraCalibration->CameraCalibrations[IdentityFootageCameraCalibration->StereoPairs[Pair].CameraIndex1].IsDepthCamera &&
										!IdentityFootageCameraCalibration->CameraCalibrations[IdentityFootageCameraCalibration->StereoPairs[Pair].CameraIndex2].IsDepthCamera)
									{
										FVector IdentityTranslation1 = CalcCameraLocation(IdentityCameraCalibrations[IdentityFootageCameraCalibration->StereoPairs[Pair].CameraIndex1].Transform);
										FVector IdentityTranslation2 = CalcCameraLocation(IdentityCameraCalibrations[IdentityFootageCameraCalibration->StereoPairs[Pair].CameraIndex2].Transform);
										FVector PerformanceTranslation1 = CalcCameraLocation(PerformanceCameraCalibrations[FootageCameraCalibration->StereoPairs[Pair].CameraIndex1].Transform);
										FVector PerformanceTranslation2 = CalcCameraLocation(PerformanceCameraCalibrations[FootageCameraCalibration->StereoPairs[Pair].CameraIndex2].Transform);

										double IdentityBaseline = (IdentityTranslation2 - IdentityTranslation1).Length();
										double PerformanceBaseline = (PerformanceTranslation2 - PerformanceTranslation1).Length();

										float PercentBaselineDiff = 100.0f * FMath::Abs(IdentityBaseline - PerformanceBaseline) / PerformanceBaseline;
										if (PercentBaselineDiff > MaximumStereoBaselineDifferenceFromIdentity)
										{
											bDiagnosticsIndicatesIssue = true;
											FText CalibrationDiagnosticsWarningMessage = FText::Format(LOCTEXT("CalibrationDiagnosticsWarningMessage3",
												"Difference between Identity and Performance CaptureData stereo baselines is {0}%, which is more than the {1}% threshold.\nThis may indicate an issue with the camera calibration for the CaptureData for the Identity or Performance."),
												PercentBaselineDiff, MaximumStereoBaselineDifferenceFromIdentity);

											if (OutDiagnosticsWarningMessage.ToString().Len() > 0)
											{
												OutDiagnosticsWarningMessage = FText::FromString(OutDiagnosticsWarningMessage.ToString() + TEXT("\n\n") + CalibrationDiagnosticsWarningMessage.ToString());
											}
											else
											{
												OutDiagnosticsWarningMessage = CalibrationDiagnosticsWarningMessage;
											}

											break;
										}
									}
								}
							}
						}		
					}
				}
			}
		}
	}

	return bDiagnosticsIndicatesIssue;
}


void UMetaHumanPerformance::FrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	MHA_CPUPROFILER_EVENT_SCOPE(UMetaHumanPerformance::FrameComplete);

	const int32 FrameNumber = InPipelineData->GetFrameNumber();

	UE_LOG(LogMetaHumanPerformance, Verbose, TEXT("Processed Frame %d (Frame range %i, Stage %i)"), FrameNumber, PipelineFrameRangesIndex + 1, PipelineStage + 1);

	const int32 AnimationFrameNumber = FrameNumber - ProcessingLimitFrameRange.GetLowerBoundValue().Value;
	check(AnimationFrameNumber >= 0 && AnimationFrameNumber < ContourTrackingResults.Num());
	check(ContourTrackingResults.Num() == AnimationData.Num());

	if (PipelineStage == 0)
	{
		if (!TrackingResultsPinName.IsEmpty() && InPipelineData->HasData<FFrameTrackingContourData>(TrackingResultsPinName))
		{
			FFrameTrackingContourData FrameTrackingContourData(InPipelineData->MoveData<FFrameTrackingContourData>(TrackingResultsPinName));
			ContourTrackingResults[AnimationFrameNumber].Camera = Camera;
			ContourTrackingResults[AnimationFrameNumber].TrackingContours = MoveTemp(FrameTrackingContourData.TrackingContours);
			if (!bSkipDiagnostics)
			{
				TMap<FString, FDepthMapDiagnosticsResult> CurDepthMapDiagnosticsResult(InPipelineData->MoveData<TMap<FString, FDepthMapDiagnosticsResult>>(DepthMapDiagnosticsResultsPinName));
				check(CurDepthMapDiagnosticsResult.Num() == 1); // currently only supporting a single depthmap so should only be one result per frame
				DepthMapDiagnosticResults[AnimationFrameNumber] = CurDepthMapDiagnosticsResult.begin()->Value;
			}
		}
	}

	if (PipelineStage == 0 || PipelineStage == 1 || PipelineStage == 2)
	{
		FFrameAnimationData& AnimationFrame = AnimationData[AnimationFrameNumber];
		AnimationFrame = InPipelineData->MoveData<FFrameAnimationData>(AnimationResultsPinName);

		if (InputType == EDataInputType::MonoFootage)
		{
			// Orientation of pose is in correct coord system
			// Translation is not however.
			
			FTransform Pose = AnimationFrame.Pose;

			FVector Trans;
			Trans.X = -Pose.GetTranslation().Y;
			Trans.Y = -Pose.GetTranslation().Z;
			Trans.Z = Pose.GetTranslation().X;

			Pose.SetTranslation(Trans);

			AnimationFrame.Pose = FMetaHumanHeadTransform::HeadToRoot(Pose);
		}
		else if (InputType == EDataInputType::Audio)
		{
			FTransform TransformedPose = AudioDrivenHeadPoseTransform(AnimationFrame.Pose);
			AnimationFrame.Pose = TransformedPose;
		}

		// if first valid frame, get the estimated scale from the estimated scale pin
		if (!bSkipDiagnostics && 
			ScaleEstimate < 0 && 
			PipelineStage == 0 && 
			InputType == EDataInputType::DepthFootage &&
			AnimationFrame.ContainsData())
		{
			ScaleEstimate = InPipelineData->MoveData<float>(ScaleDiagnosticsResultsPinName);
		}
	}

	if (CVarEnableDebugAnimation.GetValueOnAnyThread())
	{
		FFrameAnimationData& AnimationFrame = AnimationData[AnimationFrameNumber];
		const float DebugCurveValue = static_cast<float>(AnimationFrameNumber) / static_cast<float>(EndFrameToProcess);;
		for (TPair<FString, float>& Controls : AnimationFrame.AnimationData)
		{
			Controls.Value = DebugCurveValue;
		}

		const FVector DebugCurveVector(DebugCurveValue);
		AnimationFrame.Pose.SetLocation(DebugCurveVector);
		AnimationFrame.Pose.SetRotation(FRotator::MakeFromEuler(DebugCurveVector).Quaternion());
	}

	MarkPackageDirty();

	OnFrameProcessedDelegate.Broadcast(FrameNumber);
}

void UMetaHumanPerformance::ProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	const double PipelineStageElapsedTime = FPlatformTime::Seconds() - PipelineStageStartTime;
	UE_LOG(LogMetaHumanPerformance, Display, TEXT("Finished Pipeline (Frame range %i, Stage %i) in %f seconds"), PipelineFrameRangesIndex + 1, PipelineStage + 1, PipelineStageElapsedTime);
	PipelineStageStartTime = 0.0;

	int32 CompletedPipeline = Pipelines.Num() - 1;

	if (PipelineStage == 0 && SolveType == ESolveType::Preview)
	{
		PipelineStage++; // Skip 2nd stage - post processing
	}
	else if (PipelineStage == 0 && (InputType == EDataInputType::Audio || InputType == EDataInputType::MonoFootage))
	{
		PipelineStage += 2; // Single stage only. Skip 2nd and 3rd stages.
	}

	if (PipelineStage == 1 && bSkipFiltering)
	{
		PipelineStage++; // Skip 3rd stage - filtering
	}

	PipelineStage++;

	if (PipelineStage == 1)
	{
		OnStage1ProcessingFinishedDelegate.Broadcast();
	}

	if (PipelineStage == 3)
	{
		PipelineFrameRangesIndex++;
		PipelineStage = 0;
	}

	if (PipelineFrameRangesIndex < PipelineFrameRanges.Num() && InPipelineData->GetExitStatus() == UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok)
	{
		StartPipelineStage();

		if (!bBlockingProcessing)
		{
			Pipelines[CompletedPipeline]->Reset();
		}
	}
	else
	{
		SolverConfigData.Reset();
		SolverTemplateData.Reset();
		SolverDefinitionsData.Reset();
		SolverHierarchicalDefinitionsData.Reset();
		TongueSolver.Reset();

		CurrentlyProcessedPerformance.Reset();
		OnProcessingFinishedDelegate.Broadcast(InPipelineData);

		if (!bBlockingProcessing)
		{
			Pipelines[CompletedPipeline]->Reset();
			Pipelines.Reset();
			TongueSolver = nullptr;
		}

		OnProcessingFinishedDynamic.Broadcast();

		PipelineFrameRangesIndex = 0;
		PipelineFrameRanges.Reset();
		PipelineStage = 0;
	}

	MarkPackageDirty();
}

void UMetaHumanPerformance::ResetOutput(bool bInWholeSequence)
{
	const int32 ProcessingLimitStartFrame = ProcessingLimitFrameRange.GetLowerBoundValue().Value;
	const int32 NumFrames = ProcessingLimitFrameRange.GetUpperBoundValue().Value - ProcessingLimitStartFrame;

	if (!bInWholeSequence && (AnimationData.Num() != NumFrames || DepthMapDiagnosticResults.Num() != NumFrames))
	{
		bInWholeSequence = true;
		UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Frame Range mismatch! Resetting animation data"));
	}

	if (bInWholeSequence)
	{
		ContourTrackingResults.Reset(NumFrames);
		ContourTrackingResults.AddDefaulted(NumFrames);

		AnimationData.Reset(NumFrames);
		AnimationData.AddDefaulted(NumFrames);

		DepthMapDiagnosticResults.Reset(NumFrames);
		DepthMapDiagnosticResults.AddDefaulted(NumFrames);

		ProcessingExcludedFrames.Reset();
	}
	else
	{
		for (uint32 AnimationFrameNumber = StartFrameToProcess - ProcessingLimitStartFrame; AnimationFrameNumber < EndFrameToProcess - ProcessingLimitStartFrame; ++AnimationFrameNumber)
		{
			ContourTrackingResults[AnimationFrameNumber] = FFrameTrackingContourData();
			AnimationData[AnimationFrameNumber] = FFrameAnimationData();
			DepthMapDiagnosticResults[AnimationFrameNumber] = FDepthMapDiagnosticsResult();
		}

		const int32 StartFrameToProcessInt32 = (int32) StartFrameToProcess;
		const int32 EndFrameToProcessInt32 = (int32) EndFrameToProcess;

		for (int32 Index = 0; Index < ProcessingExcludedFrames.Num(); ++Index)
		{
			FFrameRange& FrameRange = ProcessingExcludedFrames[Index];

			if (FrameRange.StartFrame >= StartFrameToProcessInt32 && FrameRange.EndFrame < EndFrameToProcessInt32)
			{
				ProcessingExcludedFrames.RemoveAt(Index);
				Index--;
			}
			else if (FrameRange.StartFrame < StartFrameToProcessInt32 && FrameRange.EndFrame > EndFrameToProcessInt32)
			{
				FFrameRange SplitFrameRange;
				SplitFrameRange.Name = FrameRange.Name;
				SplitFrameRange.StartFrame = EndFrameToProcessInt32;
				SplitFrameRange.EndFrame = FrameRange.EndFrame;
					
				FrameRange.EndFrame = StartFrameToProcessInt32 - 1;

				ProcessingExcludedFrames.Insert(SplitFrameRange, Index + 1);
			}
			else if (FrameRange.StartFrame <= EndFrameToProcessInt32 && FrameRange.EndFrame >= EndFrameToProcessInt32)
			{
				FrameRange.StartFrame = EndFrameToProcessInt32;
			}
			else if (FrameRange.StartFrame <= StartFrameToProcessInt32 && FrameRange.EndFrame >= StartFrameToProcessInt32)
			{ 
				FrameRange.EndFrame = StartFrameToProcessInt32 - 1;
			}
		}
	}
}

bool UMetaHumanPerformance::ContainsAnimationData() const
{
	return Algo::AnyOf(AnimationData, [](const FFrameAnimationData& InAnimationData)
	{
		return InAnimationData.ContainsData();
	});
}

TArray<FFrameAnimationData> UMetaHumanPerformance::GetAnimationData(int32 InStartFrameNumber, int32 InEndFrameNumber) const
{
	int32 EndFrameNumber = InEndFrameNumber == -1 ? AnimationData.Num() : InEndFrameNumber;

	TArray<FFrameAnimationData> Data;
	Data.Reserve(EndFrameNumber - InStartFrameNumber);

	for (int32 Index = InStartFrameNumber; Index < EndFrameNumber; ++Index)
	{
		Data.Add(AnimationData[Index]);
	}

	return Data;
}

int32 UMetaHumanPerformance::GetNumberOfProcessedFrames() const
{
	int32 ProcessedFrameNum = 0;
	for (const FFrameAnimationData& Data : AnimationData)
	{
		if (!Data.AnimationData.IsEmpty())
		{
			++ProcessedFrameNum;
		}
	}

	return ProcessedFrameNum;
}

const TRange<FFrameNumber>& UMetaHumanPerformance::GetProcessingLimitFrameRange() const
{
	return ProcessingLimitFrameRange;
}

const TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>>& UMetaHumanPerformance::GetMediaFrameRanges() const
{
	return MediaFrameRanges;
}

FFrameNumber UMetaHumanPerformance::GetMediaStartFrame() const
{
	FFrameNumber Frame = -1;

	// Attempt to get start frame from image sequence
	if (FootageCaptureData && !FootageCaptureData->ImageSequences.IsEmpty() && FootageCaptureData->ImageSequences[0] && MediaFrameRanges.Contains(FootageCaptureData->ImageSequences[0]))
	{
		Frame = MediaFrameRanges[FootageCaptureData->ImageSequences[0]].GetLowerBoundValue();
	}

	if (Frame == -1 || InputType == EDataInputType::Audio) // Start frame not set or input type is audio, so attempt to get from audio
	{
		TObjectPtr<class USoundWave> AudioForProcessing = GetAudioForProcessing();
		if (AudioForProcessing && MediaFrameRanges.Contains(AudioForProcessing))
		{
			Frame = MediaFrameRanges[AudioForProcessing].GetLowerBoundValue();
		}
	}

	return Frame;
}

TRange<FFrameNumber> UMetaHumanPerformance::GetExportFrameRange(EPerformanceExportRange InExportRange) const
{
	if (InExportRange == EPerformanceExportRange::ProcessingRange)
	{
		return TRange<FFrameNumber>(static_cast<int32>(StartFrameToProcess), static_cast<int32>(EndFrameToProcess));
	}
	else
	{
		return ProcessingLimitFrameRange;
	}
}

USkeletalMesh* UMetaHumanPerformance::GetVisualizationMesh() const
{
	if (VisualizationMesh != nullptr)
	{
		return VisualizationMesh;
	}

	if (Identity != nullptr)
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->IsConformalRigValid())
			{
				return Face->RigComponent->GetSkeletalMeshAsset();
			}
		}
	}

	return nullptr;
}

bool UMetaHumanPerformance::HasValidAnimationPose() const
{
	if (ContainsAnimationData())
	{
		const FFrameAnimationData* FoundFrameWithValidPose = AnimationData.FindByPredicate([](const FFrameAnimationData& InAnimationData)
			{
				return InAnimationData.Pose.IsValid();
			});

		return FoundFrameWithValidPose != nullptr;
	}

	return false;
}

FTransform UMetaHumanPerformance::GetFirstValidAnimationPose() const
{
	if (ContainsAnimationData())
	{
		const FFrameAnimationData* FoundFrameWithValidPose = AnimationData.FindByPredicate([](const FFrameAnimationData& InAnimationData)
			{
				return InAnimationData.Pose.IsValid();
			});

		if (FoundFrameWithValidPose != nullptr)
		{
			return FoundFrameWithValidPose->Pose;
		}
	}

	return FTransform::Identity;
}

FTransform UMetaHumanPerformance::CalculateReferenceFramePose() 
{
	FTransform ReferenceFrameRootPose = FTransform::Identity;

	if (ContainsAnimationData())
	{ 
		// handle back-compatibility case where the new field HeadMovementReferenceFrame has been initialized to the default value of 0 but the 
		// first processed frame is above this
		if (HeadMovementReferenceFrame == 0 && HeadMovementReferenceFrame < uint32(GetProcessingLimitFrameRange().GetLowerBoundValue().Value) )
		{
			UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Initializing new property HeadMovementReferenceFrame to the first valid frame number"));
			HeadMovementReferenceFrame = GetProcessingLimitFrameRange().GetLowerBoundValue().Value;
		}

		uint32 RefFrameFinal = HeadMovementReferenceFrame;

		// handle the special case where we have selected the EndFrameToProcess as the reference frame: this is 1 past the end of the processed sequence so need to use the 
		// previous frame to avoid breaking the head transform ... unless the EndFrameToProcess is at the beginning of the sequence, in which case we can't. NOT IDEAL, but the least bad solution.
		if (EndFrameToProcess != GetProcessingLimitFrameRange().GetLowerBoundValue().Value && RefFrameFinal == EndFrameToProcess)
		{
			RefFrameFinal--;
		}
		const FFrameAnimationData& RefFrameAnimData = AnimationData[RefFrameFinal - GetProcessingLimitFrameRange().GetLowerBoundValue().Value];
		HeadMovementReferenceFrameCalculated = RefFrameFinal - GetProcessingLimitFrameRange().GetLowerBoundValue().Value;

		if (RefFrameAnimData.Pose.IsValid())
		{
			ReferenceFrameRootPose = RefFrameAnimData.Pose;
		}

		if (bAutoChooseHeadMovementReferenceFrame)
		{
			FQuat FrontalRotation = FRotator{ 0.0, 90.0, 0.0 }.Quaternion();
			float MinAngle = TNumericLimits<float>::Max();
			for (int32 Frame = 0; Frame < AnimationData.Num(); Frame++)
			{
				const FFrameAnimationData& CurAnimData = AnimationData[Frame];
				if (CurAnimData.Pose.IsValid())
				{
					FQuat RelativeQuaternion = FrontalRotation.Inverse() * CurAnimData.Pose.GetRotation();
					float Angle = 2.0f * FMath::Acos(RelativeQuaternion.W);
					if (Angle < MinAngle)
					{
						MinAngle = Angle;
						ReferenceFrameRootPose = CurAnimData.Pose;
						HeadMovementReferenceFrameCalculated = Frame;
					}
				}
			}
		}
	}

	return ReferenceFrameRootPose;
}


TSet<FString> UMetaHumanPerformance::GetAnimationCurveNames() const
{
	TSet<FString> CurveNames;

	if (ContainsAnimationData())
	{
		const FFrameAnimationData& FirstAnimationFrame = AnimationData[0];
		FirstAnimationFrame.AnimationData.GetKeys(CurveNames);
	}

	return CurveNames;
}

void UMetaHumanPerformance::LoadDefaultTracker()
{
	if (DefaultTracker == nullptr)
	{
		static constexpr const TCHAR* GenericTrackerPath = TEXT("/" UE_PLUGIN_NAME "/GenericTracker/GenericFaceContourTracker.GenericFaceContourTracker");
		if (UMetaHumanFaceContourTrackerAsset* Tracker = LoadObject<UMetaHumanFaceContourTrackerAsset>(GetTransientPackage(), GenericTrackerPath))
		{
			DefaultTracker = Tracker;
		}
	}
}

void UMetaHumanPerformance::LoadDefaultSolver()
{
	if (DefaultSolver == nullptr)
	{
		static constexpr const TCHAR* GenericSolverPath = TEXT("/" UE_PLUGIN_NAME "/Solver/GenericFaceAnimationSolver.GenericFaceAnimationSolver");
		if (UMetaHumanFaceAnimationSolver* Solver = LoadObject<UMetaHumanFaceAnimationSolver>(GetTransientPackage(), GenericSolverPath))
		{
			DefaultSolver = Solver;
		}
	}
}

void UMetaHumanPerformance::LoadDefaultControlRig()
{
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject)) // The compiler may not have been registered to handle the FaceboardControlRig blueprint class yet
	{
		if (ControlRigClass == nullptr)
		{
			const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
			TArray<FAssetData> FaceboardCRAssetData;
			AssetRegistry.GetAssetsByPackageName(TEXT("/" UE_PLUGIN_NAME "/IdentityTemplate/Face_ControlBoard_CtrlRig"), FaceboardCRAssetData);
			if (!FaceboardCRAssetData.IsEmpty())
			{
				FAssetData& FaceboardControlRigAsset = FaceboardCRAssetData[0];
				if (FaceboardControlRigAsset.IsValid())
				{
					if (FaceboardControlRigAsset.IsInstanceOf(UControlRigBlueprint::StaticClass()))
					{
						ControlRigClass = Cast<UControlRigBlueprint>(FaceboardControlRigAsset.GetAsset())->GetControlRigClass();
					}
					else if (FaceboardControlRigAsset.IsInstanceOf(URigVMBlueprintGeneratedClass::StaticClass()))
					{
						ControlRigClass = Cast<URigVMBlueprintGeneratedClass>(FaceboardControlRigAsset.GetAsset());
					}
					else
					{
						UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Unsupported Face_ControlBoard_CtrlRig class"));
					}
				}
				else
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Invalid Face_ControlBoard_CtrlRig asset"));
				}
			}
		}
	}
}

#if WITH_EDITOR
bool UMetaHumanPerformance::CanEditChange(const FProperty* InProperty) const
{
	bool bParentCanEditChange = Super::CanEditChange(InProperty);

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bSkipPreview))
	{
		bParentCanEditChange &= (SolveType != ESolveType::Preview);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bSkipTongueSolve))
	{
		bParentCanEditChange &= (GetAudioForProcessing() != nullptr);
	}
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bSkipPerVertexSolve))
	{
		bParentCanEditChange &= (SolveType != ESolveType::Preview);
	}

	return bParentCanEditChange && !IsProcessing();
}
#endif

void UMetaHumanPerformance::UpdateCaptureDataConfigName()
{
	if (DefaultSolver)
	{
		DefaultSolver->GetConfigDisplayName(FootageCaptureData, CaptureDataConfig);
	}
	else
	{
		CaptureDataConfig = TEXT("");
	}
}

bool UMetaHumanPerformance::DepthCameraConsistentWithRGBCameraOrDiagnosticsNotEnabled() const
{
	if (bSkipDiagnostics)
	{
		return true;
	}

	TArray<TPair<FString, FString>> StereoReconstructionPairs;
	TArray<FCameraCalibration> Calibrations;
	verify(IsValid(FootageCaptureData));

	if (FootageCaptureData->CameraCalibrations.IsEmpty())
	{
		return false;
	}

	const UCameraCalibration* CameraCalibration = FootageCaptureData->CameraCalibrations[0];

	if (!IsValid(CameraCalibration))
	{
		return false;
	}

	CameraCalibration->ConvertToTrackerNodeCameraModels(Calibrations, StereoReconstructionPairs);

	FCameraCalibration* DepthCalibration = Calibrations.FindByPredicate([](const FCameraCalibration& InCalibration)
	{
		return InCalibration.CameraType == FCameraCalibration::Depth;
	});

	if (!DepthCalibration)
	{
		return false;
	}

	const int32 RGBIndex = CameraCalibration->GetCalibrationIndexByName(Camera);

	if (!Calibrations.IsValidIndex(RGBIndex))
	{
		return false;
	}

	verify(Calibrations.Num() >= 2);
	FVector RGBTranslation = CalcCameraLocation(Calibrations[RGBIndex].Transform);
	FVector DepthTranslation = CalcCameraLocation(DepthCalibration->Transform);
	FVector Diff = RGBTranslation - DepthTranslation;

	if (Diff.Length() > 0.001) // NB this is the same threshold used in titan
	{
		return false;
	}

	return true;
}

EFrameRangeType UMetaHumanPerformance::GetExcludedFrame(const int32 InFrameNumber) const
{
	if (FFrameRange::ContainsFrame(InFrameNumber, UserExcludedFrames))
	{
		return EFrameRangeType::UserExcluded;
	}
	else if (FFrameRange::ContainsFrame(InFrameNumber, ProcessingExcludedFrames))
	{
		return EFrameRangeType::ProcessingExcluded;
	}
	else if (FFrameRange::ContainsFrame(InFrameNumber, RateMatchingExcludedFrames))
	{
		return EFrameRangeType::RateMatchingExcluded;
	}
	else if (InputType != EDataInputType::Audio && FootageCaptureData && !FootageCaptureData->ImageSequences.IsEmpty() && FootageCaptureData->ImageSequences[0] && FFrameRange::ContainsFrame(InFrameNumber - GetMediaStartFrame().Value, FootageCaptureData->CaptureExcludedFrames))
	{
		return EFrameRangeType::CaptureExcluded;
	}

	return EFrameRangeType::None;
}

FVector UMetaHumanPerformance::GetSkelMeshReferenceBoneLocation(USkeletalMeshComponent* InSkelMeshComponent, const FName& InBoneName)
{
	FVector BonePosition = FVector::ZeroVector;

	USkeletalMesh* SkelMesh = InSkelMeshComponent->GetSkeletalMeshAsset();
	if (SkelMesh)
	{
		const FReferenceSkeleton& RefSkel = SkelMesh->GetRefSkeleton();
		int32 BoneIndex = RefSkel.FindBoneIndex(InBoneName);

		if (BoneIndex != INDEX_NONE)
		{
			BonePosition = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, BoneIndex).GetLocation();
		}
	}

	return BonePosition;
}

bool UMetaHumanPerformance::EstimateFocalLength(FString &OutErrorMessage)
{
	bEstimateFocalLengthOK = false;
	EstimateFocalLengthErrorMessage = "";

	UE::MetaHuman::Pipeline::FPipeline Pipeline;

	check(FootageCaptureData);
	check(FootageCaptureData->ImageSequences.Num() != 0);
	const int32 ViewIndex = FootageCaptureData->GetViewIndexByCameraName(Camera);
	check(ViewIndex >= 0 && ViewIndex < FootageCaptureData->ImageSequences.Num());

	const UImgMediaSource* ImageSequence = FootageCaptureData->ImageSequences[ViewIndex];
	check(ImageSequence);

	TSharedPtr<UE::MetaHuman::Pipeline::FUEImageLoadNode> Color = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FUEImageLoadNode>("Color");
	Color->bFailOnMissingFile = true;

	FString ColorFilePath;
	int32 ColorFrameOffset = 0;
	int32 ColorNumFrames = 0;

	if (FTrackingPathUtils::GetTrackingFilePathAndInfo(ImageSequence, ColorFilePath, ColorFrameOffset, ColorNumFrames))
	{
		const int32 FrameNumberOffset = ColorFrameOffset - MediaFrameRanges[ImageSequence].GetLowerBoundValue().Value;
		UE::MetaHuman::FFrameNumberTransformer FrameNumberTransformer(FrameNumberOffset);
		Color->FramePathResolver = MakeUnique<UE::MetaHuman::FFramePathResolver>(ColorFilePath, MoveTemp(FrameNumberTransformer));

		TSharedPtr<UE::MetaHuman::Pipeline::FNeutralFrameNode> NeutralFrame = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FNeutralFrameNode>("Neutral Frame");
		NeutralFrame->bIsNeutralFrame = true;

		Pipeline.MakeConnection(Color, NeutralFrame);

		RealtimeMonoSolver = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FHyprsenseRealtimeNode>("RealtimeMonoSolver");
		RealtimeMonoSolver->LoadModels();
		RealtimeMonoSolver->SetHeadStabilization(bHeadStabilization);

		Pipeline.MakeConnection(NeutralFrame, RealtimeMonoSolver);

		UE::MetaHuman::Pipeline::FFrameComplete OnFrameComplete;
		UE::MetaHuman::Pipeline::FProcessComplete OnProcessComplete;

		OnFrameComplete.AddUObject(this, &UMetaHumanPerformance::EstimateFocalLengthFrameComplete);
		OnProcessComplete.AddUObject(this, &UMetaHumanPerformance::EstimateFocalLengthProcessComplete);

		uint32 CurrentFrame = OnGetCurrentFrame().Execute().Value;

		if (CurrentFrame >= StartFrameToProcess && CurrentFrame < EndFrameToProcess)
		{
			UE::MetaHuman::Pipeline::FPipelineRunParameters PipelineRunParameters;
			PipelineRunParameters.SetStartFrame(CurrentFrame);
			PipelineRunParameters.SetEndFrame(CurrentFrame + 1);
			PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);
			PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);
			PipelineRunParameters.SetMode(UE::MetaHuman::Pipeline::EPipelineMode::PushSyncNodes);

			Pipeline.Run(PipelineRunParameters);
		}
		else
		{
			EstimateFocalLengthErrorMessage = "Current frame outside of range";
		}
	}
	else
	{
		EstimateFocalLengthErrorMessage = FString::Printf(TEXT("Failed to find image sequence in file path %s"), *ImageSequence->GetFullPath());
	}

	if (!bEstimateFocalLengthOK)
	{
		UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Can not estimate focal length - %s"), *EstimateFocalLengthErrorMessage);
	}

	OutErrorMessage = EstimateFocalLengthErrorMessage;

	return bEstimateFocalLengthOK;
}

void UMetaHumanPerformance::EstimateFocalLengthFrameComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	const FString FocalLengthPin = RealtimeMonoSolver->Name + ".Focal Length Out";
	const FString ConfidencePin = RealtimeMonoSolver->Name + ".Confidence Out";

	if (InPipelineData->HasData<float>(FocalLengthPin) && InPipelineData->HasData<float>(ConfidencePin) && InPipelineData->GetData<float>(ConfidencePin) > 0.5)
	{
		FocalLength = InPipelineData->GetData<float>(FocalLengthPin);
	}
	else
	{ 
		EstimateFocalLengthErrorMessage = "No focal length found";
	}
}

void UMetaHumanPerformance::EstimateFocalLengthProcessComplete(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	if (EstimateFocalLengthErrorMessage.IsEmpty())
	{
		if (InPipelineData->GetExitStatus() == UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok)
		{
			bEstimateFocalLengthOK = true;
		}
		else
		{
			EstimateFocalLengthErrorMessage = "Failed to run";
		}
	}
}

FTransform UMetaHumanPerformance::AudioDrivenHeadPoseTransform(const FTransform& InHeadBonePose) const
{
	FTransform RootBonePose = FMetaHumanHeadTransform::HeadToRoot(InHeadBonePose);
	RootBonePose *= AudioDrivenAnimationViewportTransform;
	return RootBonePose;
}

FTransform UMetaHumanPerformance::AudioDrivenHeadPoseTransformInverse(const FTransform& InRootBonePose) const
{
	FTransform RootBonePose = InRootBonePose* AudioDrivenAnimationViewportTransform.Inverse();
	FTransform HeadBonePose = FMetaHumanHeadTransform::RootToHead(RootBonePose);
	return HeadBonePose;
}

bool UMetaHumanPerformance::FootageCaptureDataViewLookupsAreValid() const
{
	// This function was added as part of a hotfix and is a stop-gap measure for determining if any future index lookups
	// will fail during processing and to give the user a hint about how to resolve the problem. There are changes we
	// can make to the capture data to do this in a far safer and more encapsulated way (not exposing the index) but
	// that would require API changes we can't make as part of a hotfix.

	if (!IsValid(FootageCaptureData))
	{
		const UEnum* InputTypeEnum = StaticEnum<EDataInputType>();
		check(InputTypeEnum);

		const FString ProcessingMode = InputTypeEnum->GetNameStringByValue(static_cast<int64>(InputType));

		UE_LOG(
			LogMetaHumanPerformance,
			Error,
			TEXT("Footage capture data is invalid (nullptr). We were expecting it to be valid for the %s processing mode"),
			*ProcessingMode
		);
		return false;
	}

	const int32 ViewIndex = FootageCaptureData->GetViewIndexByCameraName(Camera);

	// Check the view is valid for the image sequence
	if (!FootageCaptureData->ImageSequences.IsValidIndex(ViewIndex))
	{
		UE_LOG(
			LogMetaHumanPerformance,
			Error,
			TEXT(
				"Failed to find an image sequence for camera \"%s\", please check that the camera names being used "
				"for the processing match those found in the footage capture data calibration"
			),
			*Camera
		);
		return false;
	}

	if (!IsValid(FootageCaptureData->ImageSequences[ViewIndex]))
	{
		UE_LOG(
			LogMetaHumanPerformance,
			Error,
			TEXT("Found an image sequence for camera \"%s\", but it was invalid (nullptr)"),
			*Camera
		);
		return false;
	}

	if (InputType == EDataInputType::DepthFootage)
	{
		// Check the view is valid for the depth sequence
		if (!FootageCaptureData->DepthSequences.IsValidIndex(ViewIndex))
		{
			UE_LOG(
				LogMetaHumanPerformance,
				Error,
				TEXT(
					"Failed to find a depth sequence for camera \"%s\", please check that the camera names being used "
					"for the processing match those found in the footage capture data calibration"
				),
				*Camera
			);
			return false;
		}

		if (!IsValid(FootageCaptureData->DepthSequences[ViewIndex]))
		{
			UE_LOG(
				LogMetaHumanPerformance,
				Error,
				TEXT("Found a depth sequence for camera \"%s\", but it was invalid (nullptr)"),
				*Camera
			);
			return false;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
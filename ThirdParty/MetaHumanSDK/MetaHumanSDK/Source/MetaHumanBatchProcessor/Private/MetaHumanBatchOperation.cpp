// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanBatchOperation.h"
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceExportUtils.h"
#include "MetaHumanBatchLog.h"
#include "Pipeline/Pipeline.h"
#include "Nodes/SpeechToAnimNode.h"
#include "Animation/AnimSequence.h"
#include "LevelSequence.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "Async/Async.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "PackageTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "MetaHumanBatchOperation"

bool FMetaHumanBatchOperationContext::IsValid() const
{
	bool bIsValid = true;

	if (AssetsToProcess.Num() == 0)
	{
		UE_LOG(LogMetaHumanBatch, Warning, TEXT("Invalid Batch Context. No assets were specified."))
		bIsValid = false;
	}

	// Check asset output path name rules
	if (EnumHasAnyFlags(BatchStepsFlags, EBatchOperationStepsFlags::SoundWaveToPerformance))
	{
		if (!ValidatePerformanceNameRule())
		{
			UE_LOG(LogMetaHumanBatch, Warning, TEXT("Invalid Batch Context. Performance output asset paths override source asset paths."));
			bIsValid = false;
		}
	}

	if (EnumHasAnyFlags(BatchStepsFlags, EBatchOperationStepsFlags::ExportAnimSequence | EBatchOperationStepsFlags::ExportLevelSequence))
	{
		if (!ValidateExportAssetNameRule())
		{
			UE_LOG(LogMetaHumanBatch, Warning, TEXT("Invalid Batch Context. Export output asset paths override source asset paths."));
			bIsValid = false;
		}
	}

	// Check for target skeleton if exporting anim sequence
	if (EnumHasAnyFlags(BatchStepsFlags, EBatchOperationStepsFlags::ExportAnimSequence))
	{
		if (TargetSkeletonOrSkeletalMesh.IsNull())
		{
			UE_LOG(LogMetaHumanBatch, Warning, TEXT("Invalid Batch Context. A target skeleton or skel mesh must be specified when exporting anim sequence."));
			bIsValid = false;
		}
	}

	return bIsValid;
}

bool FMetaHumanBatchOperationContext::ValidatePerformanceNameRule() const
{
	for (const TWeakObjectPtr<UObject> AssetPtr : AssetsToProcess)
	{
		const UObject* Asset = AssetPtr.Get();
		FString AssetPathName = Asset->GetPathName();

		const FString TargetPerfName = PerformanceNameRule.Rename(Asset);
		const FString TargetPerfPathName = UPackageTools::SanitizePackageName(PerformanceNameRule.FolderPath + TEXT("/") + TargetPerfName) + TEXT(".") + TargetPerfName;

		if (AssetPathName == TargetPerfPathName)
		{
			return false;
		}
	}

	return true;
}

bool FMetaHumanBatchOperationContext::ValidateExportAssetNameRule() const
{
	for (TWeakObjectPtr<UObject> AssetPtr : AssetsToProcess)
	{
		UObject* Asset = AssetPtr.Get();
		FString AssetPathName = Asset->GetPathName();

		const FString TargetExportName = ExportedAssetNameRule.Rename(Asset);
		const FString TargetExportPathName = UPackageTools::SanitizePackageName(ExportedAssetNameRule.FolderPath + TEXT("/") + TargetExportName) + TEXT(".") + TargetExportName;

		if (AssetPathName == TargetExportPathName)
		{
			return false;
		}

		const FString TargetPerfName = PerformanceNameRule.Rename(Asset);
		const FString TargetPerfPathName = UPackageTools::SanitizePackageName(PerformanceNameRule.FolderPath + TEXT("/") + TargetPerfName) + TEXT(".") + TargetPerfName;

		if (TargetPerfPathName == TargetExportPathName)
		{
			return false;
		}
	}

	return true;
}

void UMetaHumanBatchOperation::RunProcess(FMetaHumanBatchOperationContext& InContext)
{
	const int32 FixedSteps = 1;
	const int32 StepsPerAsset = EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::SoundWaveToPerformance) +
		EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::ProcessPerformance) +
		EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::ExportAnimSequence) +
		EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::ExportLevelSequence);

	const int32 NumProgressSteps = FixedSteps + StepsPerAsset * InContext.AssetsToProcess.Num();

	FScopedSlowTask Progress(NumProgressSteps, LOCTEXT("BatchProcessingAudio", "Batch process audio assets to animation..."));
	constexpr bool bShowCancelButton = true;
	Progress.MakeDialog(bShowCancelButton);

	if (!InContext.IsValid())
	{
		NotifyResults(InContext, Progress, true);
		return;
	}

	// Load export targets
	UObject* SkeletonOrSkelMesh = InContext.TargetSkeletonOrSkeletalMesh.LoadSynchronous();
	ExportSkeleton = Cast<USkeleton>(SkeletonOrSkelMesh);
	ExportSkeletalMesh = Cast<USkeletalMesh>(SkeletonOrSkelMesh);
	ExportMetaHuman = InContext.TargetMetaHuman.LoadSynchronous();
	if (EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::ExportAnimSequence) && !ExportSkeleton && !ExportSkeletalMesh)
	{
		UE_LOG(LogMetaHumanBatch, Warning, TEXT("Batch speech to animation aborted. Unable to load skeleton or skeltal mesh needed to export anim sequence"));
		NotifyResults(InContext, Progress, true);
		return;
	}

	bool bProcessingErrorOccurred = false;
	for (TWeakObjectPtr<UObject> AssetPtr : InContext.AssetsToProcess)
	{
		TObjectPtr<USoundWave> SoundWave = Cast<USoundWave>(AssetPtr.Get());
		TObjectPtr<UMetaHumanPerformance> Performance = Cast<UMetaHumanPerformance>(AssetPtr.Get());

		if (SoundWave)
		{
			if (EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::SoundWaveToPerformance) && SoundWave)
			{
				Performance = CreatePerformanceFromSoundWave(InContext, SoundWave, Progress);
			}
			else
			{
				Performance = GetTransientPerformance(InContext, SoundWave);
			}
			SetupPerformance(InContext, SoundWave, Performance);
		}

		if (EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::ProcessPerformance) && Performance)
		{
			if (!ProcessPerformanceAsset(InContext, Performance, Progress))
			{
				bProcessingErrorOccurred = true;
				break;
			}
		}

		if (EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::ExportAnimSequence) && Performance)
		{
			ExportAnimationSequence(InContext, SoundWave, Performance, Progress);
		}

		if (EnumHasAnyFlags(InContext.BatchStepsFlags, EBatchOperationStepsFlags::ExportLevelSequence) && Performance)
		{
			ExportLevelSequence(InContext, SoundWave, Performance, Progress);
		}
	}

	OverwriteExistingAssets(InContext, Progress);
	NotifyResults(InContext, Progress, bProcessingErrorOccurred);
	CleanupIfCancelled(Progress);
}

FString GetUniqueAssetName(const UObject* InAsset, const EditorAnimUtils::FNameDuplicationRule& InNameRule)
{
	FString TargetAssetName = InNameRule.Rename(InAsset);
	FString TargetBasePackageName = InNameRule.FolderPath + "/" + TargetAssetName;

	FString OutPackageName, TargetUniqueAssetName;
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.CreateUniqueAssetName(TargetBasePackageName, TEXT(""), OutPackageName, TargetUniqueAssetName);

	return TargetUniqueAssetName;
}

TObjectPtr<UMetaHumanPerformance> UMetaHumanBatchOperation::CreatePerformanceFromSoundWave(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSoundWave, FScopedSlowTask& InProgress)
{
	if (InProgress.ShouldCancel())
	{
		return nullptr;
	}

	FString AssetName = InSoundWave->GetName();
	InProgress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("CreatingPerformanceAsset", "Creating performance asset from: {0}"), FText::FromString(AssetName)));

	FString TargetAssetName = GetUniqueAssetName(InSoundWave, InContext.PerformanceNameRule);
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UMetaHumanPerformance* Performance = Cast<UMetaHumanPerformance>(AssetTools.CreateAsset(TargetAssetName, InContext.PerformanceNameRule.FolderPath, UMetaHumanPerformance::StaticClass(), nullptr));

	if (Performance)
	{
		CreatedAssets.Add({FAssetData(InSoundWave), FAssetData(Performance)});
	}

	return Performance;
}

TObjectPtr<UMetaHumanPerformance> UMetaHumanBatchOperation::GetTransientPerformance(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSoundWave)
{
	if (!TransientPerformance)
	{
		TransientPerformance = NewObject<UMetaHumanPerformance>(this, FName(TEXT("BatchTransientMetaHumanPerformance")), RF_Transient);
	}

	TObjectPtr<UMetaHumanPerformance> Performance = TransientPerformance;

	FString TargetAssetName = GetUniqueAssetName(InSoundWave, InContext.PerformanceNameRule);	
	Performance->Rename(*TargetAssetName);
	return Performance;
}

void UMetaHumanBatchOperation::SetupPerformance(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSoundWave, TObjectPtr<UMetaHumanPerformance> InPerformance)
{
	if (InPerformance)
	{
		USkeletalMesh* VisualizationMesh = ExportSkeletalMesh;

		if (!VisualizationMesh && ExportSkeleton)
		{
			VisualizationMesh = ExportSkeleton->GetPreviewMesh(true);
		}

		InPerformance->InputType = EDataInputType::Audio;
		InPerformance->Audio = InSoundWave;
		InPerformance->VisualizationMesh = VisualizationMesh;
		InPerformance->bGenerateBlinks = InContext.bGenerateBlinks;
		InPerformance->bDownmixChannels = InContext.bMixAudioChannels;
		InPerformance->AudioChannelIndex = InContext.AudioChannelIndex;
		InPerformance->HeadMovementMode = InContext.bEnableHeadMovement ? EPerformanceHeadMovementMode::ControlRig : EPerformanceHeadMovementMode::Disabled;
		InPerformance->AudioDrivenAnimationSolveOverrides = InContext.AudioDrivenAnimationSolveOverrides;
		InPerformance->AudioDrivenAnimationOutputControls = InContext.AudioDrivenAnimationOutputControls;

		static const FName AudioPropertyName = GET_MEMBER_NAME_STRING_CHECKED(UMetaHumanPerformance, Audio);
		static FProperty* AudioProperty = UMetaHumanPerformance::StaticClass()->FindPropertyByName(AudioPropertyName);
		FPropertyChangedEvent AudioChangedEvent(AudioProperty);
		InPerformance->PostEditChangeProperty(AudioChangedEvent);
	}
}

bool UMetaHumanBatchOperation::ProcessPerformanceAsset(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<UMetaHumanPerformance> InPerformance, FScopedSlowTask& InProgress)
{
	if (InProgress.ShouldCancel())
	{
		return false;
	}

	FString AssetName = InPerformance->GetName();
	InProgress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("ProcessingPerformanceAsset", "Processing performance asset: {0}"), FText::FromString(AssetName)));
	InProgress.ForceRefresh();

	if (!InPerformance->CanProcess())
	{
		UE_LOG(LogMetaHumanBatch, Warning, TEXT("Unable to process performance: %s"), *AssetName);
		return false;
	}

	// Set up custom pipeline for processing speech to face
	// Note we are using a custom pipeline so we can run the processing in an async thread and keeping ticking progress on the UI to keep it responsive
	TSharedPtr<UE::MetaHuman::Pipeline::FPipeline> Speech2FacePipeline = MakeShared<UE::MetaHuman::Pipeline::FPipeline>();
	TSharedPtr<UE::MetaHuman::Pipeline::FSpeechToAnimNode> SpeechToAnimNode = MakeShared<UE::MetaHuman::Pipeline::FSpeechToAnimNode>("SpeechToAnimNode");
	SpeechToAnimNode->LoadModels();
	SpeechToAnimNode->Audio = InPerformance->GetAudioForProcessing();
	SpeechToAnimNode->bDownmixChannels = InPerformance->bDownmixChannels;
	SpeechToAnimNode->AudioChannelIndex = InPerformance->AudioChannelIndex;
	SpeechToAnimNode->FrameRate = InPerformance->GetFrameRate().AsDecimal();
	SpeechToAnimNode->bGenerateBlinks = InPerformance->bGenerateBlinks;
	SpeechToAnimNode->SetMood(InPerformance->AudioDrivenAnimationSolveOverrides.Mood);
	SpeechToAnimNode->SetMoodIntensity(InPerformance->AudioDrivenAnimationSolveOverrides.MoodIntensity);
	SpeechToAnimNode->SetOutputControls(InPerformance->AudioDrivenAnimationOutputControls);

	Speech2FacePipeline->AddNode(SpeechToAnimNode);

	TRange<FFrameNumber> PerfFrameRange = InPerformance->GetProcessingLimitFrameRange();

	// Update animation in performance as pipeline pushes out animation on frame complete
	UE::MetaHuman::Pipeline::FFrameComplete OnFrameComplete;
	OnFrameComplete.AddLambda(
		[InPerformance, PerfFrameRange](TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
		{
			const int32 FrameNumber = InPipelineData->GetFrameNumber() - PerfFrameRange.GetLowerBoundValue().Value;
			FFrameAnimationData& AnimationFrame = InPerformance->AnimationData[FrameNumber];
			AnimationFrame = InPipelineData->MoveData<FFrameAnimationData>("SpeechToAnimNode.Animation Out");

			FTransform TransformedPose = InPerformance->AudioDrivenHeadPoseTransform(AnimationFrame.Pose);
			AnimationFrame.Pose = TransformedPose;
		}
	);

	bool bProcessStatusOk = false;
	UE::MetaHuman::Pipeline::FFrameComplete OnProcessComplete;
	OnProcessComplete.AddLambda([InPerformance, &bProcessStatusOk](TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData){
		UE::MetaHuman::Pipeline::EPipelineExitStatus ExitStatus = InPipelineData->GetExitStatus();
		bProcessStatusOk = ExitStatus == UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok || ExitStatus == UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted;
		if (!bProcessStatusOk)
		{
			UE_LOG(LogMetaHumanBatch, Warning, TEXT("Performance processing pipeline exited with an error %s"), *(InPipelineData->GetErrorMessage()));
		}
	});

	UE::MetaHuman::Pipeline::FPipelineRunParameters PipelineRunParameters;
	PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);
	PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);
	PipelineRunParameters.SetMode(UE::MetaHuman::Pipeline::EPipelineMode::PushSync);
	PipelineRunParameters.SetRestrictStartingToGameThread(false);
	PipelineRunParameters.SetCheckProcessingSpeed(false);
	PipelineRunParameters.SetStartFrame(PerfFrameRange.GetLowerBoundValue().Value);
	PipelineRunParameters.SetEndFrame(PerfFrameRange.GetUpperBoundValue().Value);

	// Run pipeline in async thread
	TFuture<void> PipelineRunFuture = AsyncThread([Speech2FacePipeline, PipelineRunParameters]() {
		Speech2FacePipeline->Run(PipelineRunParameters);
	});

	// Keep ticking until either finished or cancelled
	while (!PipelineRunFuture.WaitFor(FTimespan::FromMilliseconds(100)))
	{
		InProgress.TickProgress();
		if (InProgress.ShouldCancel())
		{
			SpeechToAnimNode->CancelModelSolve();
			Speech2FacePipeline->Cancel();
			break;
		}
	}

	return bProcessStatusOk;
}

void UMetaHumanBatchOperation::ExportAnimationSequence(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSourceSoundWave, TObjectPtr<UMetaHumanPerformance> InPerformance, FScopedSlowTask& InProgress)
{
	if (InProgress.ShouldCancel())
	{
		return;
	}

	UObject* SourceAsset = (InSourceSoundWave != nullptr) ? Cast<UObject>(InSourceSoundWave) : Cast<UObject>(InPerformance);
	FString ExportAssetName = GetUniqueAssetName(SourceAsset, InContext.ExportedAssetNameRule);

	if (InPerformance)
	{
		FString AssetName = InPerformance->GetName();
		InProgress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("ExportingAnimSequence", "Exporting anim sequence for: {0}"), FText::FromString(AssetName)));

		if (!InPerformance->CanExportAnimation())
		{
			UE_LOG(LogMetaHumanBatch, Warning, TEXT("Unable to export anim sequence from performance: %s"), *AssetName);
			return;
		}

		UMetaHumanPerformanceExportAnimationSettings* ExportSettings = GetMutableDefault<UMetaHumanPerformanceExportAnimationSettings>();
		ExportSettings->bShowExportDialog = false;
		ExportSettings->bAutoSaveAnimSequence = false;
		ExportSettings->bEnableHeadMovement = InContext.bEnableHeadMovement;
		ExportSettings->TargetSkeletonOrSkeletalMesh = ExportSkeletalMesh ? Cast<UObject>(ExportSkeletalMesh) : Cast<UObject>(ExportSkeleton);
		ExportSettings->CurveInterpolation = InContext.CurveInterpolation;
		ExportSettings->PackagePath = InContext.ExportedAssetNameRule.FolderPath;
		ExportSettings->AssetName = ExportAssetName;

		if (UAnimSequence* ExportedAnimSequence = UMetaHumanPerformanceExportUtils::ExportAnimationSequence(InPerformance, ExportSettings))
		{
			CreatedAssets.Add({FAssetData(SourceAsset), FAssetData(ExportedAnimSequence)});
		}
	}
}

void UMetaHumanBatchOperation::ExportLevelSequence(const FMetaHumanBatchOperationContext& InContext, TObjectPtr<USoundWave> InSourceSoundWave, TObjectPtr<UMetaHumanPerformance> InPerformance, FScopedSlowTask& InProgress)
{
	if (InProgress.ShouldCancel())
	{
		return;
	}

	UObject* SourceAsset = (InSourceSoundWave != nullptr) ? Cast<UObject>(InSourceSoundWave) : Cast<UObject>(InPerformance);
	FString ExportAssetName = GetUniqueAssetName(SourceAsset, InContext.ExportedAssetNameRule);

	if (InPerformance)
	{
		FString AssetName = InPerformance->GetName();
		InProgress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("ExportingLevelSequence", "Exporting level sequence for: {0}"), FText::FromString(AssetName)));

		if (!InPerformance->CanExportAnimation())
		{
			UE_LOG(LogMetaHumanBatch, Warning, TEXT("Unable to export level sequence from performance: %s"), *AssetName);
			return;
		}

		UMetaHumanPerformanceExportLevelSequenceSettings* ExportSettings = GetMutableDefault<UMetaHumanPerformanceExportLevelSequenceSettings>();
		ExportSettings->bShowExportDialog = false;
		ExportSettings->bExportVideoTrack = false;
		ExportSettings->bExportDepthTrack = false;
		ExportSettings->bExportDepthMesh = false;
		ExportSettings->bExportAudioTrack = InContext.bExportAudioTrack;
		ExportSettings->bExportCamera = InContext.bExportCamera;
		ExportSettings->bExportImagePlane = false;
		ExportSettings->bExportIdentity = false;
		ExportSettings->bExportControlRigTrack = false;
		ExportSettings->bEnableControlRigHeadMovement = false;
		ExportSettings->bExportTransformTrack = false;
		ExportSettings->bEnableMetaHumanHeadMovement = InContext.bEnableHeadMovement;
		ExportSettings->CurveInterpolation = InContext.CurveInterpolation;

		ExportSettings->TargetMetaHumanClass = ExportMetaHuman;

		ExportSettings->PackagePath = InContext.ExportedAssetNameRule.FolderPath;
		ExportSettings->AssetName = ExportAssetName;

		if (ULevelSequence* ExportedLevelSequence = UMetaHumanPerformanceExportUtils::ExportLevelSequence(InPerformance, ExportSettings))
		{
			CreatedAssets.Add({FAssetData(SourceAsset), FAssetData(ExportedLevelSequence)});
		}
	}
}

void UMetaHumanBatchOperation::OverwriteExistingAssets(const FMetaHumanBatchOperationContext& InContext, FScopedSlowTask& InProgress)
{
	if (InProgress.ShouldCancel())
	{
		return;
	}

	if (!InContext.bOverrideAssets)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	for(const TPair<FAssetData, FAssetData>& AssetData : CreatedAssets)
	{
		UObject* SourceAsset = AssetData.Key.GetAsset();
		UObject* CreatedAsset = AssetData.Value.GetAsset();

		FString DesiredObjectName;
		if (Cast<UMetaHumanPerformance>(CreatedAsset) != nullptr)
		{
			DesiredObjectName = InContext.PerformanceNameRule.Rename(SourceAsset);
		}
		else
		{ 
			DesiredObjectName = InContext.ExportedAssetNameRule.Rename(SourceAsset);
		}

		if (CreatedAsset->GetName() == DesiredObjectName)
		{
			// asset was not renamed due to collision with existing asset, so there's nothing to replace
			continue;
		}

		FString PathName = FPackageName::GetLongPackagePath(CreatedAsset->GetPathName());
		FString DesiredPackageName = PathName + "/" + DesiredObjectName;
		FString DesiredObjectPath = DesiredPackageName + "." + DesiredObjectName;
		FAssetData AssetDataToReplace = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(DesiredObjectPath));

		const bool bHasDuplicateToReplace = AssetDataToReplace.IsValid() && AssetDataToReplace.GetAsset()->GetClass() == CreatedAsset->GetClass();
		if (!bHasDuplicateToReplace)
		{
			// this could happen if the desired name was already in use by a different asset type
			continue;
		}

		UObject* AssetToReplace = AssetDataToReplace.GetAsset();
		if (AssetToReplace == SourceAsset)
		{
			// we never the source asset, only previously created assets
			continue;
		}

		// reroute all references from old asset to new asset
		TArray<UObject*> AssetsToReplace = {AssetToReplace};
		ObjectTools::ForceReplaceReferences(CreatedAsset, AssetsToReplace);
		
		// delete the old asset
		ObjectTools::ForceDeleteObjects({AssetToReplace}, false /*bShowConfirmation*/);
			
		// rename the new asset with the desired name
		FString CurrentAssetPath = CreatedAsset->GetPathName();
		TArray<FAssetRenameData> AssetsToRename = { FAssetRenameData(CurrentAssetPath, DesiredObjectPath) };
		AssetToolsModule.Get().RenameAssets(AssetsToRename);
	}
}

void UMetaHumanBatchOperation::NotifyResults(const FMetaHumanBatchOperationContext& InContext, FScopedSlowTask& InProgress, bool bInErrorOccurred)
{
	// create pop-up notification in editor UI
	constexpr float NotificationDuration = 5.0f;
	FNotificationInfo Notification(FText::GetEmpty());
	Notification.ExpireDuration = NotificationDuration;

	if (InProgress.ShouldCancel())
	{
		InProgress.EnterProgressFrame(1.f, FText(LOCTEXT("CancelledBatchProcessMetaHuman", "Cancelled.")));

		// notify user that processing was cancelled		
		Notification.Text = FText(LOCTEXT("CancelledBatchProcessMetaHumanPerformance", "Process MetaHuman Performance cancelled."));
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
	else if (bInErrorOccurred)
	{
		Notification.Text = FText(LOCTEXT("ErroredBatchProcessMetaHumanPerformance", "Error processing MetaHuman Performance. See output log for details."));
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
	else
	{
		InProgress.EnterProgressFrame(1.f, FText(LOCTEXT("DoneBatchProcessMetaHumanPerformance", "Process MetaHuman Performance complete!")));

		TArray<FAssetData> CreatedAssetsArray;
		for(const TPair<FAssetData, FAssetData>& AssetData : CreatedAssets)
		{
			CreatedAssetsArray.Add(AssetData.Value);
		}

		const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(CreatedAssetsArray);
		
		// log details of what assets were created
		for (const FAssetData& CreatedAssetData : CreatedAssetsArray)
		{
			UE_LOG(LogMetaHumanBatch, Display, TEXT("Process MetaHuman Performance - New Asset Created: %s"), *(CreatedAssetData.GetAsset()->GetPathName()));
		}

		// notify user that processing completed
		Notification.Text = FText::Format(
			LOCTEXT("MultiProcessMetaHumanPerformances", "{0} assets were created. See Output for details."),
			FText::AsNumber(CreatedAssets.Num()));
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
}

void UMetaHumanBatchOperation::CleanupIfCancelled(const FScopedSlowTask& InProgress) const
{
	if (!InProgress.ShouldCancel())
	{
		return;
	}

	TArray<UObject*> NewAssets;
	for(const TPair<FAssetData, FAssetData>& AssetData : CreatedAssets)
	{
		NewAssets.Add(AssetData.Value.GetAsset());
	}

	// delete any newly created assets
	constexpr bool bShowConfirmation = true;
	ObjectTools::DeleteObjects(NewAssets, bShowConfirmation);
}

#undef LOCTEXT_NAMESPACE

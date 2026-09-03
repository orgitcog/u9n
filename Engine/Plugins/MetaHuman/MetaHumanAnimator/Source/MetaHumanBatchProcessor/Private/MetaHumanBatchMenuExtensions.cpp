// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanBatchMenuExtensions.h"
#include "ContentBrowserMenuContexts.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "ToolMenus.h"
#include "SMetaHumanBatchExportPathDialog.h"
#include "MetaHumanSpeechProcessingSettings.h"
#include "SMetaHumanSpeechProcessingSettings.h"
#include "MetaHumanBatchOperation.h"
#include "Sound/SoundWave.h"
#include "MetaHumanPerformance.h"

#define LOCTEXT_NAMESPACE "MetaHumanBatchMenuExtensions"

namespace MetaHumanBatchMenuExtension
{
	/** Finds assets in InContext of type InClass and sets them as assets to process in batch context */
	void SetAssetsToProcess(FMetaHumanBatchOperationContext& InBatchContext, const FToolMenuContext& InContext, const UClass* InClass)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			for (const FAssetData& Asset : Context->GetSelectedAssetsOfType(InClass))
			{
				if (UObject* Object = Cast<UObject>(Asset.GetAsset()))
				{
					InBatchContext.AssetsToProcess.Add(Object);
				}
			}
		}
	}

	/** Get the performance batch path dialog */
	const TSharedRef<SMetaHumanBatchExportPathDialog> GetPerformanceBatchPathDialog(FMetaHumanBatchOperationContext& InBatchContext, bool bSetPathFromAsset)
	{
		static FText CanProcessText = FText::FromString(TEXT(""));
		static FText CanNotProcessText =  FText(LOCTEXT("MetaHumanBatchPaths_Performance", "Invalid paths. Output paths override source asset paths."));
	
		// Create dialog to select performance output paths
		TAttribute<FCanProcessResult> CanProcessConditional = TAttribute<FCanProcessResult>::CreateLambda([&]{ 
			bool bValidNameRule = InBatchContext.ValidatePerformanceNameRule();
			return FCanProcessResult(bValidNameRule, (bValidNameRule ? CanProcessText : CanNotProcessText));
		});

		if (bSetPathFromAsset && !InBatchContext.AssetsToProcess.IsEmpty())
		{
			FString FolderPath, RightS;
			InBatchContext.AssetsToProcess[0].Get()->GetPathName().Split(TEXT("/"), &FolderPath, &RightS, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			InBatchContext.PerformanceNameRule.FolderPath = FolderPath;
		}

		const TSharedRef<SMetaHumanBatchExportPathDialog> PathDialog = SNew(SMetaHumanBatchExportPathDialog)
			.NameRule(&InBatchContext.PerformanceNameRule)
			.AssetTypeName("Performance")
			.PrefixHint("e.g. PERF_")
			.CanProcessConditional(CanProcessConditional);

		return PathDialog;
	}

	/** Get the anim sequence batch path dialog */
	const TSharedRef<SMetaHumanBatchExportPathDialog> GetAnimSequenceBatchPathDialog(FMetaHumanBatchOperationContext& InBatchContext, bool bSetPathFromAsset)
	{
		static FText CanProcessText = FText::FromString(TEXT(""));
		static FText CanNotProcessText =  FText(LOCTEXT("MetaHumanBatchPaths_AnimSequence", "Invalid paths. Output paths override source asset paths."));
	
		// Create dialog to select export output paths
		TAttribute<FCanProcessResult> CanProcessConditional = TAttribute<FCanProcessResult>::CreateLambda([&]{ 
			bool bValidNameRule = InBatchContext.ValidateExportAssetNameRule();
			return FCanProcessResult(bValidNameRule, (bValidNameRule ? CanProcessText : CanNotProcessText));
		});

		if (bSetPathFromAsset && !InBatchContext.AssetsToProcess.IsEmpty())
		{
			FString FolderPath, RightS;
			InBatchContext.AssetsToProcess[0].Get()->GetPathName().Split(TEXT("/"), &FolderPath, &RightS, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			InBatchContext.ExportedAssetNameRule.FolderPath = FolderPath;
		}

		const TSharedRef<SMetaHumanBatchExportPathDialog> PathDialog = SNew(SMetaHumanBatchExportPathDialog)
			.NameRule(&InBatchContext.ExportedAssetNameRule)
			.AssetTypeName("Anim Sequence")
			.PrefixHint("e.g. AS_")
			.CanProcessConditional(CanProcessConditional);

		return PathDialog;
	}

	/** Get the level sequence batch path dialog */
	const TSharedRef<SMetaHumanBatchExportPathDialog> GetLevelSequenceBatchPathDialog(FMetaHumanBatchOperationContext& InBatchContext, bool bSetPathFromAsset)
	{
		static FText CanProcessText = FText::FromString(TEXT(""));
		static FText CanNotProcessText =  FText(LOCTEXT("MetaHumanBatchPaths_LevelSequence", "Invalid paths. Output export paths override source asset paths."));
	
		// Create dialog to select export output paths
		TAttribute<FCanProcessResult> CanProcessConditional = TAttribute<FCanProcessResult>::CreateLambda([&]{ 
			bool bValidNameRule = InBatchContext.ValidateExportAssetNameRule();
			return FCanProcessResult(bValidNameRule, (bValidNameRule ? CanProcessText : CanNotProcessText));
		});

		if (bSetPathFromAsset && !InBatchContext.AssetsToProcess.IsEmpty())
		{
			FString FolderPath, RightS;
			InBatchContext.AssetsToProcess[0].Get()->GetPathName().Split(TEXT("/"), &FolderPath, &RightS, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			InBatchContext.ExportedAssetNameRule.FolderPath = FolderPath;
		}

		const TSharedRef<SMetaHumanBatchExportPathDialog> PathDialog = SNew(SMetaHumanBatchExportPathDialog)
			.NameRule(&InBatchContext.ExportedAssetNameRule)
			.AssetTypeName("Level Sequence")
			.PrefixHint("e.g. LS_")
			.CanProcessConditional(CanProcessConditional);

		return PathDialog;
	}

	static TSoftObjectPtr<UObject> GetFaceArchetypeSkeleton()
	{
		FString NewClassPath;
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		NewClassPath = ContentBrowserModule.Get().GetInitialPathToSaveAsset(FContentBrowserItemPath(NewClassPath, EContentBrowserPathType::Internal)).GetInternalPathString();
		const FSoftObjectPath FaceArchetypePath(NewClassPath + TEXT("/MetaHumans/Common/Face/Face_Archetype_Skeleton.Face_Archetype_Skeleton"));
		return TSoftObjectPtr<UObject>(FaceArchetypePath);
	}

	/** Process speech audio via MetaHuman performance */
	void CreateAndProcessAudioPerformances(const FToolMenuContext& InContext)
	{
		FMetaHumanBatchOperationContext BatchContext;
		EnumAddFlags(BatchContext.BatchStepsFlags, EBatchOperationStepsFlags::SoundWaveToPerformance | EBatchOperationStepsFlags::ProcessPerformance);
		SetAssetsToProcess(BatchContext, InContext, USoundWave::StaticClass());

		const TSharedRef<SMetaHumanBatchExportPathDialog> PathDialog = MetaHumanBatchMenuExtension::GetPerformanceBatchPathDialog(BatchContext, true);
		if (PathDialog->ShowModal() == EAppReturnType::Cancel)
		{
			return;
		}

		// Create dialog to select processing settings
		UMetaHumanSpeechToPerformance* SpeechProcessingSettings = GetMutableDefault<UMetaHumanSpeechToPerformance>();
		SpeechProcessingSettings->VisualizationMesh = GetFaceArchetypeSkeleton();
		TSharedRef<SMetaHumanSpeechToAnimProcessingSettings> SpeechToAnimSettingsDialog = SNew(SMetaHumanSpeechToAnimProcessingSettings).Settings(SpeechProcessingSettings);
		if (SpeechToAnimSettingsDialog->ShowModel() == EAppReturnType::Cancel)
		{
			return;
		}

		BatchContext.bGenerateBlinks = SpeechProcessingSettings->ProcessingSettings.bGenerateBlinks;
		BatchContext.bMixAudioChannels = SpeechProcessingSettings->ProcessingSettings.bMixAudioChannels;
		BatchContext.AudioChannelIndex = SpeechProcessingSettings->ProcessingSettings.AudioChannelIndex;
		BatchContext.TargetSkeletonOrSkeletalMesh = SpeechProcessingSettings->VisualizationMesh;
		BatchContext.bOverrideAssets = SpeechProcessingSettings->bOverwriteAssets;
		BatchContext.AudioDrivenAnimationOutputControls = SpeechProcessingSettings->ProcessingSettings.OutputControls;
		BatchContext.AudioDrivenAnimationSolveOverrides = SpeechProcessingSettings->ProcessingSettings.SolveOverrides;
		BatchContext.bEnableHeadMovement = SpeechProcessingSettings->ProcessingSettings.bEnableHeadMovement;

		// Run batch operation
		const TStrongObjectPtr<UMetaHumanBatchOperation> BatchOperation(NewObject<UMetaHumanBatchOperation>());
		BatchOperation->RunProcess(BatchContext);
	}

	/** Export to anim sequence */
	void ExportAnimSequences(const FToolMenuContext& InContext)
	{
		FMetaHumanBatchOperationContext BatchContext;
		EnumAddFlags(BatchContext.BatchStepsFlags, EBatchOperationStepsFlags::ExportAnimSequence);
		SetAssetsToProcess(BatchContext, InContext, UMetaHumanPerformance::StaticClass());

		const TSharedRef<SMetaHumanBatchExportPathDialog> PathDialog = MetaHumanBatchMenuExtension::GetAnimSequenceBatchPathDialog(BatchContext, true);
		if (PathDialog->ShowModal() == EAppReturnType::Cancel)
		{
			return;
		}

		// Create dialog to select processing settings
		UMetaHumanExportAnimSequenceSettings* ExportAnimSequenceSettings = GetMutableDefault<UMetaHumanExportAnimSequenceSettings>();
		ExportAnimSequenceSettings->ExportSettings.TargetSkeletonOrSkeletalMesh = GetFaceArchetypeSkeleton();
		TAttribute<bool> CanExportConditional = TAttribute<bool>::CreateLambda([ExportAnimSequenceSettings]{ return ExportAnimSequenceSettings->ExportSettings.TargetSkeletonOrSkeletalMesh != nullptr; });
		TSharedRef<SMetaHumanSpeechToAnimProcessingSettings> SpeechToAnimSettingsDialog = SNew(SMetaHumanSpeechToAnimProcessingSettings).Settings(ExportAnimSequenceSettings).CanProcessConditional(CanExportConditional);
		if (SpeechToAnimSettingsDialog->ShowModel() == EAppReturnType::Cancel)
		{
			return;
		}

		BatchContext.TargetSkeletonOrSkeletalMesh = ExportAnimSequenceSettings->ExportSettings.TargetSkeletonOrSkeletalMesh;
		BatchContext.CurveInterpolation = ExportAnimSequenceSettings->ExportSettings.CurveInterpolation;
		BatchContext.bOverrideAssets = ExportAnimSequenceSettings->ExportSettings.bOverwriteAssets;

		// Run batch operation
		const TStrongObjectPtr<UMetaHumanBatchOperation> BatchOperation(NewObject<UMetaHumanBatchOperation>());
		BatchOperation->RunProcess(BatchContext);
	}

	void ExportLevelSequences(const FToolMenuContext& InContext)
	{
		FMetaHumanBatchOperationContext BatchContext;
		EnumAddFlags(BatchContext.BatchStepsFlags, EBatchOperationStepsFlags::ExportLevelSequence);
		SetAssetsToProcess(BatchContext, InContext, UMetaHumanPerformance::StaticClass());
	
		const TSharedRef<SMetaHumanBatchExportPathDialog> PathDialog = MetaHumanBatchMenuExtension::GetLevelSequenceBatchPathDialog(BatchContext, true);
		if (PathDialog->ShowModal() == EAppReturnType::Cancel)
		{
			return;
		}

		// Create dialog to select processing settings
		UMetaHumanExportLevelSequenceSettings* ExportLevelSequenceSettings = GetMutableDefault<UMetaHumanExportLevelSequenceSettings>();
		TAttribute<bool> CanExportConditional = TAttribute<bool>::CreateLambda(
			[ExportLevelSequenceSettings]()
			{
				const bool bExportAudioTrackOrCamera = ExportLevelSequenceSettings->ExportSettings.bExportAudioTrack || ExportLevelSequenceSettings->ExportSettings.bExportCamera;
				const bool bIsValidMetaHumanClass = !ExportLevelSequenceSettings->ExportSettings.TargetMetaHumanClass.IsNull();
				return bExportAudioTrackOrCamera && bIsValidMetaHumanClass;
			}
		);
		TSharedRef<SMetaHumanSpeechToAnimProcessingSettings> SpeechToAnimSettingsDialog = SNew(SMetaHumanSpeechToAnimProcessingSettings).Settings(ExportLevelSequenceSettings).CanProcessConditional(CanExportConditional);
		if (SpeechToAnimSettingsDialog->ShowModel() == EAppReturnType::Cancel)
		{
			return;
		}

		BatchContext.CurveInterpolation = ExportLevelSequenceSettings->ExportSettings.CurveInterpolation;
		BatchContext.TargetMetaHuman = ExportLevelSequenceSettings->ExportSettings.TargetMetaHumanClass;
		BatchContext.bExportCamera  = ExportLevelSequenceSettings->ExportSettings.bExportCamera;
		BatchContext.bExportAudioTrack = ExportLevelSequenceSettings->ExportSettings.bExportAudioTrack;
		BatchContext.bOverrideAssets = ExportLevelSequenceSettings->ExportSettings.bOverwriteAssets;

		// Run batch operation
		const TStrongObjectPtr<UMetaHumanBatchOperation> BatchOperation(NewObject<UMetaHumanBatchOperation>());
		BatchOperation->RunProcess(BatchContext);
	}

	/** Process speech audio via MetaHuman performance and export to anim sequence */
	void ProcessAudioPerformancesToAnimSequences(const FToolMenuContext& InContext)
	{
		FMetaHumanBatchOperationContext BatchContext;
		EnumAddFlags(BatchContext.BatchStepsFlags, EBatchOperationStepsFlags::ProcessPerformance | EBatchOperationStepsFlags::ExportAnimSequence);
		SetAssetsToProcess(BatchContext, InContext, USoundWave::StaticClass());

		const TSharedRef<SMetaHumanBatchExportPathDialog> ExportPathDialog = MetaHumanBatchMenuExtension::GetAnimSequenceBatchPathDialog(BatchContext, true);
		if (ExportPathDialog->ShowModal() == EAppReturnType::Cancel)
		{
			return;
		}

		// Create dialog to select processing settings
		UMetaHumanSpeechToAnimSequenceProcessingSettings* SpeechToAnimSequenceProcessingSettings = GetMutableDefault<UMetaHumanSpeechToAnimSequenceProcessingSettings>();
		SpeechToAnimSequenceProcessingSettings->ExportSettings.TargetSkeletonOrSkeletalMesh = GetFaceArchetypeSkeleton();
		TAttribute<bool> CanExportConditional = TAttribute<bool>::CreateLambda([SpeechToAnimSequenceProcessingSettings]{ return !SpeechToAnimSequenceProcessingSettings->ExportSettings.TargetSkeletonOrSkeletalMesh.IsNull(); });
		TSharedRef<SMetaHumanSpeechToAnimProcessingSettings> SpeechToAnimSettingsDialog = SNew(SMetaHumanSpeechToAnimProcessingSettings).Settings(SpeechToAnimSequenceProcessingSettings).CanProcessConditional(CanExportConditional);
		if (SpeechToAnimSettingsDialog->ShowModel() == EAppReturnType::Cancel)
		{
			return;
		}

		BatchContext.bGenerateBlinks = SpeechToAnimSequenceProcessingSettings->ProcessingSettings.bGenerateBlinks;
		BatchContext.bMixAudioChannels = SpeechToAnimSequenceProcessingSettings->ProcessingSettings.bMixAudioChannels;
		BatchContext.AudioChannelIndex = SpeechToAnimSequenceProcessingSettings->ProcessingSettings.AudioChannelIndex;
		BatchContext.TargetSkeletonOrSkeletalMesh = SpeechToAnimSequenceProcessingSettings->ExportSettings.TargetSkeletonOrSkeletalMesh;
		BatchContext.CurveInterpolation = SpeechToAnimSequenceProcessingSettings->ExportSettings.CurveInterpolation;
		BatchContext.bOverrideAssets = SpeechToAnimSequenceProcessingSettings->ExportSettings.bOverwriteAssets;
		BatchContext.AudioDrivenAnimationSolveOverrides = SpeechToAnimSequenceProcessingSettings->ProcessingSettings.SolveOverrides;
		BatchContext.AudioDrivenAnimationOutputControls = SpeechToAnimSequenceProcessingSettings->ProcessingSettings.OutputControls;
		BatchContext.bEnableHeadMovement = SpeechToAnimSequenceProcessingSettings->ProcessingSettings.bEnableHeadMovement;

		// Run batch operation
		const TStrongObjectPtr<UMetaHumanBatchOperation> BatchOperation(NewObject<UMetaHumanBatchOperation>());
		BatchOperation->RunProcess(BatchContext);
	}

	/** Process speech audio via MetaHuman performance and export to level sequence */
	void ProcessAudioPerformancesToLevelSequences(const FToolMenuContext& InContext)
	{
		FMetaHumanBatchOperationContext BatchContext;
		EnumAddFlags(BatchContext.BatchStepsFlags, EBatchOperationStepsFlags::ProcessPerformance | EBatchOperationStepsFlags::ExportLevelSequence);
		SetAssetsToProcess(BatchContext, InContext, USoundWave::StaticClass());

		const TSharedRef<SMetaHumanBatchExportPathDialog> ExportPathDialog = MetaHumanBatchMenuExtension::GetLevelSequenceBatchPathDialog(BatchContext, true);
		if (ExportPathDialog->ShowModal() == EAppReturnType::Cancel)
		{
			return;
		}

		// Create dialog to select processing settings
		UMetaHumanSpeechToLevelSequenceSettings* SpeechToLevelSequenceSettings = GetMutableDefault<UMetaHumanSpeechToLevelSequenceSettings>();
		TAttribute<bool> CanExportConditional = TAttribute<bool>::CreateLambda(
			[SpeechToLevelSequenceSettings]()
			{
				const bool bExportAudioTrackOrCamera = SpeechToLevelSequenceSettings->ExportSettings.bExportAudioTrack || SpeechToLevelSequenceSettings->ExportSettings.bExportCamera;
				const bool bIsValidMetaHumanClass = !SpeechToLevelSequenceSettings->ExportSettings.TargetMetaHumanClass.IsNull();
				return bExportAudioTrackOrCamera && bIsValidMetaHumanClass;
			}
		);
		TSharedRef<SMetaHumanSpeechToAnimProcessingSettings> SpeechToAnimSettingsDialog = SNew(SMetaHumanSpeechToAnimProcessingSettings).Settings(SpeechToLevelSequenceSettings).CanProcessConditional(CanExportConditional);
		if (SpeechToAnimSettingsDialog->ShowModel() == EAppReturnType::Cancel)
		{
			return;
		}

		BatchContext.bGenerateBlinks = SpeechToLevelSequenceSettings->ProcessingSettings.bGenerateBlinks;
		BatchContext.bMixAudioChannels = SpeechToLevelSequenceSettings->ProcessingSettings.bMixAudioChannels;
		BatchContext.AudioChannelIndex = SpeechToLevelSequenceSettings->ProcessingSettings.AudioChannelIndex;
		BatchContext.CurveInterpolation = SpeechToLevelSequenceSettings->ExportSettings.CurveInterpolation;
		BatchContext.TargetMetaHuman = SpeechToLevelSequenceSettings->ExportSettings.TargetMetaHumanClass;
		BatchContext.bExportCamera  = SpeechToLevelSequenceSettings->ExportSettings.bExportCamera;
		BatchContext.bExportAudioTrack = SpeechToLevelSequenceSettings->ExportSettings.bExportAudioTrack;
		BatchContext.bOverrideAssets = SpeechToLevelSequenceSettings->ExportSettings.bOverwriteAssets;
		BatchContext.AudioDrivenAnimationSolveOverrides = SpeechToLevelSequenceSettings->ProcessingSettings.SolveOverrides;
		BatchContext.AudioDrivenAnimationOutputControls = SpeechToLevelSequenceSettings->ProcessingSettings.OutputControls;
		BatchContext.bEnableHeadMovement = SpeechToLevelSequenceSettings->ProcessingSettings.bEnableHeadMovement;

		// Run batch operation
		const TStrongObjectPtr<UMetaHumanBatchOperation> BatchOperation(NewObject<UMetaHumanBatchOperation>());
		BatchOperation->RunProcess(BatchContext);
	}

	/** If one performance asset is selected, check performance is processed */
	bool CanExportFromPerformance(const FToolMenuContext& InContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			TArray<FAssetData> SelectedAssets =  Context->GetSelectedAssetsOfType(UMetaHumanPerformance::StaticClass());

			if (SelectedAssets.Num() == 1)
			{
				if (UMetaHumanPerformance* Performance = Cast<UMetaHumanPerformance>(SelectedAssets[0].GetAsset()))
				{
					return Performance->CanExportAnimation();
				}
			}
			else
			{
				return true;
			}
		}

		return false;
	}

	/** Fill the sound wave process performance submenu */
	void FillSoundWaveProcessPerformanceSubMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("ProcessPerformances");

		FToolUIAction ProcessAction;
		ProcessAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&MetaHumanBatchMenuExtension::CreateAndProcessAudioPerformances);
		Section.AddMenuEntry("SoundWave_ProcessMetaHumanPerformance", 
			LOCTEXT("SoundWave_ProcessMetaHumanPerformance", "Create Performances And Process"), 
			LOCTEXT("SoundWave_ProcessMetaHumanPerformanceTooltip", "Create MetaHuman performance assets and and process from audio"),
			FSlateIcon(TEXT("MetaHumanPerformanceStyle"), TEXT("Performance.StartProcessingShot"), TEXT("Performance.StartProcessingShot")),
			ProcessAction);

		FToolUIAction ProcessToASAction;
		ProcessToASAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&MetaHumanBatchMenuExtension::ProcessAudioPerformancesToAnimSequences);
		Section.AddMenuEntry("SoundWave_ProcessMetaHumanPerformanceToAS", 
			LOCTEXT("SoundWave_ProcessMetaHumanPerformanceToAS", "Process And Export to Anim Sequences"), 
			LOCTEXT("SoundWave_ProcessMetaHumanPerformanceToASTooltip", "Process audio as MetaHuman performances and export to anim sequences"),
			FSlateIcon(TEXT("MetaHumanPerformanceStyle"), TEXT("Performance.ExportAnimation"), TEXT("Performance.ExportAnimation")),
			ProcessToASAction);

		FToolUIAction ProcessToLSAction;
		ProcessToLSAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&MetaHumanBatchMenuExtension::ProcessAudioPerformancesToLevelSequences);
		Section.AddMenuEntry("SoundWave_ProcessMetaHumanPerformanceToLS", 
			LOCTEXT("SoundWave_ProcessMetaHumanPerformanceToLS", "Process and Export To Level Sequences"), 
			LOCTEXT("SoundWave_ProcessMetaHumanPerformanceToLSTooltip", "Process audio as MetaHuman performances and export to level sequences"),
			FSlateIcon(TEXT("MetaHumanPerformanceStyle"), TEXT("Performance.ExportLevelSequence"), TEXT("Performance.ExportLevelSequence")),
			ProcessToLSAction);
	}

	/** Fill the MetaHuman performance export submenu */
	void FillPerformanceExportSubMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("Export");

		FToolUIAction ExportASAction;
		ExportASAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&MetaHumanBatchMenuExtension::ExportAnimSequences);
		ExportASAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&MetaHumanBatchMenuExtension::CanExportFromPerformance);
		Section.AddMenuEntry("MetaHumanPerformance_ExportAS", 
			LOCTEXT("MetaHumanPerformance_ExportAS", "Export Anim Sequences"), 
			LOCTEXT("MetaHumanPerformance_ExportASTooltip", "Export animation sequences from processed performance"),
			FSlateIcon(TEXT("MetaHumanPerformanceStyle"), TEXT("Performance.ExportAnimation"), TEXT("Performance.ExportAnimation")),
			ExportASAction);

		FToolUIAction ExportLSAction;
		ExportLSAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&MetaHumanBatchMenuExtension::ExportLevelSequences);
		ExportLSAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&MetaHumanBatchMenuExtension::CanExportFromPerformance);
		Section.AddMenuEntry("MetaHumanPerformance_ExportLS", 
			LOCTEXT("MetaHumanPerformance_ExportLS", "Export Level Sequences"), 
			LOCTEXT("MetaHumanPerformance_ExportLSTooltip", "Export level sequences from processed performance"),
			FSlateIcon(TEXT("MetaHumanPerformanceStyle"), TEXT("Performance.ExportLevelSequence"), TEXT("Performance.ExportLevelSequence")),
			ExportLSAction);
	}
}

FMetaHumanBatchMenuExtensions::FMetaHumanBatchMenuExtensions()
{
}

FMetaHumanBatchMenuExtensions::~FMetaHumanBatchMenuExtensions()
{
}

void FMetaHumanBatchMenuExtensions::RegisterMenuExtensions()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMetaHumanBatchMenuExtensions::AddMenuExtensions));
}

void FMetaHumanBatchMenuExtensions::UnregisterMenuExtensions()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FMetaHumanBatchMenuExtensions::AddMenuExtensions()
{
	AddSoundWaveMenuExtensions();
	AddPerformanceMenuExtensions();
}

void FMetaHumanBatchMenuExtensions::AddSoundWaveMenuExtensions()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(USoundWave::StaticClass());

	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
	Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
	{
		InSection.AddSubMenu("SoundWave_ProcessMetaHumanPerformanceSubMenu",
			LOCTEXT("ProcessMetaHumanPerformanceSubMenu", "MetaHuman Performance"),
			LOCTEXT("ProcesssMetaHumanPerformanceSubMenu_Tooltip", "Process MetaHuman performances using speech audio"),
			FNewToolMenuDelegate::CreateStatic(&MetaHumanBatchMenuExtension::FillSoundWaveProcessPerformanceSubMenu),
			false,
			FSlateIcon(TEXT("MetaHumanPerformanceStyle"), TEXT("ClassIcon.MetaHumanPerformance"), TEXT("ClassIcon.MetaHumanPerformance")));
	}));
}

void FMetaHumanBatchMenuExtensions::AddPerformanceMenuExtensions()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UMetaHumanPerformance::StaticClass());

	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
	Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
	{
		InSection.AddSubMenu("MetaHumanPerformance_ExportSubMenu",
			LOCTEXT("MetaHumanPerformanceExportSubMenu", "Export"),
			LOCTEXT("MetaHumanPerformanceExportSubMenu_Tooltip", "Export processed MetaHuman performances"),
			FNewToolMenuDelegate::CreateStatic(&MetaHumanBatchMenuExtension::FillPerformanceExportSubMenu),
			false,
			FSlateIcon(TEXT("MetaHumanPerformanceStyle"), TEXT("ClassIcon.MetaHumanPerformance"), TEXT("ClassIcon.MetaHumanPerformance")));
	}));
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditor.h"

#include "AudioDevice.h"
#include "AudioWidgetsStyle.h"
#include "AssetDefinitionRegistry.h"
#include "Components/AudioComponent.h"
#include "EditorReimportHandler.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/TransactionObjectEvent.h"
#include "PropertyEditorModule.h"
#include "Sound/SoundWave.h"
#include "SparseSampledSequenceTransportCoordinator.h"
#include "STransformedWaveformViewPanel.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "SWaveformTransformationsOverlay.h"
#include "ToolMenus.h"
#include "TransformedWaveformView.h"
#include "TransformedWaveformViewFactory.h"
#include "WaveformEditorCommands.h"
#include "WaveformEditorDetailsCustomization.h"
#include "WaveformEditorLog.h"
#include "WaveformEditorModule.h"
#include "WaveformEditorSequenceDataProvider.h"
#include "WaveformEditorStyle.h"
#include "WaveformEditorToolMenuContext.h"
#include "WaveformEditorTransformationsSettings.h"
#include "WaveformEditorWaveWriter.h"
#include "WaveformTransformationMarkers.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "WaveformEditor"

const FName FWaveformEditor::AppIdentifier("WaveformEditorApp");
const FName FWaveformEditor::PropertiesTabId("WaveformEditor_Properties");
const FName FWaveformEditor::TransformationsTabId("WaveformEditor_Transformations");
const FName FWaveformEditor::WaveformDisplayTabId("WaveformEditor_Display");
const FName FWaveformEditor::EditorName("Waveform Editor");
const FName FWaveformEditor::ToolkitFName("WaveformEditor");

bool FWaveformEditor::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USoundWave* SoundWaveToEdit)
{
	checkf(SoundWaveToEdit, TEXT("Tried to open a Soundwave Editor from a null soundwave"));

	const TSharedRef<FTabManager::FLayout>  StandaloneDefaultLayout = SetupStandaloneLayout();

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	const bool bToolbarFocusable = false;
	const bool bUseSmallIcons = true;

	SoundWave = SoundWaveToEdit;

	// Initialize TransformationChainConfig from persisted transformations or if there are none falling back to default transformations.
	if (SoundWave->Transformations.Num() == 0)
	{
		AddDefaultTransformations();
	}
	else
	{
		TransformationChainConfig = SoundWave->UpdateTransformations();
	}

	bool bIsInitialized = true;
	
	bIsInitialized &= CreateDetailsViews();
	bIsInitialized &= CreateTransportCoordinator();
	bIsInitialized &= InitializeZoom();
	bIsInitialized &= CreateWaveformView();
	bIsInitialized &= InitializeAudioComponent();
	bIsInitialized &= CreateTransportController();
	bIsInitialized &= CreateWaveWriter();
	bIsInitialized &= BindDelegates();
	bIsInitialized &= SetUpAssetReimport();

	bIsInitialized &= RegisterToolbar();
	bIsInitialized &= BindCommands();

	GEditor->RegisterForUndo(this);

	if (bIsInitialized)
	{
		FAssetEditorToolkit::InitAssetEditor(
			Mode,
			InitToolkitHost,
			AppIdentifier,
			StandaloneDefaultLayout,
			bCreateDefaultStandaloneMenu,
			bCreateDefaultToolbar,
			SoundWaveToEdit,
			bToolbarFocusable,
			bUseSmallIcons);

			// Initialize waveform view to with playhead at start of active transformations.
			check(WaveformView.DataProvider != nullptr);
			WaveformView.DataProvider->GenerateLayersChain();
			WaveformView.DataProvider->UpdateRenderElements();
			PlaybackTimeBeforeTransformInteraction = 0;
			StartTimeBeforeTransformInteraction = TransformationChainConfig.StartTime; // StartTime=0, EndTime=-1 if no transformations
			check(TransportController != nullptr);
			TransportController->CacheStartTime(PlaybackTimeBeforeTransformInteraction);
			check(TransportCoordinator != nullptr);
			TransportCoordinator->SetProgressRatio(0.0f);

			OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddLambda([this, SoundWaveToEdit](const FString&, bool)
				{
					CloseWindow(EAssetEditorCloseReason::EditorRefreshRequested);

					FNotificationInfo Info(FText::FromString(TEXT("Do you want to reopen the sound wave for editing?")));
					Info.bFireAndForget = true;
					Info.ExpireDuration = 15.0f;

					Info.ButtonDetails.Add(FNotificationButtonInfo(
						FText::FromString(TEXT("Yes")),
						FText::FromString(TEXT("Open the SoundWave asset")),
						FSimpleDelegate::CreateLambda([this, SoundWaveToEdit]() 
							{
								bool bSuccess = false;
								if (GEditor && SoundWaveToEdit)
								{
									TArray<UObject*> AssetsToOpen = { SoundWaveToEdit };
									if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
									{
										bSuccess = AssetEditorSubsystem->OpenEditorForAssets(AssetsToOpen);
									}
								}
								if (ReopenNotificationItem.IsValid())
								{
									if (bSuccess)
									{
										ReopenNotificationItem->SetCompletionState(SNotificationItem::CS_Success);
									}
									else
									{
										ReopenNotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
									}
									ReopenNotificationItem->Fadeout();
								}
							})
					));

					Info.ButtonDetails.Add(FNotificationButtonInfo(
						FText::FromString(TEXT("No")),
						FText::FromString(TEXT("Dismiss this message")),
						FSimpleDelegate::CreateLambda([this]()
							{
								if (ReopenNotificationItem.IsValid())
								{
									ReopenNotificationItem->SetCompletionState(SNotificationItem::CS_None);
									ReopenNotificationItem->Fadeout();
								}
							})
					));

					ReopenNotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
					
					if (ReopenNotificationItem.IsValid())
					{
						ReopenNotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
					}
				});
	}

	return bIsInitialized;
}

FWaveformEditor::~FWaveformEditor()
{
	if (OnMapOpenedHandle.IsValid())
	{
		FEditorDelegates::OnMapOpened.Remove(OnMapOpenedHandle);
		OnMapOpenedHandle.Reset();
	}

	if (FReimportManager::Instance())
	{
		FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
	}

	TObjectPtr<UWaveformTransformationMarkers> MarkerTransformation = GetMarkerTransformation();

	if (MarkerTransformation != nullptr)
	{
		if (bCueChangeRegisteredByWaveformEditor)
		{
			check(MarkerTransformation->Markers);
			MarkerTransformation->Markers->CueChanged.Unbind();
		}

		if (TransformationChainConfig.bIsPreviewingLoopRegion)
		{
			// Note: if the user doesn't save the soundwave asset, bLooping will not be reset.
			check(SoundWave);
			SoundWave->Modify();
			SoundWave->bLooping = TransformationChainConfig.bCachedSoundWaveLoopState;

			MarkerTransformation->ResetLoopPreviewing();
			SoundWave->UpdateTransformations();

			SoundWave->PostEditChange();
		}
	}
}

void FWaveformEditor::AddDefaultTransformations()
{
	if (!ensure(SoundWave))
	{
		return;
	}

	if (SoundWave->Transformations.Num() == 0)
	{
		const UWaveformEditorTransformationsSettings* TransformationsSettings = GetWaveformEditorTransformationsSettings();
		for (const TSubclassOf<UWaveformTransformationBase>& TransformationClass : TransformationsSettings->LaunchTransformations)
		{
			//Adding default transformations does not mark the SoundWave to be saved
			//Modify marks it as dirty to inform the user it needs to be saved if they want the default transformations to remain.
			SoundWave->Modify();
			
			if (TransformationClass)
			{
				EObjectFlags MaskedOuterFlags = SoundWave ? SoundWave->GetMaskedFlags(RF_PropagateToSubObjects) : RF_NoFlags;

				if (SoundWave->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
				{
					MaskedOuterFlags |= RF_ArchetypeObject;
				}

				UWaveformTransformationBase* TransformationToAdd = NewObject<UWaveformTransformationBase>(SoundWave, TransformationClass.Get(), NAME_None, MaskedOuterFlags);
				SoundWave->Transformations.Add(TransformationToAdd);
			}
			else
			{
				SoundWave->Transformations.Add(nullptr);
			}

			//Update the content browser asset state to dirty
			SoundWave->PostEditChange();
		}

		TransformationChainConfig = SoundWave->UpdateTransformations();
	}
}

void FWaveformEditor::NotifyPostTransformationChange(const EPropertyChangeType::Type& PropertyChangeType)
{
	check(SoundWave);

	FProperty* TransformationsProperty = FindFProperty<FProperty>(USoundWave::StaticClass(), GET_MEMBER_NAME_CHECKED(USoundWave, Transformations));

	FEditPropertyChain PropertyChain;
	PropertyChain.AddHead(TransformationsProperty);
	PropertyChain.SetActivePropertyNode(TransformationsProperty);

	FPropertyChangedEvent PropertyChangedEvent(TransformationsProperty, PropertyChangeType);

	NotifyPostChange(PropertyChangedEvent, &PropertyChain);

	//Update the content browser asset state to dirty
	SoundWave->PostEditChange();

	TransformationChainConfig = SoundWave->UpdateTransformations();
}

TObjectPtr<UWaveformTransformationTrimFade> FWaveformEditor::GetOrAddTrimFadeTransformation()
{
	check(SoundWave);

	TObjectPtr<UWaveformTransformationTrimFade> TrimFadeTransformation;

	for (TObjectPtr<UWaveformTransformationBase> Transformation : SoundWave->Transformations)
	{
		TrimFadeTransformation = Cast<UWaveformTransformationTrimFade>(Transformation);
		if (TrimFadeTransformation)
		{
			break;
		}
	}

	if (TrimFadeTransformation == nullptr)
	{
		EObjectFlags MaskedOuterFlags = SoundWave ? SoundWave->GetMaskedFlags(RF_PropagateToSubObjects) : RF_NoFlags;

		if (SoundWave->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			MaskedOuterFlags |= RF_ArchetypeObject;
		}

		TrimFadeTransformation = NewObject<UWaveformTransformationTrimFade>(SoundWave, UWaveformTransformationTrimFade::StaticClass(), NAME_None, MaskedOuterFlags);

		SoundWave->Modify();
		SoundWave->Transformations.Add(TrimFadeTransformation);

		NotifyPostTransformationChange();
	}
	return TrimFadeTransformation;
}

TObjectPtr<UWaveformTransformationMarkers> FWaveformEditor::GetOrAddMarkerTransformation()
{
	check(SoundWave);

	TObjectPtr<UWaveformTransformationMarkers> MarkersTransformation = GetMarkerTransformation();

	if (MarkersTransformation == nullptr)
	{
		MarkersTransformation = AddMarkerTransformation();
	}

	return MarkersTransformation;
}

TObjectPtr<UWaveformTransformationMarkers> FWaveformEditor::GetMarkerTransformation()
{
	if (SoundWave == nullptr)
	{
		return nullptr;
	}

	TObjectPtr<UWaveformTransformationMarkers> MarkersTransformation;

	for (TObjectPtr<UWaveformTransformationBase> Transformation : SoundWave->Transformations)
	{
		MarkersTransformation = Cast<UWaveformTransformationMarkers>(Transformation);
		if (MarkersTransformation)
		{
			return MarkersTransformation;
		}
	}

	return nullptr;
}

TObjectPtr<UWaveformTransformationMarkers> FWaveformEditor::AddMarkerTransformation()
{
	TObjectPtr<UWaveformTransformationMarkers> MarkersTransformation;
	EObjectFlags MaskedOuterFlags = SoundWave ? SoundWave->GetMaskedFlags(RF_PropagateToSubObjects) : RF_NoFlags;

	if (SoundWave->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		MaskedOuterFlags |= RF_ArchetypeObject;
	}

	MarkersTransformation = NewObject<UWaveformTransformationMarkers>(SoundWave, UWaveformTransformationMarkers::StaticClass(), NAME_None, MaskedOuterFlags);

	SoundWave->Modify();
	SoundWave->Transformations.Add(MarkersTransformation);

	NotifyPostTransformationChange();

	return MarkersTransformation;
}

void FWaveformEditor::ToggleFadeIn()
{
	check(SoundWave);
	check(GEditor != nullptr);
	GEditor->BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("SetNumericPropertyTransactionToggleFadeIn", "ToggleFadeIn"), nullptr);
	SoundWave->Modify();

	TObjectPtr<UWaveformTransformationTrimFade> TrimFadeTransformation = GetOrAddTrimFadeTransformation();
	check(TrimFadeTransformation);

	TrimFadeTransformation->Modify();
	
	//if fade duration is 0, assume there is no fade present and toggle on the fade with the curve data selected in the menu
	if (TrimFadeTransformation->StartFadeTime == 0.0f)
	{
		check(DefaultFadeInAmount >= 0.0f);
		check(FadeInCurve >= -0.1f && FadeInCurve <= 10);

		if (CachedFadeInAmount <= 0.0f)
		{
			CachedFadeInAmount = DefaultFadeInAmount;
		}

		TrimFadeTransformation->StartFadeTime = CachedFadeInAmount;
		TrimFadeTransformation->StartFadeCurve = FadeInCurve;
	}
	else //if fade duration is not 0, assume there is a fade applied and toggle fade "off" by setting fade time to 0
	{
		CachedFadeInAmount = TrimFadeTransformation->StartFadeTime;

		//No need to set FadeCurve when toggling off. This will preserve user curve data if they later manually change the StartFadeTime.
		TrimFadeTransformation->StartFadeTime = 0.0f;
	}

	NotifyPostTransformationChange(EPropertyChangeType::ValueSet);

	GEditor->EndTransaction();
}

bool FWaveformEditor::CanFadeIn()
{
	return true;
}

void FWaveformEditor::ToggleFadeOut()
{
	check(SoundWave);
	check(GEditor != nullptr);
	GEditor->BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("SetNumericPropertyTransactionToggleFadeOut", "ToggleFadeOut"), nullptr);
	SoundWave->Modify();

	TObjectPtr<UWaveformTransformationTrimFade> TrimFadeTransformation = GetOrAddTrimFadeTransformation();
	check(TrimFadeTransformation);

	TrimFadeTransformation->Modify();

	//if fade duration is 0, assume there is no fade present and toggle on the fade with the curve data selected in the menu
	if (TrimFadeTransformation->EndFadeTime == 0.0f)
	{
		check(DefaultFadeOutAmount >= 0.0f);
		check(FadeOutCurve >= -0.1f && FadeOutCurve <= 10);

		if (CachedFadeOutAmount <= 0.0f)
		{
			CachedFadeOutAmount = DefaultFadeOutAmount;
		}
		
		TrimFadeTransformation->EndFadeTime = CachedFadeOutAmount;
		TrimFadeTransformation->EndFadeCurve = FadeOutCurve;
	}
	else //if fade duration is not 0, assume there is a fade applied and toggle fade "off" by setting fade time to 0
	{
		CachedFadeOutAmount = TrimFadeTransformation->EndFadeTime;

		//No need to set FadeCurve when toggling off. This will preserve user curve data if they later manually change the EndFadeTime.
		TrimFadeTransformation->EndFadeTime = 0.0f;
	}

	NotifyPostTransformationChange(EPropertyChangeType::ValueSet);
	GEditor->EndTransaction();
}

bool FWaveformEditor::CanFadeOut()
{
	return true;
}

void FWaveformEditor::CreateMarker(bool bIsLoopRegion)
{
	check(SoundWave);
	check(GEditor != nullptr);
	GEditor->BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("SetNumericPropertyTransactionCreateMarker", "CreateMarker"), nullptr);
	SoundWave->Modify();

	TObjectPtr<UWaveformTransformationMarkers> MarkerTransformation = GetOrAddMarkerTransformation();
	check(MarkerTransformation);

	int32 HighestCueID = INDEX_NONE;

	MarkerTransformation->Markers->Modify();

	for (FSoundWaveCuePoint CuePoint : MarkerTransformation->Markers->CuesAndLoops)
	{
		if (CuePoint.CuePointID > HighestCueID)
		{
			HighestCueID = CuePoint.CuePointID;
		}
	}

	FSoundWaveCuePoint NewCuePoint;
	NewCuePoint.CuePointID = HighestCueID + 1;
	NewCuePoint.FramePosition = static_cast<int64>(TransportCoordinator->GetFocusPoint() * SoundWave->TotalSamples);

	if (bIsLoopRegion)
	{
		NewCuePoint.SetLoopRegion(true);
		NewCuePoint.FrameLength = static_cast<int64>(SoundWave->TotalSamples * 0.1f); // Default loop region to 10% of samples for easier tuning
	}

	MarkerTransformation->Markers->CuesAndLoops.Add(NewCuePoint);
	NotifyPostTransformationChange(EPropertyChangeType::ValueSet);
	GEditor->EndTransaction();
}

void FWaveformEditor::DeleteMarker()
{
	check(SoundWave);
	check(GEditor != nullptr);
	GEditor->BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("SetNumericPropertyTransactionDeleteMarker", "DeleteMarker"), nullptr);
	SoundWave->Modify();

	TObjectPtr<UWaveformTransformationMarkers> MarkerTransformation = GetOrAddMarkerTransformation();
	check(MarkerTransformation);
	check(MarkerTransformation->Markers);

	int32 CueToDelete = MarkerTransformation->Markers->SelectedCue;

	MarkerTransformation->Markers->Modify();

	if (CueToDelete == INDEX_NONE)
	{
		GEditor->EndTransaction();
		return; // No cue to selected
	}

	for (int i = 0; i < MarkerTransformation->Markers->CuesAndLoops.Num(); i++)
	{
		if (CueToDelete == MarkerTransformation->Markers->CuesAndLoops[i].CuePointID)
		{
			if (MarkerTransformation->Markers->CuesAndLoops[i].IsLoopRegion())
			{
				MarkerTransformation->ResetLoopPreviewing();
			}

			MarkerTransformation->Markers->CuesAndLoops.RemoveAt(i);
			MarkerTransformation->Markers->SelectedCue = INDEX_NONE;

			break;
		}
	}

	NotifyPostTransformationChange(EPropertyChangeType::ValueSet);

	GEditor->EndTransaction();
}

void FWaveformEditor::SkipToNextMarker()
{
	check(SoundWave != nullptr);

	if (TObjectPtr<UWaveformTransformationMarkers> MarkerTransformation = GetOrAddMarkerTransformation())
	{
		check(TransportCoordinator != nullptr);
		check(SoundWave->TotalSamples > 0);
		const int64 PlaybackFramePosition = static_cast<int64>(TransportCoordinator->GetFocusPoint() * SoundWave->TotalSamples);
		int64 NewPlaybackFramePosition = MAX_int64;

		// Find the nearest marker beyond the current playhead.
		check(MarkerTransformation->Markers != nullptr);
		for (int i = 0; i < MarkerTransformation->Markers->CuesAndLoops.Num(); i++)
		{
			if (MarkerTransformation->Markers->CuesAndLoops[i].FramePosition > PlaybackFramePosition + 1)
			{
				if (MarkerTransformation->Markers->CuesAndLoops[i].FramePosition < NewPlaybackFramePosition)
				{
					NewPlaybackFramePosition = MarkerTransformation->Markers->CuesAndLoops[i].FramePosition;
				}
			}
		}

		if (NewPlaybackFramePosition != MAX_int64)
		{
			float ActiveDuration = TransformationChainConfig.EndTime - TransformationChainConfig.StartTime;
			if (ActiveDuration <= 0.0f)
			{
				// If no active+initialized transformation, ActiveDuration will be <= 0.0f, so fallback to sound duration:
				ActiveDuration = SoundWave->Duration; // not GetDuration() to get the raw duration and not INDEFINITELY_LOOPING_DURATION if looping
			}
			if (ActiveDuration > 0)
			{
				check(TransformationChainConfig.SampleRate > 0);
				const float ActiveDurationInFrames = ActiveDuration * TransformationChainConfig.SampleRate;
				const float AdjustedNewPlaybackFramePosition = FMath::Max(NewPlaybackFramePosition - TransformationChainConfig.StartTime * TransformationChainConfig.SampleRate, 0.0f);
				
				HandlePlayheadScrub(AdjustedNewPlaybackFramePosition / ActiveDurationInFrames, false);
				TransportCoordinator->SetProgressRatio(AdjustedNewPlaybackFramePosition / ActiveDurationInFrames);
			}
		}
	}
}

void FWaveformEditor::RegenerateTransformations()
{
	WaveformView.DataProvider->GenerateLayersChain();
	WaveformView.DataProvider->UpdateRenderElements();
	TransformationChainConfig = SoundWave->GetTransformationChainConfig();

	UpdateTransportState();
}

void FWaveformEditor::ModifyMarkerLoopRegion(ELoopModificationControls Modification)
{
	check(SoundWave);
	TObjectPtr<UWaveformTransformationMarkers> MarkersTransformation = GetOrAddMarkerTransformation();
	check(MarkersTransformation);

	MarkersTransformation->ModifyMarkerLoopRegion(Modification);
}

void FWaveformEditor::CycleMarkerLoopRegion(ELoopModificationControls Modification)
{
	check(SoundWave);
	TObjectPtr<UWaveformTransformationMarkers> MarkersTransformation = GetOrAddMarkerTransformation();
	check(MarkersTransformation);

	MarkersTransformation->CycleMarkerLoopRegion(Modification);
}

bool FWaveformEditor::InitializeAudioComponent()
{
	if (!ensure(SoundWave))
	{
		return false;
	}

	if (AudioComponent == nullptr)
	{
		if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
		{
			if (FAudioDevice* AudioDevice = AudioDeviceManager->GetMainAudioDeviceRaw())
			{
				USoundBase* SoundBase = Cast<USoundBase>(SoundWave);
				AudioComponent = FAudioDevice::CreateComponent(SoundBase);
			}
		}

		if (AudioComponent == nullptr)
		{
			return false;
		}
	}

	AudioComponent->bAutoDestroy = false;
	AudioComponent->bIsUISound = true;
	AudioComponent->bAllowSpatialization = false;
	AudioComponent->bReverb = false;
	AudioComponent->bCenterChannelOnly = false;
	AudioComponent->bIsPreviewSound = true;

	return true;
}

bool FWaveformEditor::CreateTransportController()
{
	if (!ensure(AudioComponent))
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Trying to setup transport controls with a null audio component"));
		return false;
	}

	TransportController = MakeShared<FWaveformEditorTransportController>(AudioComponent);
	return TransportController != nullptr;
}

bool FWaveformEditor::InitializeZoom()
{
	ZoomManager = MakeShared<FWaveformEditorZoomController>();

	check(TransportCoordinator)
	ZoomManager->OnZoomRatioChanged.AddSP(TransportCoordinator.ToSharedRef(), &FSparseSampledSequenceTransportCoordinator::SetZoomRatio);

	return ZoomManager != nullptr;
}

bool FWaveformEditor::BindDelegates()
{
	if (!ensure(AudioComponent))
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Failed to bind to playback percentage change, audio component is null"));
		return false;
	}

	AudioComponent->OnAudioPlaybackPercentNative.AddSP(this, &FWaveformEditor::HandlePlaybackPercentageChange);
	AudioComponent->OnAudioPlayStateChangedNative.AddSP(this, &FWaveformEditor::HandleAudioComponentPlayStateChanged);
	TransportCoordinator->OnFocusPointScrubUpdate.AddSP(this, &FWaveformEditor::HandlePlayheadScrub);
	return true;
}

bool FWaveformEditor::SetUpAssetReimport()
{
	if (!FReimportManager::Instance())
	{
		return false;
	}

	FReimportManager::Instance()->OnPostReimport().AddSP(this, &FWaveformEditor::OnAssetReimport);
	return true;
}

void FWaveformEditor::ExecuteReimport()
{
	if (!CanExecuteReimport())
	{
		return;
	}
	
	if (ReimportMode == EWaveEditorReimportMode::SameFileOverwrite)
	{
		ExecuteOverwriteTransformations();
	}

	const bool bSelectNewAsset = ReimportMode == EWaveEditorReimportMode::SelectFile;

	TArray<UObject*> CopyOfSelectedAssets;
	CopyOfSelectedAssets.Add(SoundWave);
	FReimportManager::Instance()->ValidateAllSourceFileAndReimport(CopyOfSelectedAssets, true, -1, bSelectNewAsset);
}

void FWaveformEditor::ExecuteOverwriteTransformations()
{
	for (TObjectPtr<UWaveformTransformationBase> Transformation : SoundWave->Transformations)
	{
		Transformation->OverwriteTransformation();
	}
}

void FWaveformEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	if (TransformationsDetails == nullptr)
	{
		return;
	}

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_WaveformEditor", "Sound Wave Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FWaveformEditor::SpawnTab_Properties))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(TransformationsTabId, FOnSpawnTab::CreateSP(this, &FWaveformEditor::SpawnTab_Transformations))
		.SetDisplayName(LOCTEXT("ProcessingTab", "Processing"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(WaveformDisplayTabId, FOnSpawnTab::CreateSP(this, &FWaveformEditor::SpawnTab_WaveformDisplay))
		.SetDisplayName(LOCTEXT("WaveformDisplayTab", "WaveformDisplay"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

}

void FWaveformEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(PropertiesTabId);
	InTabManager->UnregisterTabSpawner(WaveformDisplayTabId);
}

bool FWaveformEditor::RegisterToolbar()
{
	const FName MenuName = FAssetEditorToolkit::GetToolMenuToolbarName();

	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		const FWaveformEditorCommands& Commands = FWaveformEditorCommands::Get();
		UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(MenuName, "AssetEditor.DefaultToolBar", EMultiBoxType::ToolBar);

		if (ToolBar == nullptr)
		{
			return false;
		}

		FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);
		FToolMenuSection& PlayBackSection = ToolBar->AddSection("Transport Controls", TAttribute<FText>(), InsertAfterAssetSection);

		FToolMenuEntry PlayEntry = FToolMenuEntry::InitToolBarButton(
			Commands.PlaySoundWave,
			FText(),
			LOCTEXT("WaveformEditorPlayButtonTooltip", "Plays this SoundWave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PlayInViewport")
		);

		PlayEntry.StyleNameOverride = FName("Toolbar.BackplateLeftPlay");
		
		FToolMenuEntry PauseEntry = FToolMenuEntry::InitToolBarButton(
			Commands.PauseSoundWave,
			FText(),
			LOCTEXT("WaveformEditorPauseButtonTooltip", "Pauses this SoundWave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PausePlaySession.Small")
		);

		PauseEntry.StyleNameOverride = FName("Toolbar.BackplateCenter");

		FToolMenuEntry StopEntry = FToolMenuEntry::InitToolBarButton(
			Commands.StopSoundWave,
			FText(),
			LOCTEXT("WaveformEditorStopButtonTooltip", "Stops this SoundWave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.StopPlaySession.Small")
		);

		StopEntry.StyleNameOverride = FName("Toolbar.BackplateRight");

		PlayBackSection.AddEntry(PlayEntry);
		PlayBackSection.AddEntry(PauseEntry);
		PlayBackSection.AddEntry(StopEntry);


		FToolMenuInsert InsertAfterPlaybackSection("Transport Controls", EToolMenuInsertType::After);
		FToolMenuSection& ZoomSection = ToolBar->AddSection("Zoom Controls", TAttribute<FText>(), InsertAfterPlaybackSection);

		FToolMenuEntry ZoomInEntry = FToolMenuEntry::InitToolBarButton(
			Commands.ZoomIn,
			FText(),
			LOCTEXT("WaveformEditorZoomInButtonTooltip", "Zooms into the soundwave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
		);

		FToolMenuEntry ZoomOutEntry = FToolMenuEntry::InitToolBarButton(
			Commands.ZoomOut,
			FText(),
			LOCTEXT("WaveformEditorZoomOutButtonTooltip", "Zooms out the soundwave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Minus")
		);

		ZoomSection.AddEntry(ZoomInEntry);
		ZoomSection.AddEntry(ZoomOutEntry);

		FToolMenuInsert InsertAfterZoomSection("Zoom Controls", EToolMenuInsertType::After);
		FToolMenuSection& ExportSection = ToolBar->AddSection("Export Controls", TAttribute<FText>(), InsertAfterZoomSection);


		ExportSection.AddDynamicEntry("ExportButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection) 
		{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

			if (TSharedPtr<FWaveformEditor> LockedObserver = Context->WaveformEditor.Pin())
			{
				FToolMenuEntry ExportEntry = FToolMenuEntry::InitToolBarButton(
					Commands.ExportWaveform,
					FText(),
					TAttribute< FText >::CreateRaw(LockedObserver.Get(), &FWaveformEditor::GetExportButtonToolTip),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll")
				);

				InSection.AddEntry(ExportEntry);
			}
		}));

		ExportSection.AddDynamicEntry("ExportOptions", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection) 
		{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();
				
				if (TSharedPtr<FWaveformEditor> LockedObserver = Context->WaveformEditor.Pin())
				{
					FToolMenuEntry ExportOptionsEntry = FToolMenuEntry::InitComboButton(
						"ExportsOptionsCombo",
						FToolUIActionChoice(FUIAction()),
						FNewToolMenuChoice(FOnGetContent::CreateSP(LockedObserver.Get(), &FWaveformEditor::GenerateExportOptionsMenu)),
						LOCTEXT("ExportsOptions_Label", "Export Options"),
						LOCTEXT("ExportsOptions_ToolTip", "Export options for this waveform"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll"),
						true
					);

					InSection.AddEntry(ExportOptionsEntry);
				}				
		}));

		ExportSection.AddDynamicEntry("ImportButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection)
		{
			const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

			if (TSharedPtr<FWaveformEditor> LockedObserver = Context->WaveformEditor.Pin())
			{
				FToolMenuEntry ReimportEntry = FToolMenuEntry::InitToolBarButton(
					Commands.ReimportAsset,
					FText(),
					TAttribute< FText >::CreateRaw(LockedObserver.Get(), &FWaveformEditor::GetReimportButtonToolTip),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Reimport"));

				InSection.AddEntry(ReimportEntry);
			}
		}));

		ExportSection.AddDynamicEntry("ReimportOptions", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection) 
		{
			const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

			if (TSharedPtr<FWaveformEditor> LockedObserver = Context->WaveformEditor.Pin())
			{
				FToolMenuEntry ReimportOptionsEntry = FToolMenuEntry::InitComboButton(
					"ReimportOptionsCombo",
					FToolUIActionChoice(FUIAction()),
					FNewToolMenuChoice(FOnGetContent::CreateSP(LockedObserver.Get(), &FWaveformEditor::GenerateImportOptionsMenu)),
					LOCTEXT("ReimportOptions_Label", "Reimport Options"),
					LOCTEXT("ReimportOptions_ToolTip", "Reimport options for this USoundWave"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Reimport"),
					true
				);

				InSection.AddEntry(ReimportOptionsEntry);
			}

		}));

		FToolMenuInsert InsertAfterExportSection("Export Controls", EToolMenuInsertType::After);
		FToolMenuSection& TransformationSection = ToolBar->AddSection("Transformation Controls", TAttribute<FText>(), InsertAfterExportSection);

		TransformationSection.AddDynamicEntry("ToggleFadeInButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection)
			{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

				if (Context && Context->WaveformEditor.IsValid())
				{
					FToolMenuEntry ToggleFadeIn = FToolMenuEntry::InitToolBarButton(
						Commands.ToggleFadeIn,
						FText(),
						LOCTEXT("WaveformEditorFadeInButtonTooltip", "Toggle Fade In Transformation onto the soundwave"),
						FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeIn")
					);
					InSection.AddEntry(ToggleFadeIn);
				}
			}));

		TransformationSection.AddDynamicEntry("FadeInOptions", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
			{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

				if (TSharedPtr<FWaveformEditor> LockedObserver = Context->WaveformEditor.Pin())
				{
					FToolMenuEntry ExportOptionsEntry = FToolMenuEntry::InitComboButton(
						"FadeInOptionsCombo",
						FToolUIActionChoice(FUIAction()),
						FNewToolMenuChoice(FOnGetContent::CreateSP(LockedObserver.Get(), &FWaveformEditor::GenerateFadeInOptionsMenu)),
						LOCTEXT("FadeInOptions_Label", "Fade In Options"),
						LOCTEXT("FadeInOptions_ToolTip", "Fade In options for this waveform. Applied when Toggle Fade In is pressed"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll"),
						true
					);

					InSection.AddEntry(ExportOptionsEntry);
				}
			}));

		TransformationSection.AddDynamicEntry("ToggleFadeOutButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection)
			{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

				if (Context && Context->WaveformEditor.IsValid())
				{
					FToolMenuEntry ToggleFadeOut = FToolMenuEntry::InitToolBarButton(
						Commands.ToggleFadeOut,
						FText(),
						LOCTEXT("WaveformEditorFadeOutButtonTooltip", "Toggle Fade Out Transformation onto the soundwave"),
						FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeOut")
					);
					InSection.AddEntry(ToggleFadeOut);
				}
			}));

		TransformationSection.AddDynamicEntry("FadeOutOptions", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
			{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

				if (TSharedPtr<FWaveformEditor> LockedObserver = Context->WaveformEditor.Pin())
				{
					FToolMenuEntry ExportOptionsEntry = FToolMenuEntry::InitComboButton(
						"FadeOutOptionsCombo",
						FToolUIActionChoice(FUIAction()),
						FNewToolMenuChoice(FOnGetContent::CreateSP(LockedObserver.Get(), &FWaveformEditor::GenerateFadeOutOptionsMenu)),
						LOCTEXT("FadeOutOptions_Label", "Fade Out Options"),
						LOCTEXT("FadeOutOptions_ToolTip", "Fade Out options for this waveform. Applied when Toggle Fade Out is pressed"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll"),
						true
					);

					InSection.AddEntry(ExportOptionsEntry);
				}
			}));

		TransformationSection.AddDynamicEntry("CreateMarkerButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection)
			{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

				if (Context && Context->WaveformEditor.IsValid())
				{
					FToolMenuEntry CreateMarker = FToolMenuEntry::InitToolBarButton(
						Commands.CreateMarker,
						FText(),
						LOCTEXT("WaveformEditorCreateMarkerButtonTooltip", "Create a marker cue for the soundwave"),
						FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.MarkerAdd")
					);
					InSection.AddEntry(CreateMarker);
				}
			}));


		TransformationSection.AddDynamicEntry("CreateLoopRegionButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection)
			{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

				if (Context && Context->WaveformEditor.IsValid())
				{
					FToolMenuEntry CreateLoopRegion = FToolMenuEntry::InitToolBarButton(
						Commands.CreateLoopRegion,
						FText(),
						LOCTEXT("WaveformEditorCreateLoopRegionButtonTooltip", "Create a marker loop region for the soundwave"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Transport.Looping")
					);
					InSection.AddEntry(CreateLoopRegion);
				}
			}));

		TransformationSection.AddDynamicEntry("DeleteMarkerButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection)
			{
				const UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

				if (Context && Context->WaveformEditor.IsValid())
				{
					FToolMenuEntry DeleteLoopRegion = FToolMenuEntry::InitToolBarButton(
						Commands.DeleteMarker,
						FText(),
						LOCTEXT("WaveformEditorDeleteMarkerButtonTooltip", "Delete a marker cue or loop region for the soundwave"),
						FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.MarkerDelete")
					);
					InSection.AddEntry(DeleteLoopRegion);
				}
			}));
	}

	return true;
}

TSharedRef<SWidget> FWaveformEditor::GenerateFadeInOptionsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("FadeInOptionsSection_Label", "Fade In Options"));
	{
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeInLinear, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeInLinear"));
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeInExponential, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeInExponential"));
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeInLogarithmic, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeInLogarithmic"));
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeInSigmoid, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeInSigmoid"));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FWaveformEditor::GenerateFadeOutOptionsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("FadeOutOptionsSection_Label", "Fade Out Options"));
	{
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeOutLinear, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeOutLinear"));
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeOutExponential, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeOutExponential"));
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeOutLogarithmic, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeOutLogarithmic"));
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().FadeOutSigmoid, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAudioWidgetsStyle::StyleName, "AudioWidgetsStyle.FadeOutSigmoid"));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FWaveformEditor::GenerateExportOptionsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());
	
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ChannelSection_Label", "Export Channel Format"));
	{
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ExportFormatMono);
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ExportFormatStereo);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FWaveformEditor::GenerateImportOptionsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ReimportMode_Label", "Reimport Mode"));
	{
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ReimportModeSameFile);
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ReimportModeSameFileOverwriteTransformations);
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ReimportModeNewFile);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool FWaveformEditor::CanExecuteReimport() const
{
	if (FReimportManager::Instance() == nullptr)
	{
		return false;
	}

	return FReimportManager::Instance()->CanReimport(SoundWave);
}

bool FWaveformEditor::BindCommands()
{
	const FWaveformEditorCommands& Commands = FWaveformEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.PlaySoundWave,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::Play),
		FCanExecuteAction::CreateSP(this, &FWaveformEditor::CanPressPlayButton));

	ToolkitCommands->MapAction(
		Commands.StopSoundWave,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::Stop),
		FCanExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::CanStop));

	ToolkitCommands->MapAction(
		Commands.TogglePlayback,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::TogglePlayback));

	ToolkitCommands->MapAction(
		Commands.PauseSoundWave,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::Pause),
		FCanExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::IsPlaying));

	ToolkitCommands->MapAction(
		Commands.ZoomIn,
		FExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::ZoomIn),
		FCanExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::CanZoomIn));

	ToolkitCommands->MapAction(
		Commands.ZoomOut,
		FExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::ZoomOut),
		FCanExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::CanZoomOut));

	ToolkitCommands->MapAction(
		Commands.ExportWaveform,
		FExecuteAction::CreateSP(this, &FWaveformEditor::ExportWaveform),
		FCanExecuteAction::CreateSP(WaveWriter.ToSharedRef(), &FWaveformEditorWaveWriter::CanCreateSoundWaveAsset));

	ToolkitCommands->MapAction(
		Commands.ExportFormatMono,
		FExecuteAction::CreateLambda([this] { (WaveWriter->SetExportChannelsFormat(WaveformEditorWaveWriter::EChannelFormat::Mono)); }),
		FCanExecuteAction::CreateLambda([this] {return WaveWriter.IsValid(); }),
		FIsActionChecked::CreateLambda([this] {return WaveWriter->GetExportChannelsFormat() == WaveformEditorWaveWriter::EChannelFormat::Mono; }));

	ToolkitCommands->MapAction(
		Commands.ExportFormatStereo,
		FExecuteAction::CreateLambda([this] { (WaveWriter->SetExportChannelsFormat(WaveformEditorWaveWriter::EChannelFormat::Stereo)); }),
		FCanExecuteAction::CreateLambda([this] {return WaveWriter.IsValid(); }),
		FIsActionChecked::CreateLambda([this] {return WaveWriter->GetExportChannelsFormat() == WaveformEditorWaveWriter::EChannelFormat::Stereo; }));

	ToolkitCommands->MapAction(
		Commands.ReimportAsset,
		FExecuteAction::CreateSP(this, &FWaveformEditor::ExecuteReimport),
		FCanExecuteAction::CreateSP(this, &FWaveformEditor::CanExecuteReimport));

	ToolkitCommands->MapAction(
		Commands.ReimportModeSameFile,
		FExecuteAction::CreateLambda([this] { ReimportMode = EWaveEditorReimportMode::SameFile; }),
		FCanExecuteAction::CreateLambda([this] { return CanExecuteReimport(); }),
		FIsActionChecked::CreateLambda([this] {return ReimportMode == EWaveEditorReimportMode::SameFile; }));

	ToolkitCommands->MapAction(
		Commands.ReimportModeSameFileOverwriteTransformations,
		FExecuteAction::CreateLambda([this] { ReimportMode = EWaveEditorReimportMode::SameFileOverwrite; }),
		FCanExecuteAction::CreateLambda([this] { return CanExecuteReimport(); }),
		FIsActionChecked::CreateLambda([this] {return ReimportMode == EWaveEditorReimportMode::SameFileOverwrite; }));

	ToolkitCommands->MapAction(
		Commands.ReimportModeNewFile,
		FExecuteAction::CreateLambda([this] { ReimportMode = EWaveEditorReimportMode::SelectFile; }),
		FCanExecuteAction::CreateLambda([this] { return CanExecuteReimport(); }),
		FIsActionChecked::CreateLambda([this] {return ReimportMode == EWaveEditorReimportMode::SelectFile; }));

	ToolkitCommands->MapAction(
		Commands.ToggleFadeIn,
		FExecuteAction::CreateSP(this, &FWaveformEditor::ToggleFadeIn),
		FCanExecuteAction::CreateSP(this, &FWaveformEditor::CanFadeIn));

	ToolkitCommands->MapAction(
		Commands.FadeInLinear,
		FExecuteAction::CreateLambda([this] { FadeInCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Linear]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeInCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Linear]; }));

	ToolkitCommands->MapAction(
		Commands.FadeInExponential,
		FExecuteAction::CreateLambda([this] { FadeInCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Exponetial]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeInCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Exponetial]; }));

	ToolkitCommands->MapAction(
		Commands.FadeInLogarithmic,
		FExecuteAction::CreateLambda([this] { FadeInCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Logarithmic]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeInCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Logarithmic]; }));

	ToolkitCommands->MapAction(
		Commands.FadeInSigmoid,
		FExecuteAction::CreateLambda([this] { FadeInCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Sigmoid]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeInCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Sigmoid]; }));

	ToolkitCommands->MapAction(
		Commands.ToggleFadeOut,
		FExecuteAction::CreateSP(this, &FWaveformEditor::ToggleFadeOut),
		FCanExecuteAction::CreateSP(this, &FWaveformEditor::CanFadeOut));

	ToolkitCommands->MapAction(
		Commands.FadeOutLinear,
		FExecuteAction::CreateLambda([this] { FadeOutCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Linear]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeOutCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Linear]; }));

	ToolkitCommands->MapAction(
		Commands.FadeOutExponential,
		FExecuteAction::CreateLambda([this] { FadeOutCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Exponetial]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeOutCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Exponetial]; }));

	ToolkitCommands->MapAction(
		Commands.FadeOutLogarithmic,
		FExecuteAction::CreateLambda([this] { FadeOutCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Logarithmic]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeOutCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Logarithmic]; }));

	ToolkitCommands->MapAction(
		Commands.FadeOutSigmoid,
		FExecuteAction::CreateLambda([this] { FadeOutCurve = UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Sigmoid]; }),
		FCanExecuteAction::CreateLambda([this] {return true; }),
		FIsActionChecked::CreateLambda([this] {return FadeOutCurve == UWaveformTransformationTrimFade::FadeModeToCurveValueMap[EWaveEditorFadeMode::Sigmoid]; }));

	ToolkitCommands->MapAction(
		Commands.LeftBoundsIncrease,
		FExecuteAction::CreateLambda([this] { ModifyMarkerLoopRegion(ELoopModificationControls::LeftHandleIncrement); }));

	ToolkitCommands->MapAction(
		Commands.LeftBoundsDecrease,
		FExecuteAction::CreateLambda([this] { ModifyMarkerLoopRegion(ELoopModificationControls::LeftHandleDecrement); }));

	ToolkitCommands->MapAction(
		Commands.RightBoundsIncrease,
		FExecuteAction::CreateLambda([this] { ModifyMarkerLoopRegion(ELoopModificationControls::RightHandleIncrement); }));

	ToolkitCommands->MapAction(
		Commands.RightBoundsDecrease,
		FExecuteAction::CreateLambda([this] { ModifyMarkerLoopRegion(ELoopModificationControls::RightHandleDecrement); }));

	ToolkitCommands->MapAction(
		Commands.BoundsIncrementIncrease,
		FExecuteAction::CreateLambda([this] { ModifyMarkerLoopRegion(ELoopModificationControls::IncreaseIncrement); }));

	ToolkitCommands->MapAction(
		Commands.BoundsIncrementDecrease,
		FExecuteAction::CreateLambda([this] { ModifyMarkerLoopRegion(ELoopModificationControls::DecreaseIncrement); }));

	ToolkitCommands->MapAction(
		Commands.SelectNextLoop,
		FExecuteAction::CreateLambda([this] { CycleMarkerLoopRegion(ELoopModificationControls::SelectNextLoop); }));

	ToolkitCommands->MapAction(
		Commands.SelectPreviousLoop,
		FExecuteAction::CreateLambda([this] { CycleMarkerLoopRegion(ELoopModificationControls::SelectPreviousLoop); }));

	ToolkitCommands->MapAction(
		Commands.CreateMarker,
		FExecuteAction::CreateLambda([this] { FWaveformEditor::CreateMarker(false); }));

	ToolkitCommands->MapAction(
		Commands.CreateLoopRegion,
		FExecuteAction::CreateLambda([this] { FWaveformEditor::CreateMarker(true); }));

	ToolkitCommands->MapAction(
		Commands.DeleteMarker,
		FExecuteAction::CreateSP(this, &FWaveformEditor::DeleteMarker));

	ToolkitCommands->MapAction(
		Commands.SkipToNextMarker,
		FExecuteAction::CreateSP(this, &FWaveformEditor::SkipToNextMarker));

	return true;
}


FName FWaveformEditor::GetEditorName() const
{
	return EditorName;
}

FName FWaveformEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FWaveformEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Waveform Editor");
}

EVisibility FWaveformEditor::GetVisibilityWhileAssetCompiling() const
{
	return EVisibility::Visible;
}

FString FWaveformEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Waveform Editor").ToString();
}

FLinearColor FWaveformEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}

void FWaveformEditor::OnAssetReimport(UObject* ReimportedObject, bool bSuccessfullReimport)
{
	if (!bSuccessfullReimport)
	{
		return;
	}

	if (ReimportedObject->IsA<USoundWave>() && ReimportedObject->GetPathName() == SoundWave->GetPathName())
	{
		//If the waveform editor is open, updating Transformations on reimport will show
		// the effects of the overwritten transformations immediately instead of after a
		// change is made by the user.
		TransformationChainConfig = SoundWave->UpdateTransformations();
		CreateWaveformView();
		WaveformView.DataProvider->RequestSequenceView(TransportCoordinator->GetDisplayRange());
		WaveformView.ViewWidget->SetPlayheadRatio(TransportCoordinator->GetFocusPoint());
		this->TabManager->FindExistingLiveTab(WaveformDisplayTabId)->SetContent(WaveformView.ViewWidget.ToSharedRef());
	}
}


void FWaveformEditor::NotifyPreChange(FEditPropertyChain* PropertyAboutToChange)
{
	check(TransportController != nullptr);
	bWasPlayingBeforeChange = TransportController->IsPlaying();
}

void FWaveformEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged)
{	
	TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* PropertyNode = PropertyThatChanged->GetActiveMemberNode();

	if (!PropertyNode)
	{
		return;
	}
	
	bool bIsTransformation = false;

	do 
	{
		bIsTransformation |= PropertyNode->GetValue()->GetName() == TEXT("Transformations");
		PropertyNode = PropertyNode->GetPrevNode();
	} while (PropertyNode != nullptr);

	if (!bIsTransformation)
	{
		return;
	}
	
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		bIsInteractingWithTransformations = true;

		if (TransportController->IsPlaying() || TransportController->IsPaused())
		{
			TransformInteractionPlayState = AudioComponent->GetPlayState();
			PlaybackTimeBeforeTransformInteraction = LastReceivedPlaybackPercent * (TransformationChainConfig.EndTime - TransformationChainConfig.StartTime);
			StartTimeBeforeTransformInteraction = TransformationChainConfig.StartTime;
			AudioComponent->Stop();
			bWasPlayingBeforeChange = false;
		}
	}

	const bool bUpdateTransformationChain = PropertyChangedEvent.GetPropertyName() == TEXT("Transformations");
	if (bUpdateTransformationChain)
	{
		WaveformView.DataProvider->GenerateLayersChain();
	}
	WaveformView.DataProvider->UpdateRenderElements();
	TransformationChainConfig = SoundWave->GetTransformationChainConfig();

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		UpdateTransportState();

		bIsInteractingWithTransformations = false;
	}
	TObjectPtr<UWaveformTransformationMarkers> MarkerTransformation = GetMarkerTransformation();

	if (MarkerTransformation != nullptr && !bCueChangeRegisteredByWaveformEditor)
	{
		MarkerTransformation->Markers->CueChanged.BindLambda([this]() { RegenerateTransformations(); });
		bCueChangeRegisteredByWaveformEditor = true;
	}
	else if (MarkerTransformation == nullptr)
	{
		bCueChangeRegisteredByWaveformEditor = false;
	}
}

void FWaveformEditor::UpdateTransportState()
{
	if (!TransportController->IsPlaying())
	{
		// StartTime=0, EndTime=-1 if no transformations
		const float TransformationDuration = TransformationChainConfig.EndTime - TransformationChainConfig.StartTime;

		// Clamp Playback/Start times to start if out of range of TransformationChainConfig.
		// If no active+initialized transformation, TransformationDuration will be <= 0.0f, ignore in this case.
		if ((PlaybackTimeBeforeTransformInteraction + StartTimeBeforeTransformInteraction - TransformationChainConfig.StartTime <= 0)
			|| (TransformationDuration > 0.0f && PlaybackTimeBeforeTransformInteraction + StartTimeBeforeTransformInteraction - TransformationChainConfig.StartTime >= TransformationDuration))
		{
			PlaybackTimeBeforeTransformInteraction = 0;
			StartTimeBeforeTransformInteraction = TransformationChainConfig.StartTime;
		}
		const float StartTimeDifference = StartTimeBeforeTransformInteraction - TransformationChainConfig.StartTime;
		const float AdjustedPlaybackTime = PlaybackTimeBeforeTransformInteraction + StartTimeDifference;

		switch (TransformInteractionPlayState)
		{
		case EAudioComponentPlayState::Playing:
			TransportController->Play(AdjustedPlaybackTime);
			TransformInteractionPlayState = EAudioComponentPlayState::Stopped;
			break;
		case EAudioComponentPlayState::Paused:
			TransformInteractionPlayState = EAudioComponentPlayState::Stopped;
		case EAudioComponentPlayState::Stopped:
			// Set TransportController and displayed TransportCoordinator to value clamped within TransformationChainConfig.
			TransportController->CacheStartTime(AdjustedPlaybackTime);
			if (!TransportCoordinator->IsScrubbing())
			{
				const float CachedAudioStartTimeAsPercentage = TransportController->GetCachedAudioStartTimeAsPercentage();
				TransportCoordinator->SetProgressRatio(CachedAudioStartTimeAsPercentage);
			}
			break;
		default:
			break;
		}

		if (bWasPlayingBeforeChange)
		{
			AudioComponent->Play();
		}
	}
}

void FWaveformEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		WaveformView.DataProvider->GenerateLayersChain();
		WaveformView.DataProvider->UpdateRenderElements();

		TObjectPtr<UWaveformTransformationMarkers> MarkerTransformation = GetMarkerTransformation();

		if (MarkerTransformation != nullptr && !bCueChangeRegisteredByWaveformEditor)
		{
			MarkerTransformation->Markers->CueChanged.BindLambda([this]() { RegenerateTransformations(); });
			bCueChangeRegisteredByWaveformEditor = true;
		}
		else if (MarkerTransformation == nullptr)
		{
			bCueChangeRegisteredByWaveformEditor = false;
		}
	}
}

void FWaveformEditor::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

bool FWaveformEditor::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	bool bShoouldUndo = false;
	for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
	{
		UObject* Object = TransactionObjectPair.Key;
		const UClass* ObjectClass = Object->GetClass();

		if (ObjectClass)
		{
			bShoouldUndo = ObjectClass->IsChildOf(UWaveformTransformationBase::StaticClass()) || ObjectClass->IsChildOf(USoundWave::StaticClass());
		}
	}

	return bShoouldUndo;
}

void FWaveformEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(MenuContext);

	UWaveformEditorToolMenuContext* Context = NewObject<UWaveformEditorToolMenuContext>();
	Context->WaveformEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

bool FWaveformEditor::CreateDetailsViews()
{
	if (!ensure(SoundWave)) 
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Trying to setup wav editor properties view from a null SoundWave"));
		return false;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = this;

	PropertiesDetails = PropertyModule.CreateDetailView(Args);
	PropertiesDetails->SetObject(SoundWave);

	TransformationsDetails = PropertyModule.CreateDetailView(Args);
	FOnGetDetailCustomizationInstance TransformationsDetailsCustomizationInstance = FOnGetDetailCustomizationInstance::CreateLambda([]() {
			return MakeShared<FWaveformTransformationsDetailsCustomization>();
		}
	);

	TransformationsDetails->RegisterInstancedCustomPropertyLayout(SoundWave->GetClass(), TransformationsDetailsCustomizationInstance);
	TransformationsDetails->SetObject(SoundWave);

	return true;
}

TSharedRef<SDockTab> FWaveformEditor::SpawnTab_WaveformDisplay(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == WaveformDisplayTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("WaveformDisplayTitle", "Waveform Display"))
		[
			WaveformView.ViewWidget.ToSharedRef()
		];
}

const TSharedRef<FTabManager::FLayout> FWaveformEditor::SetupStandaloneLayout()
{
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_WaveformEditor_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(PropertiesTabId, ETabState::OpenedTab)
					->AddTab(TransformationsTabId, ETabState::OpenedTab)
					->SetForegroundTab(PropertiesTabId)

				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(true)
					->AddTab(WaveformDisplayTabId, ETabState::OpenedTab)
				)
			)
		);

	return StandaloneDefaultLayout;
}

TSharedRef<SDockTab> FWaveformEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PropertiesTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("SoundWaveDetailsTitle", "Details"))
		[
			PropertiesDetails.ToSharedRef()
		];
}

TSharedRef<SDockTab> FWaveformEditor::SpawnTab_Transformations(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TransformationsTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("SoundWaveProcessingTitle", "Processing"))
		[
			TransformationsDetails.ToSharedRef()
		];
}

bool FWaveformEditor::CreateWaveformView()
{
	if (!ensure(SoundWave))
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Trying to setup waveform panel from a null SoundWave"));
		return false;
	}
	
	if (WaveformView.IsValid())
	{
		RemoveWaveformViewDelegates(*WaveformView.DataProvider, *WaveformView.ViewWidget);
	}

	WaveformView = FTransformedWaveformViewFactory::Get().GetTransformedView(SoundWave, TransportCoordinator.ToSharedRef(), this, ZoomManager);

	check(ZoomManager)

	BindWaveformViewDelegates(*WaveformView.DataProvider, *WaveformView.ViewWidget);

	ZoomManager->OnZoomRatioChanged.AddSP(TransportCoordinator.ToSharedRef(), &FSparseSampledSequenceTransportCoordinator::SetZoomRatio);
	TransportCoordinator->OnDisplayRangeUpdated.AddSP(this, &FWaveformEditor::HandleDisplayRangeUpdate);

	return WaveformView.IsValid();
}

void FWaveformEditor::BindWaveformViewDelegates(FWaveformEditorSequenceDataProvider& ViewDataProvider, STransformedWaveformViewPanel& ViewWidget)
{
	check(TransportCoordinator)

	ViewDataProvider.OnRenderElementsUpdated.AddSP(this, &FWaveformEditor::HandleRenderDataUpdate);
	TransportCoordinator->OnFocusPointMoved.AddSP(&ViewWidget, &STransformedWaveformViewPanel::SetPlayheadRatio);
}

void FWaveformEditor::RemoveWaveformViewDelegates(FWaveformEditorSequenceDataProvider& ViewDataProvider, STransformedWaveformViewPanel& ViewWidget)
{
	check(TransportCoordinator)

	ViewDataProvider.OnRenderElementsUpdated.RemoveAll(this);
	TransportCoordinator->OnFocusPointMoved.RemoveAll(&ViewWidget);
}

bool FWaveformEditor::CreateTransportCoordinator()
{
	TransportCoordinator = MakeShared<FSparseSampledSequenceTransportCoordinator>();
	TransportCoordinator->OnDisplayRangeUpdated.AddSP(this, &FWaveformEditor::HandleDisplayRangeUpdate);

	return TransportCoordinator != nullptr;
}

void FWaveformEditor::HandlePlaybackPercentageChange(const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackPercentage)
{
	const bool bIsStopped = AudioComponent->GetPlayState() == EAudioComponentPlayState::Stopped;
	const bool bIsPaused = AudioComponent->GetPlayState() == EAudioComponentPlayState::Paused;
	const bool bPropagatePercentage = !bIsStopped && !bIsPaused;
	LastReceivedPlaybackPercent = InPlaybackPercentage;
	
	if (InComponent == AudioComponent && bPropagatePercentage)
	{
		if (TransportCoordinator.IsValid())
		{
			const float ClampedPlayBackPercentage = FGenericPlatformMath::Fmod(InPlaybackPercentage, 1.f);
			TransportCoordinator->SetProgressRatio(ClampedPlayBackPercentage);
		}
	}
}

void FWaveformEditor::HandleAudioComponentPlayStateChanged(const UAudioComponent* InAudioComponent, EAudioComponentPlayState NewPlayState)
{
	if (InAudioComponent != AudioComponent)
	{
		return;
	}

	switch (NewPlayState)
	{
	default:
		break;
	case EAudioComponentPlayState::Stopped:
		if (!TransportCoordinator->IsScrubbing())
		{
			// USoundWave::PostEditChangeProperty calls USoundWave::UpdateAsset which frees the soundwave's resources,
			// stopping the sound, calling this handler.  Avoiding this implicit state change is currently too risky and
			// so we clear the AudioComponent's paused state instead to keep it in sync with the USoundWave.
			AudioComponent->SetPaused(false);

			if (!bIsInteractingWithTransformations)
			{
				const float CachedAudioStartTimeAsPercentage = TransportController->GetCachedAudioStartTimeAsPercentage();
				TransportCoordinator->SetProgressRatio(CachedAudioStartTimeAsPercentage); // show cached start time for next play rather than playhead stop point (doesn't affect pause)
			}
		}
		break;
	}
}

void FWaveformEditor::HandleRenderDataUpdate()
{
	if (TransportCoordinator != nullptr)
	{
		TransportCoordinator->UpdatePlaybackRange(WaveformView.DataProvider->GetTransformedWaveformBounds());
		WaveformView.DataProvider->RequestSequenceView(TransportCoordinator->GetDisplayRange());
	}
}

void FWaveformEditor::HandleDisplayRangeUpdate(const TRange<double> NewRange)
{
	WaveformView.DataProvider->RequestSequenceView(NewRange);
}

void FWaveformEditor::HandlePlayheadScrub(const float InTargetPlayBackRatio, const bool bIsMoving)
{
	if (bIsMoving)
	{
		if (TransportController->IsPlaying())
		{
			TransportController->Stop();
			bWasPlayingBeforeScrubbing = true;
		}
	}
	else
	{
		float ActiveDuration = TransformationChainConfig.EndTime - TransformationChainConfig.StartTime;
		if (ActiveDuration <= 0.0f)
		{
			// If no active+initialized transformation, ActiveDuration will be <= 0.0f, so fallback to sound duration:
			ActiveDuration = SoundWave->Duration; // not GetDuration() to get the raw duration and not INDEFINITELY_LOOPING_DURATION if looping
		}
		const float ClampedTargetPlayBackRatio = FMath::Max(InTargetPlayBackRatio, 0.0f);
		const float NewTime = FGenericPlatformMath::Fmod(ClampedTargetPlayBackRatio, 1.0f) * ActiveDuration;

		PlaybackTimeBeforeTransformInteraction = NewTime;
		StartTimeBeforeTransformInteraction = TransformationChainConfig.StartTime;

		if (TransportController->IsPlaying())
		{
			TransportController->Seek(NewTime);
			return;
		}

		if (bWasPlayingBeforeScrubbing)
		{
			TransportController->Play(NewTime);
			bWasPlayingBeforeScrubbing = false;
		}
		else
		{
			TransportController->CacheStartTime(NewTime);
		}
	}
}

void FWaveformEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SoundWave);
	Collector.AddReferencedObject(AudioComponent);
}

FString FWaveformEditor::GetReferencerName() const
{
	return TEXT("FWaveformEditor");
}

bool FWaveformEditor::CanPressPlayButton() const
{
	return TransportController->CanPlay() && (TransportController->IsPaused() || !TransportController->IsPlaying());
}

bool FWaveformEditor::CreateWaveWriter()
{
	if (!ensure(SoundWave))
	{
		return false;
	}

	WaveWriter = MakeShared<FWaveformEditorWaveWriter>(SoundWave);
	return WaveWriter != nullptr;
}

void FWaveformEditor::ExportWaveform()
{
	check(WaveWriter);
	WaveWriter->ExportTransformedWaveform();
}

const UWaveformEditorTransformationsSettings* FWaveformEditor::GetWaveformEditorTransformationsSettings() const
{
	const UWaveformEditorTransformationsSettings* WaveformEditorTransformationsSettings = GetDefault<UWaveformEditorTransformationsSettings>();
	check(WaveformEditorTransformationsSettings)

	return WaveformEditorTransformationsSettings;
}

FText FWaveformEditor::GetReimportButtonToolTip() const
{
	FText ReimportModeText;

	switch (ReimportMode)
	{
	case(EWaveEditorReimportMode::SelectFile):
		ReimportModeText = LOCTEXT("SelectFile", "Reimport from new file");
		break;
	case(EWaveEditorReimportMode::SameFile):
		ReimportModeText = LOCTEXT("SameFile", "Reimport from same file");
		break;
	case(EWaveEditorReimportMode::SameFileOverwrite):
		ReimportModeText = LOCTEXT("SameFileOverwrite", "Reimport from same file and overwrite transformations");
		break;
	default:
		static_assert(static_cast<int32>(EWaveEditorReimportMode::COUNT) == 3, "Possible missing switch case coverage for 'EWaveEditorReimportMode'");
		break;
	}

	return FText::Format(LOCTEXT("WaveformEditorReimportButtonTooltip", "{0}."), ReimportModeText);
}

FText FWaveformEditor::GetExportButtonToolTip() const
{

	FText ExportModeText;

	if (!WaveWriter)
	{
		return ExportModeText;
	}

	switch (WaveWriter->GetExportChannelsFormat())
	{
	case(WaveformEditorWaveWriter::EChannelFormat::Stereo):
		ExportModeText = LOCTEXT("ExportModeStereo", "stereo");
		break;
	case(WaveformEditorWaveWriter::EChannelFormat::Mono):
		ExportModeText = LOCTEXT("ExportModeMono", "mono");
		break;
	default:
		break;
	}

	return FText::Format(LOCTEXT("WaveformEditorExportButtonTooltip", "Exports the edited waveform to a {0} USoundWave asset."), ExportModeText);
}

#undef LOCTEXT_NAMESPACE

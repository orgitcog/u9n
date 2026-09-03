// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityAssetEditorToolkit.h"
#include "MetaHumanIdentityAssetEditor.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityLog.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityCommands.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanIdentityViewportSettings.h"
#include "MetaHumanIdentityStyle.h"
#include "MetaHumanIdentityTooltipProvider.h"
#include "MetaHumanToolkitStyle.h"
#include "MetaHumanToolkitCommands.h"
#include "MetaHumanEditorSettings.h"
#include "MetaHumanIdentityAssetEditorContext.h"
#include "MetaHumanTemplateMeshComponent.h"
#include "MetaHumanPredictiveSolversTask.h"
#include "MetaHumanIdentityStateValidator.h"
#include "MetaHumanTrace.h"
#include "MetaHumanSupportedRHI.h"

#include "UI/SMetaHumanIdentityPartsEditor.h"
#include "UI/SMetaHumanIdentityPromotedFramesEditor.h"
#include "UI/SMetaHumanIdentityOutliner.h"
#include "UI/MetaHumanIdentityViewportClient.h"

#include "SMetaHumanEditorViewport.h"

#include "MetaHumanFaceContourTrackerAsset.h"
#include "MetaHumanFaceFittingSolver.h"
#include "PromotedFrameUtils.h"

#include "Widgets/Docking/SDockTab.h"
#include "AdvancedPreviewScene.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ToolMenus.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SWarningOrErrorBox.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Layout/SScaleBox.h"
#include "IDetailsView.h"
#include "ScopedTransaction.h"

#include "Sections/MovieSceneAudioSection.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "ImgMediaSource.h"
#include "MetaHumanSequence.h"
#include "CaptureData.h"
#include "MetaHumanFootageComponent.h"
#include "MediaTexture.h"

#include "MetaHumanMovieSceneMediaTrack.h"
#include "Engine/Texture2D.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "LandmarkConfigIdentityHelper.h"
#include "MetaHumanContourDataVersion.h"
#include "MetaHumanMovieSceneChannel.h"
#include "MovieScene.h"
#include "Dialogs/Dialogs.h"
#include "Misc/ScopedSlowTask.h"
#include "Async/Async.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "MetaHumanFaceTrackerInterface.h"

#include "SkelMeshDNAReader.h"

#include "ImageSequenceTimecodeUtils.h"

#define LOCTEXT_NAMESPACE "MetaHumanIdentityToolkit"

const FName FMetaHumanIdentityAssetEditorToolkit::PartsTabId(TEXT("FMetaHumanIdentityAssetEditorToolkit_Parts"));
const FName FMetaHumanIdentityAssetEditorToolkit::OutlinerTabId(TEXT("FMetaHumanIdentityAssetEditorToolkit_Outliner"));

FMetaHumanIdentityAssetEditorToolkit::FMetaHumanIdentityAssetEditorToolkit(UAssetEditor* InOwningAssetEditor)
	: FMetaHumanToolkitBase{ InOwningAssetEditor }
{
	// Get a reference to the Identity being edited
	TArray<UObject*> ObjectsToEdit;
	OwningAssetEditor->GetObjectsToEdit(ObjectsToEdit);
	check(!ObjectsToEdit.IsEmpty() && ObjectsToEdit[0] != nullptr);

	Identity = CastChecked<UMetaHumanIdentity>(ObjectsToEdit[0]);

	LoadGenericFaceContourTracker();

	// Register the commands that are used in this editor toolbar
	FMetaHumanIdentityEditorCommands::Register();

	LandmarkConfigHelper = MakeShared<FLandmarkConfigIdentityHelper>();

	IdentityStateValidator = MakeShared<FMetaHumanIdentityStateValidator>();

	const FString LayoutString = TEXT("Standalone_MetaHumanIdentityAssetEditorToolkit_Layout_v1");
	StandaloneDefaultLayout = FTabManager::NewLayout(FName{ LayoutString })
		->AddArea
		(
			// Create a vertical area and spawn the toolbar
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.35f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.3f)
						->AddTab(PartsTabId, ETabState::OpenedTab)
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.7f)
						->AddTab(DetailsTabID, ETabState::OpenedTab)
						->SetHideTabWell(true)
						->AddTab(PreviewSettingsTabId, ETabState::ClosedTab)
						->SetHideTabWell(false)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.97f)
					->AddTab(ViewportTabID, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(OutlinerTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				->AddTab(TimelineTabId, ETabState::OpenedTab)
			)
		);
}

FMetaHumanIdentityAssetEditorToolkit::~FMetaHumanIdentityAssetEditorToolkit()
{
	if (TimelineSequencer)
	{
		TimelineSequencer->OnMovieSceneDataChanged().RemoveAll(this);
		TimelineSequencer->Close();
	}

	if (Identity->OnAutoRigServiceFinishedDelegate.IsBound())
	{
		Identity->OnAutoRigServiceFinishedDelegate.RemoveAll(this);
	}
}

FName FMetaHumanIdentityAssetEditorToolkit::GetToolkitFName() const
{
	return TEXT("MetaHumanIdentityAssetEditorToolkit");
}

FText FMetaHumanIdentityAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "MetaHuman Identity Asset Editor Toolkit");
}

FText FMetaHumanIdentityAssetEditorToolkit::GetToolkitToolTipText() const
{
	FText AssetName = FText::FromString(Identity->GetName());
	return FText::Format(LOCTEXT("IdentityToolkitToolTipTextExtended", "Asset: {0} (MetaHuman Identity)"), AssetName);
}

FString FMetaHumanIdentityAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "MetaHuman ").ToString();
}

FLinearColor FMetaHumanIdentityAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FColor::White;
}

void FMetaHumanIdentityAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "MetaHuman Identity"));

	FMetaHumanToolkitBase::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PartsTabId, FOnSpawnTab::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::SpawnPartsTab))
		.SetDisplayName(LOCTEXT("PartsIdTabName", "Parts"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("Identity.Tab.Parts") });

	InTabManager->RegisterTabSpawner(OutlinerTabId, FOnSpawnTab::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::SpawnOutlinerTab))
		.SetDisplayName(LOCTEXT("OutlinerTabName", "Markers"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon{ FMetaHumanToolkitStyle::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.Tabs.Markers") });
}

void FMetaHumanIdentityAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FMetaHumanToolkitBase::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterAllTabSpawners();
}

void FMetaHumanIdentityAssetEditorToolkit::HandleSequencerGlobalTimeChanged()
{
	FMetaHumanToolkitBase::HandleSequencerGlobalTimeChanged();

	if (SelectedIdentityPose != nullptr && IsUsingFootageData())
	{
		Identity->ViewportSettings->SetFrameTimeForPose(SelectedIdentityPose->PoseType, TimelineSequencer->GetGlobalTime().Time);
	}

	FText Overlay;
	UFootageCaptureData* FootageCaptureData = GetFootageCaptureData();

	if (FootageCaptureData && !FootageCaptureData->ImageSequences.IsEmpty() && FootageCaptureData->ImageSequences[0] && MediaFrameRanges.Contains(FootageCaptureData->ImageSequences[0]))
	{
		// Check if frame is excluded in capture data
		int32 FrameForExclusionCheck = GetCurrentFrameNumber().Value - MediaFrameRanges[FootageCaptureData->ImageSequences[0]].GetLowerBoundValue().Value;
		if (FFrameRange::ContainsFrame(FrameForExclusionCheck, GetFootageCaptureData()->CaptureExcludedFrames))
		{
			UEnum::GetDisplayValueAsText(EFrameRangeType::CaptureExcluded, Overlay);
		}
	}

	if (ViewportClient)
	{
		GetMetaHumanIdentityViewportClient()->SetOverlay(Overlay);
	}
}

void FMetaHumanIdentityAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (Sequence)
	{
		Collector.AddReferencedObject(Sequence);
	}
	if (PromotedFrameTexture.Key)
	{
		Collector.AddReferencedObject(PromotedFrameTexture.Key);
	}
	if (PromotedFrameTexture.Value)
	{
		Collector.AddReferencedObject(PromotedFrameTexture.Value);
	}
}

FString FMetaHumanIdentityAssetEditorToolkit::GetReferencerName() const
{
	return TEXT("FMetaHumanIdentityAssetEditorToolkit");
}

void FMetaHumanIdentityAssetEditorToolkit::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged)
{
	FMetaHumanToolkitBase::NotifyPostChange(InPropertyChangedEvent, InPropertyThatChanged);

	if (IdentityPartsEditor.IsValid())
	{
		IdentityPartsEditor->NotifyPostChange(InPropertyChangedEvent, InPropertyThatChanged);
	}

	if (PromotedFramesEditorWidget.IsValid())
	{
		PromotedFramesEditorWidget->NotifyPostChange(InPropertyChangedEvent, InPropertyThatChanged);
	}
}

void FMetaHumanIdentityAssetEditorToolkit::InitToolMenuContext(struct FToolMenuContext& InMenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(InMenuContext);

	UMetaHumanIdentityAssetEditorContext* Context = NewObject<UMetaHumanIdentityAssetEditorContext>();
	Context->MetaHumanIdentityAssetEditor = SharedThis(this);
	InMenuContext.AddObject(Context);
}

void FMetaHumanIdentityAssetEditorToolkit::SetEditingObject(UObject* InObject)
{
	// Overriding the base SetEditing object to do nothing as this will set the object
	// being edited in the details panel as the last action in UAssetEditor::Initialize()
	// so that the Identity will always be the object being edited but we want control
	// over that here as the object being edited is determined by the selection in the tree view.
	// See HandleIdentityTreeSelectionChanged to see how we are setting the object being edited
}

TSharedPtr<FEditorViewportClient> FMetaHumanIdentityAssetEditorToolkit::CreateEditorViewportClient() const
{
	TSharedRef<FMetaHumanIdentityViewportClient> IdentityViewportClient = MakeShared<FMetaHumanIdentityViewportClient>(PreviewScene.Get(), Identity);
	IdentityViewportClient->OnCameraStoppedDelegate.AddSP(const_cast<FMetaHumanIdentityAssetEditorToolkit*>(this), &FMetaHumanIdentityAssetEditorToolkit::HandleCameraStopped);
	return IdentityViewportClient;
}

void FMetaHumanIdentityAssetEditorToolkit::CreateWidgets()
{
	FMetaHumanToolkitBase::CreateWidgets();

	SAssignNew(IdentityPartsEditor, SMetaHumanIdentityPartsEditor)
		.Identity(Identity)
		.PreviewActor(PreviewActor)
		.ViewportClient(GetMetaHumanIdentityViewportClient())
		.OnIdentityTreeSelectionChanged(this, &FMetaHumanIdentityAssetEditorToolkit::HandleIdentityTreeSelectionChanged)
		.OnCaptureSourceSelectionChanged(this, &FMetaHumanIdentityAssetEditorToolkit::HandleCaptureDataChanged)
		.OnIdentityPartRemoved(this, &FMetaHumanIdentityAssetEditorToolkit::HandleIdentityPartRemoved)
		.OnIdentityPoseAdded(this, &FMetaHumanIdentityAssetEditorToolkit::HandleIdentityPoseAdded)
		.OnIdentityPoseRemoved(this, &FMetaHumanIdentityAssetEditorToolkit::HandleIdentityPoseRemoved);

	SAssignNew(PromotedFramesEditorWidget, SMetaHumanIdentityPromotedFramesEditor)
		.ViewportClient(GetMetaHumanIdentityViewportClient())
		.Identity(Identity)
		.CommandList(GetToolkitCommands())
		.FrameRange(this, &FMetaHumanIdentityAssetEditorToolkit::GetSequencerPlaybackRange)
		.IsCurrentFrameValid(this, &FMetaHumanIdentityAssetEditorToolkit::GetIsCurrentFrameValid)
		.IsTrackingCurrentFrame_Lambda([this] { return Identity->IsFrameTrackingPipelineProcessing(); })
		.OnPromotedFrameSelectionChanged(this, &FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameSelectedInPromotedFramesPanel)
		.OnPromotedFrameAdded(this, &FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameAdded)
		.OnPromotedFrameRemoved(this, &FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameRemoved)
		.OnPromotedFrameTrackingModeChanged(this, &FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameTrackingModeChanged);

	PromotedFramesEditorWidget->SetToolTipText(TAttribute<FText>(PromotedFramesEditorWidget.ToSharedRef(), &SMetaHumanIdentityPromotedFramesEditor::GetPromotedFramesContainerTooltip));
}

void FMetaHumanIdentityAssetEditorToolkit::PostInitAssetEditor()
{
	FMetaHumanToolkitBase::PostInitAssetEditor();

	CreateSceneCaptureComponent();

	ExtendMenu();
	ExtendToolBar();

	Sequence->GetExcludedFrameInfo.BindSP(this, &FMetaHumanIdentityAssetEditorToolkit::GetExcludedFrameInfo);

	// Set the default tracker image size used in Mesh to MetaHuman, which is the default for this editor
	GetMetaHumanIdentityViewportClient()->SetTrackerImageSize(UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize);

	if (!Identity->OnAutoRigServiceFinishedDelegate.IsBound())
	{
		Identity->OnAutoRigServiceFinishedDelegate.AddSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleAutoriggingServiceFinished);
	}

	SetUpEditorForCaptureDataType();
	PromotedFrameTexture.Key = UTexture2D::CreateTransient(256, 256);
	PromotedFrameTexture.Value = UTexture2D::CreateTransient(256, 256);

	// Restore the tree view selection
	IdentityPartsEditor->SelectNode(Identity->ViewportSettings->SelectedTreeNode);

	// Restore the selected promoted frame
	if (SelectedIdentityPose != nullptr)
	{
		constexpr bool bForceNotify = true;
		PromotedFramesEditorWidget->SetSelection(Identity->ViewportSettings->GetSelectedPromotedFrame(SelectedIdentityPose->PoseType), bForceNotify);
	}

	if (IModularFeatures::Get().IsModularFeatureAvailable(IPredictiveSolverInterface::GetModularFeatureName()))
	{
		bDepthProcessingEnabled = true;
	}
	
	IdentityStateValidator->PostAssetLoadHashInitialization(Identity);

	GetMetaHumanIdentityViewportClient()->UpdateABVisibility();
}

TSharedRef<SDockTab> FMetaHumanIdentityAssetEditorToolkit::SpawnPartsTab(const FSpawnTabArgs& InArgs)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PartsTabTitle", "Identity Parts"))
		.ToolTipText(LOCTEXT("IdentityPartsTabTooltip", "MetaHuman Identity Parts Tree View\n\nIn this tab you can add body parts to MetaHuman Identity and Pose data specific to each part.\nClick on the component items in the Tree View to select them in the AB Viewport\nand review their details in the Details tab.\nClick on Pose items to enable their Promoted Frames Timeline (and Footage Timeline in case\nof poses containing footage Capture Data)"))
		[
			IdentityPartsEditor.ToSharedRef()
		];
}

TSharedRef<SDockTab> FMetaHumanIdentityAssetEditorToolkit::SpawnOutlinerTab(const FSpawnTabArgs& InArgs)
{
	UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = nullptr;
	int32 PromotedFrameIndex = INDEX_NONE;

	if (PromotedFramesEditorWidget.IsValid())
	{
		SelectedPromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame();

		if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
		{
			 PromotedFrameIndex = Pose->PromotedFrames.IndexOfByKey(SelectedPromotedFrame);
		}
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("OutlinerTabTitle", "Markers"))
		.ToolTipText(LOCTEXT("IdentityOutlinerTooltip", "Marker Curves\n\nIn this tab you can toggle the visibility of Markers or Marker Groups and whether they are used for solving the MetaHuman Identity\nThe contents of the panel show only when a Pose is selected in MetaHuman Identity Parts tab, and at least one frame is promoted."))
		[
			SAssignNew(OutlinerWidget, SMetaHumanIdentityOutliner)
			.LandmarkConfigHelper(LandmarkConfigHelper)
			.ViewportClient(GetMetaHumanIdentityViewportClient())
			.FaceIsConformed(this, &FMetaHumanIdentityAssetEditorToolkit::FaceIsConformed)
		];
}

void FMetaHumanIdentityAssetEditorToolkit::BindCommands()
{
	FMetaHumanToolkitBase::BindCommands();

	const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();

	ToolkitCommands->MapAction(Commands.RigidFitCurrent,
							   FExecuteAction{},
							   FCanExecuteAction::CreateLambda([] { return false; }));

	ToolkitCommands->MapAction(Commands.RigidFitAll,
							   FExecuteAction{},
							   FCanExecuteAction::CreateLambda([] { return false; }));

	ToolkitCommands->MapAction(Commands.TrackCurrent,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleTrackCurrent),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanTrackCurrent));

	ToolkitCommands->MapAction(Commands.IdentitySolve,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleConform),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanConform));

	ToolkitCommands->MapAction(Commands.MeshToMetaHumanDNAOnly,
								FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleSubmitToAutoRigging),
								FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanSubmitToAutoRigging));

	ToolkitCommands->MapAction(Commands.ImportDNA,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleImportDNA),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanImportDNA));

	ToolkitCommands->MapAction(Commands.ExportDNA,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleExportDNA),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanExportDNA));

	ToolkitCommands->MapAction(Commands.FitTeeth,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleFitTeeth),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanFitTeeth));

	ToolkitCommands->MapAction(Commands.PrepareForPerformance,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandlePredictiveSolverTraining),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanRunSolverTraining));

	ToolkitCommands->MapAction(Commands.ResetTemplateMesh,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleResetTemplateMesh),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanResetTemplateMesh));

	ToolkitCommands->MapAction(Commands.ExportTemplateMesh,
							   FExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::HandleExportTemplateMeshClicked),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::CanExportTemplateMesh));

	ABCommandList.MapAction(Commands.ToggleCurrentPose,
							GetMetaHumanIdentityViewportClient(),
							&FMetaHumanIdentityViewportClient::ToggleCurrentPoseVisibility,
							&FMetaHumanIdentityViewportClient::CanExecuteAction,
							&FMetaHumanIdentityViewportClient::IsCurrentPoseVisible);

	ABCommandList.MapAction(Commands.ToggleConformalMesh,
							GetMetaHumanIdentityViewportClient(),
							&FMetaHumanIdentityViewportClient::ToggleConformalMeshVisibility,
							&FMetaHumanIdentityViewportClient::CanExecuteAction,
							&FMetaHumanIdentityViewportClient::IsTemplateMeshVisible);

	ABCommandList.MapAction(Commands.ToggleRig,
							GetMetaHumanIdentityViewportClient(),
							&FMetaHumanIdentityViewportClient::ToggleRigVisibility,
							&FMetaHumanIdentityViewportClient::CanExecuteAction,
							&FMetaHumanIdentityViewportClient::IsRigVisible);

	ABCommandList.MapAction(Commands.TogglePlayback,
							SharedThis(this),
							&FMetaHumanIdentityAssetEditorToolkit::HandleTogglePlayback,
							&FMetaHumanIdentityAssetEditorToolkit::CanTogglePlayback,
							&FMetaHumanIdentityAssetEditorToolkit::CanTogglePlayback);
}

TSharedRef<SWidget> FMetaHumanIdentityAssetEditorToolkit::GetViewportExtraContentWidget()
{
	return PromotedFramesEditorWidget.ToSharedRef();
}

void FMetaHumanIdentityAssetEditorToolkit::HandleGetViewABMenuContents(EABImageViewMode InViewMode, FMenuBuilder& InMenuBuilder)
{
	const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();
	const FMetaHumanToolkitCommands& BaseCommands = FMetaHumanToolkitCommands::Get();

	UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>();
	const bool bShowNeutral = Face && Face->FindPoseByType(EIdentityPoseType::Neutral);
	const bool bShowTeeth = Face && Face->FindPoseByType(EIdentityPoseType::Teeth);

	InMenuBuilder.BeginSection(TEXT("GeometryExtensionsHook"), LOCTEXT("GeometrySectionLabel", "Geometry"));
	{
		if (bShowNeutral || bShowTeeth)
		{
			InMenuBuilder.AddMenuEntry(Commands.ToggleCurrentPose);
		}

		InMenuBuilder.AddMenuEntry(Commands.ToggleConformalMesh);
		InMenuBuilder.AddMenuEntry(Commands.ToggleRig);

		if (IsUsingFootageData())
		{
			InMenuBuilder.AddMenuEntry(BaseCommands.ToggleDepthMesh);
		}
	}
	InMenuBuilder.EndSection();

	if (IsUsingFootageData())
	{
		InMenuBuilder.BeginSection(TEXT("ChannelsExtensionHook"), LOCTEXT("FootageExtensionHook", "Video"));
		{
			InMenuBuilder.AddMenuEntry(BaseCommands.ToggleUndistortion);
		}
		InMenuBuilder.EndSection();
	}
}

void FMetaHumanIdentityAssetEditorToolkit::ExtendMenu()
{
	const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();

	const FName IdentityMenuName = UToolMenus::JoinMenuPaths(GetToolMenuAppName(), TEXT("Identity"));
	const FName SectionName = UToolMenus::JoinMenuPaths(IdentityMenuName, TEXT("DynamicIdentityMenuSection"));

	UToolMenus* ToolMenus = UToolMenus::Get();

	if (!ToolMenus->IsMenuRegistered(IdentityMenuName))
	{
		UToolMenu* IdentityMenu = ToolMenus->RegisterMenu(IdentityMenuName);

		IdentityMenu->AddDynamicSection(SectionName, FNewToolMenuDelegate::CreateLambda([Commands](UToolMenu* InMenu)
			{
				UMetaHumanIdentityAssetEditorContext* Context = InMenu->FindContext<UMetaHumanIdentityAssetEditorContext>();
				if (Context && Context->MetaHumanIdentityAssetEditor.IsValid())
				{
					FMetaHumanIdentityAssetEditorToolkit* MetaHumanIdentityAssetEditor = Context->MetaHumanIdentityAssetEditor.Pin().Get();

					FToolMenuSection& ComponentCreationSection = InMenu->AddSection(TEXT("IdentityMenuComponentCreation"), LOCTEXT("IdentityMenuComponentCreationSection", "Component Creation"));
					{
						ComponentCreationSection.AddSubMenu(TEXT("FromMeshSubMenu"),
							LOCTEXT("FromMeshSubMenuLabel", "Create Components From Mesh"),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetComponentsFromMeshTooltip),
							FNewToolMenuChoice{ FOnGetContent::CreateSP(MetaHumanIdentityAssetEditor , &FMetaHumanIdentityAssetEditorToolkit::MakeAssetPickerForCaptureDataType, UMeshCaptureData::StaticClass()) },
							false,
							FSlateIcon(TEXT("MetaHumanIdentityStyle"), TEXT("Identity.Tools.ComponentsFromMesh"), TEXT("Identity.Tools.ComponentsFromMesh"))
						);

						ComponentCreationSection.AddSubMenu(TEXT("FromFootageSubMenu"),
							LOCTEXT("FromFootageSubMenuLabel", "Create Components From Footage"),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetComponentsFromFootageTooltip),
							FNewToolMenuChoice{ FOnGetContent::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::MakeAssetPickerForCaptureDataType, UFootageCaptureData::StaticClass()) },
							false,
							FSlateIcon(TEXT("MetaHumanIdentityStyle"), TEXT("Identity.Tools.ComponentsFromFootage"), TEXT("Identity.Tools.ComponentsFromFootage"))
						);
					}


					FToolMenuSection& FramesSection = InMenu->AddSection(TEXT("IdentityMenuFrames"), LOCTEXT("IdentityMenuFramesSection", "Frames"));
					{
						FramesSection.AddMenuEntry(
							Commands.PromoteFrame,
							Commands.PromoteFrame->GetLabel(),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetPromoteFrameButtonTooltip),
							Commands.PromoteFrame->GetIcon()
						);
						FramesSection.AddMenuEntry(
							Commands.DemoteFrame,
							Commands.DemoteFrame->GetLabel(),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetDemoteFrameButtonTooltip),
							Commands.DemoteFrame->GetIcon()
						);
					}

					FToolMenuSection& TrackingSection = InMenu->AddSection(TEXT("IdentityMenuTrackers"), LOCTEXT("TrackMenuTrackersSection", "Trackers"));
					{
						TrackingSection.AddMenuEntry
						(
							Commands.TrackCurrent,
							Commands.TrackCurrent->GetLabel(),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetTrackActiveFrameButtonTooltip),
							Commands.TrackCurrent->GetIcon()
						);
					}
					FToolMenuSection& SolveSection = InMenu->AddSection(TEXT("IdentityMenuLocalSolve"), LOCTEXT("IdentityMenuLocalSolveSection", "Local Solve"));
					{
						SolveSection.AddMenuEntry
						(
							Commands.IdentitySolve,
							Commands.IdentitySolve->GetLabel(),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetIdentitySolveButtonTooltip),
							Commands.IdentitySolve->GetIcon()
						);
					}
					FToolMenuSection& MetaHumanServiceSection = InMenu->AddSection(TEXT("IdentityMenuMetaHumanService"), LOCTEXT("IdentityMenuMetaHumanServiceSection", "MetaHuman Service"));
					{
						MetaHumanServiceSection.AddMenuEntry
						(
							Commands.MeshToMetaHumanDNAOnly,
							Commands.MeshToMetaHumanDNAOnly->GetLabel(),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetMeshToMetaHumanDNAOnlyButtonTooltip),
							Commands.MeshToMetaHumanDNAOnly->GetIcon()
						);
					}
					FToolMenuSection& AdjustmentsSection = InMenu->AddSection(TEXT("IdentityMenuAdjustments"), LOCTEXT("IdentityMenuAdjustmentsSection", "Adjustments"));
					{
						AdjustmentsSection.AddMenuEntry
						(
							Commands.FitTeeth,
							Commands.FitTeeth->GetLabel(),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetFitTeethButtonTooltip),
							Commands.FitTeeth->GetIcon()
						);
						AdjustmentsSection.AddMenuEntry
						(
							Commands.PrepareForPerformance,
							Commands.PrepareForPerformance->GetLabel(),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetPrepareForPerformanceButtonTooltip),
							Commands.PrepareForPerformance->GetIcon()
						);
					}

					FToolMenuSection& DNASection = InMenu->AddSection(TEXT("IdentityMenuDNAImportExport"), LOCTEXT("IdentityMenuDNAImportExportSection", "MetaHuman DNA"));
					{
						DNASection.AddMenuEntry
						(
							Commands.ImportDNA,
							Commands.ImportDNA->GetLabel(),
							Commands.ImportDNA->GetDescription(),
							Commands.ImportDNA->GetIcon()
						);
						DNASection.AddMenuEntry
						(
							Commands.ExportDNA,
							Commands.ExportDNA->GetLabel(),
							Commands.ExportDNA->GetDescription(),
							Commands.ExportDNA->GetIcon()
						);
					}

					FToolMenuSection& MeshExportSection = InMenu->AddSection(TEXT("IdentityMeshExport"), LOCTEXT("IdentityMeshExportSection", "Mesh Export"));
					{
						MeshExportSection.AddMenuEntry
						(
							Commands.ExportTemplateMesh,
							Commands.ExportTemplateMesh->GetLabel(),
							Commands.ExportTemplateMesh->GetDescription(),
							Commands.ExportTemplateMesh->GetIcon()
						);
					}

				}
			})
		);
	}

	const FName IdentityMainMenuName = UToolMenus::JoinMenuPaths(GetToolMenuName(), TEXT("Identity"));

	if (!ToolMenus->IsMenuRegistered(IdentityMainMenuName))
	{
		ToolMenus->RegisterMenu(IdentityMainMenuName, IdentityMenuName);
	}

	if (UToolMenu* MainMenu = ToolMenus->ExtendMenu(GetToolMenuName()))
	{
		const FToolMenuInsert MenuInsert{ TEXT("Tools"), EToolMenuInsertType::After };

		FToolMenuSection& Section = MainMenu->FindOrAddSection(NAME_None);

		FToolMenuEntry& IdentityEntry = Section.AddSubMenu(TEXT("Identity"),
			LOCTEXT("IdentityEditorIdentityMenuLabel", "MetaHuman Identity"),
			LOCTEXT("IdentityEditorIdentityMenuTooltip", "Commands used in MetaHuman Identity workflow"),
			FNewToolMenuChoice{});

		IdentityEntry.InsertPosition = MenuInsert;
	}

	const FName AssetMainMenuName = UToolMenus::JoinMenuPaths(GetToolMenuName(), TEXT("Asset"));
	if (UToolMenu* AssetMenu = ToolMenus->ExtendMenu(AssetMainMenuName))
	{
		FToolMenuSection& Section = AssetMenu->AddSection(TEXT("MetaHumanIdentityAssetActions"), LOCTEXT("MetaHumanIdentityAssetActionsSection", "MetaHuman Identity"));
		Section.AddMenuEntry(Commands.ResetTemplateMesh);		
	}
	AddTemplateToMetaHumanToAssetMenu();
}

void FMetaHumanIdentityAssetEditorToolkit::ExtendToolBar()
{
	const FName MainToolbarMenuName = GetToolMenuToolbarName();
	const FName SectionName = UToolMenus::JoinMenuPaths(MainToolbarMenuName, TEXT("DynamicToolbarSection"));

	if (UToolMenu* ToolBarMenu = UToolMenus::Get()->ExtendMenu(MainToolbarMenuName))
	{
		// Define the dynamic section only once and use the UMetaHumanIdentityAssetEditorContext 
		// to get the state of the open asset
		if (!ToolBarMenu->FindSection(SectionName))
		{
			ToolBarMenu->AddDynamicSection(SectionName, FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
				{
					const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();
					UMetaHumanIdentityAssetEditorContext* Context = InMenu->FindContext<UMetaHumanIdentityAssetEditorContext>();
					if (Context && Context->MetaHumanIdentityAssetEditor.IsValid())
					{
						FMetaHumanIdentityAssetEditorToolkit* MetaHumanIdentityAssetEditor = Context->MetaHumanIdentityAssetEditor.Pin().Get();

						FToolMenuSection& IdentityToolsSection = InMenu->AddSection(TEXT("MetaHumanIdentityTools"));
						{
							const bool bSimpleComboBox = false;
							IdentityToolsSection.AddEntry(FToolMenuEntry::InitComboButton(
								TEXT("CreateComponentsToolButton"),
								FUIAction{ FExecuteAction{}, FCanExecuteAction::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::CanCreateComponents) },
								FNewToolMenuDelegate::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::MakeCreateComponentsMenu),
								LOCTEXT("CreateComponentsToolButtonLabel", "Create Components"),
								TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetCreateComponentsToolbarComboTooltip),
								FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("MetaHuman Identity.Toolbar.CreateComponents") },
								bSimpleComboBox));
							IdentityToolsSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.PromoteFrame,
								Commands.PromoteFrame->GetLabel(),
								TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetPromoteFrameButtonTooltip),
								FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("MetaHuman Identity.Toolbar.PromoteFrame") }
							));

							IdentityToolsSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.TrackCurrent,
								Commands.TrackCurrent->GetLabel(),
								TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetTrackActiveFrameButtonTooltip),
								FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("MetaHuman Identity.Toolbar.TrackCurrent") }
							));

							IdentityToolsSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.IdentitySolve,
								Commands.IdentitySolve->GetLabel(),
								TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetIdentitySolveButtonTooltip),
								FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("MetaHuman Identity.Toolbar.IdentitySolve") }
							));
						}

						FToolMenuSection& AutoRiggingSection = InMenu->AddSection(TEXT("AutoRigging"));
						{
							AutoRiggingSection.AddEntry(FToolMenuEntry::InitToolBarButton(
								Commands.MeshToMetaHumanDNAOnly,
								Commands.MeshToMetaHumanDNAOnly->GetLabel(),
								TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetMeshToMetaHumanButtonTooltip),
								FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("MetaHuman Identity.Toolbar.MeshToMetaHuman") }
							));
						}
						FToolMenuSection& FitTeethSection = InMenu->AddSection(TEXT("FitTeeth"));
						{
							FitTeethSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.FitTeeth,
								Commands.FitTeeth->GetLabel(),
								TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetFitTeethButtonTooltip),
								FSlateIcon{ FMetaHumanIdentityStyle::Get().GetStyleSetName(), TEXT("MetaHuman Identity.Toolbar.FitTeeth") }
							));
						}
						FToolMenuSection& PrepareForPerformanceSection = InMenu->AddSection(TEXT("PrepareForPerformance"));
						{
							PrepareForPerformanceSection.AddEntry(
								FToolMenuEntry::InitToolBarButton(
									Commands.PrepareForPerformance,
									Commands.PrepareForPerformance->GetLabel(),
									TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetPrepareForPerformanceButtonTooltip),
									FSlateIcon(FMetaHumanIdentityStyle::Get().GetStyleSetName(), "MetaHuman Identity.Toolbar.PrepareForPerformance", "MetaHuman Identity.PrepareForPerformance")
								)
							);
						}

						//NOTE: using Warning Triangle Widget directly here will crash, because Identity being edited could be closed and another one opened with the same widget meanwhile
						//Instead, we use MetaHumanIdentityAssetEditor->WarningTriangleWidget, which we got a bit above from the Context
						MetaHumanIdentityAssetEditor->WarningTriangleWidget = SNew(SScaleBox)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.WarningWithColor"))
								.ToolTipText(TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetIdentityInvalidationWarningIconTooltip))
								.Visibility(TAttribute<EVisibility>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetIdentityInvalidationWarningIconVisibility))
							];

						FText IconName = FText::FromString(TEXT("Invalidated"));
						//NOTE: the following method is obscuring the ToolTip type for the entry (TAttribute<FText>, which is exactly what we need to be able to change the tooltip text dynamically) - the optional argument
						//for the tooltip in InitWidget is const FText; luckily, we can bypass this by directly accessing the ToolTip after creation
						FToolMenuEntry Entry = FToolMenuEntry::InitWidget(
							"Invalidation",
							MetaHumanIdentityAssetEditor->WarningTriangleWidget.ToSharedRef(),
							IconName,
							false,
							true,
							false
							//tooltip - doesn't work here, as the widget takes precedence, so the tooltip has to go into the SImage widget
						);

						FToolMenuSection& IdentityInvalidationSection = InMenu->AddSection(TEXT("IdentityInvalidation"));
						{
							IdentityInvalidationSection.AddEntry(
								Entry
							);
						}

					}
				})
			);
		}
	}
}

// TODO: This feature is now disabled. Fix or extract any code related to warning triangle in future engine version
EVisibility FMetaHumanIdentityAssetEditorToolkit::GetIdentityInvalidationWarningIconVisibility() const
{
	return EVisibility::Hidden;
	//FText InvalidatedText = IdentityStateValidator->GetInvalidationStateToolTip();
	//EVisibility Visibility = InvalidatedText.IsEmpty () ? EVisibility::Hidden : EVisibility::Visible;
	//return Visibility;
}

FText FMetaHumanIdentityAssetEditorToolkit::GetIdentityInvalidationWarningIconTooltip() const
{
	return IdentityStateValidator->GetInvalidationStateToolTip();
}

bool FMetaHumanIdentityAssetEditorToolkit::CanCreateComponents() const
{
	return Identity->FindPartOfClass<UMetaHumanIdentityFace>() == nullptr;
}

FText FMetaHumanIdentityAssetEditorToolkit::GetCreateComponentsToolbarComboTooltip() const
{
	FText TooltipText = LOCTEXT("CreateComponentsToolbarButtonTooltip", "Create a Face part from a mesh or footage with a Neutral Pose and Body");
	if (Identity->FindPartOfClass<UMetaHumanIdentityFace>() != nullptr)
	{
		return FText::Format(LOCTEXT("CreateComponentsToolbarButtonNoFaceTooltip", "{0}\n\nTo enable this option, delete the existing Face Part in the MetaHuman Identity Treeview"), TooltipText);
	}
	else
	{
		return TooltipText;
	}
}

FText FMetaHumanIdentityAssetEditorToolkit::GetPromoteFrameButtonTooltip() const
{
	return PromotedFramesEditorWidget->GetPromoteFrameButtonTooltip();
}

FText FMetaHumanIdentityAssetEditorToolkit::GetDemoteFrameButtonTooltip() const
{
	return PromotedFramesEditorWidget->GetDemoteFrameButtonTooltip();
}

FText FMetaHumanIdentityAssetEditorToolkit::GetTrackActiveFrameButtonTooltip() const
{
	UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = nullptr;
	if (PromotedFramesEditorWidget.IsValid())
	{
		SelectedPromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame();
	}

	return FMetaHumanIdentityTooltipProvider::GetTrackActiveFrameButtonTooltip(Identity, SelectedIdentityPose, SelectedPromotedFrame);
}

FText FMetaHumanIdentityAssetEditorToolkit::GetIdentitySolveButtonTooltip() const
{
	return FMetaHumanIdentityTooltipProvider::GetIdentitySolveButtonTooltip(Identity);
}

FText FMetaHumanIdentityAssetEditorToolkit::GetMeshToMetaHumanButtonTooltip() const
{
	return FMetaHumanIdentityTooltipProvider::GetMeshToMetaHumanButtonTooltip(Identity);
}

FText FMetaHumanIdentityAssetEditorToolkit::GetFitTeethButtonTooltip() const
{
	return FMetaHumanIdentityTooltipProvider::GetFitTeethButtonTooltip(Identity, CanFitTeeth());
}

bool FMetaHumanIdentityAssetEditorToolkit::WarnUnknownDeviceModelDialog() const
{
	FSuppressableWarningDialog::FSetupInfo Info(
		LOCTEXT("IdentityWarnUnknownDeviceModelDialog_Message", "The Device Model in the footage has not been set. Default settings will be used and fitting quality may be affected."),
		LOCTEXT("IdentityWarnUnknownDeviceModelDialog_Title", "Unspecified Capture Device"),
		TEXT("IdentityWarnUnknownDeviceModelDialog"));
	Info.ConfirmText = LOCTEXT("IdentityWarnUnknownDeviceModelDialog_ConfirmText", "Continue");
	Info.CancelText = LOCTEXT("IdentityWarnUnknownDeviceModelDialog_CancelText", "Cancel");

	FSuppressableWarningDialog ShouldRecordDialog(Info);
	FSuppressableWarningDialog::EResult UserInput = ShouldRecordDialog.ShowModal();

	return UserInput == FSuppressableWarningDialog::EResult::Cancel ? false : true;
}

TSharedRef<FMetaHumanIdentityViewportClient> FMetaHumanIdentityAssetEditorToolkit::GetMetaHumanIdentityViewportClient() const
{
	return StaticCastSharedPtr<FMetaHumanIdentityViewportClient>(ViewportClient).ToSharedRef();
}

void FMetaHumanIdentityAssetEditorToolkit::CreateSceneCaptureComponent()
{
	// Create the SceneCaptureComponent used to read the scene as a texture for tracking camera frames
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
	RenderTarget->InitAutoFormat(UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.X, UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.Y);
	RenderTarget->UpdateResourceImmediate(false);

	SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	SceneCaptureComponent->TextureTarget = RenderTarget;
	SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;
	SceneCaptureComponent->bAlwaysPersistRenderingState = true;
	SceneCaptureComponent->PostProcessSettings = FMetaHumanEditorViewportClient::GetDefaultPostProcessSettings();

	ViewportClient->GetPreviewScene()->AddComponent(SceneCaptureComponent, FTransform::Identity);
}

void FMetaHumanIdentityAssetEditorToolkit::LoadGenericFaceContourTracker()
{
	UMetaHumanFaceContourTrackerAsset* Tracker = nullptr;
	if (GetMutableDefault<UMetaHumanEditorSettings>()->bLoadTrackersOnStartup && Identity->GetMetaHumanAuthoringObjectsPresent() && FMetaHumanSupportedRHI::IsSupported())
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
			{
				if (NeutralPose->IsDefaultTrackerValid())
				{
					Tracker = NeutralPose->DefaultTracker.Get();
				}
			}
		}
		if (!Tracker)
		{
			// If neutral pose promoted frames were not found, load generic default tracker.
			static constexpr const TCHAR* GenericTrackerPath = TEXT("/" UE_PLUGIN_NAME "/GenericTracker/GenericFaceContourTracker.GenericFaceContourTracker");
			Tracker = LoadObject<UMetaHumanFaceContourTrackerAsset>(GetTransientPackage(), GenericTrackerPath);
		}
		const bool bShowProgress = true;
		Tracker->LoadTrackers(bShowProgress, [=](bool bTrackersLoaded)
		{
			if (!bTrackersLoaded)
			{
				UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Failed to load trackers"));
			}
		});
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleIdentityTreeSelectionChanged(UObject* InObject, EIdentityTreeNodeIdentifier InNodeIdentifier)
{
	DetailsView->SetObject(InObject);

	SelectedIdentityPose = Cast<UMetaHumanIdentityPose>(InObject);

	if (PromotedFramesEditorWidget.IsValid())
	{
		PromotedFramesEditorWidget->SetIdentityPose(SelectedIdentityPose.IsValid() ? SelectedIdentityPose.Get() : nullptr);
	}

	UpdateTimelineTabVisibility(IsUsingFootageData());
	UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>();

	if (SelectedIdentityPose.IsValid())
	{
		ClearMediaTracks();

		if (SelectedIdentityPose->GetCaptureData())
		{
			if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(SelectedIdentityPose->GetCaptureData()))
			{
				UpdatedViewportForCaptureData(FootageCaptureData, SelectedIdentityPose->TimecodeAlignment, SelectedIdentityPose->Camera);
			}
		}

		if (Face)
		{
			Face->ShowHeadMeshForPose(SelectedIdentityPose->PoseType);
		}
	}

	// only show teeth mesh for the teeth pose selection in the tree
	if (Face && Face->TemplateMeshComponent)
	{
		if (SelectedIdentityPose.IsValid() && SelectedIdentityPose->PoseType == EIdentityPoseType::Teeth)
		{
			Face->TemplateMeshComponent->SetTeethMeshVisibility(true);
		}
		else
		{
			Face->TemplateMeshComponent->SetTeethMeshVisibility(false);
		}
	}


	if (SelectedIdentityPose.IsValid() && IsUsingFootageData())
	{
		TimelineSequencer->SetGlobalTime(Identity->ViewportSettings->GetFrameTimeForPose(SelectedIdentityPose->PoseType));
	}

	TSharedRef<FMetaHumanIdentityViewportClient> IdentityViewportClient = GetMetaHumanIdentityViewportClient();
	IdentityViewportClient->ResetABWipePostion();
	IdentityViewportClient->UpdateABVisibility();
}

void FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameTrackingModeChanged(class UMetaHumanIdentityPromotedFrame* InPromotedFrame)
{
	if (PromotedFramesEditorWidget.IsValid()
		&& InPromotedFrame == PromotedFramesEditorWidget->GetSelectedPromotedFrame()
		&& InPromotedFrame->IsTrackingOnChange())
	{
		HandleCameraStopped();
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameSelectedInPromotedFramesPanel(UMetaHumanIdentityPromotedFrame* InPromotedFrame, bool bForceNotify)
{
	if (InPromotedFrame != nullptr)
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (SelectedIdentityPose.IsValid())
			{
				int32 PromotedFrameIndex = SelectedIdentityPose->PromotedFrames.IndexOfByKey(InPromotedFrame);

				if (OutlinerWidget.IsValid())
				{
					OutlinerWidget->SetPromotedFrame(InPromotedFrame, PromotedFrameIndex, SelectedIdentityPose->PoseType);
				}

				if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(InPromotedFrame))
				{
					if (UpdatePromotedFrameTexture(FootageFrame->FrameNumber))
					{
						UMetaHumanFootageComponent* FootageComponent = Cast<UMetaHumanFootageComponent>(SelectedIdentityPose->CaptureDataSceneComponent);
						if (UMetaHumanFootageComponent* FootageComponentInstance = Cast<UMetaHumanFootageComponent>(IdentityPartsEditor->GetPrimitiveComponent(FootageComponent, true)))
						{
							if (FootageComponentInstance)
							{
								FootageComponentInstance->SetMediaTextures(PromotedFrameTexture.Key, PromotedFrameTexture.Value);
							}
						}

						// Set the depth texture in the depth mesh component
						SetDepthMeshTexture(PromotedFrameTexture.Value);
					}

					UMovieScene* MovieScene = Sequence->GetMovieScene();
					check(MovieScene);

					FFrameRate TickRate = MovieScene->GetTickResolution();
					FFrameRate SourceRate = MovieScene->GetDisplayRate();

					const FFrameTime FrameTime = FFrameRate::TransformTime(FFrameTime{ FootageFrame->FrameNumber }, SourceRate, TickRate);

					Identity->ViewportSettings->SetFrameTimeForPose(SelectedIdentityPose->PoseType, FrameTime);
				}

				if (InPromotedFrame->bIsHeadAlignmentSet)
				{
					// We have a valid HeadAlignment transform that we can use to update the conformal mesh and rig
					constexpr bool bUpdateRigPosition = true;
					Face->SetTemplateMeshTransform(InPromotedFrame->HeadAlignment, bUpdateRigPosition);
				}

				GetMetaHumanIdentityViewportClient()->RefreshTrackerImageViewer();
			}
		}
	}
	else
	{
		if (OutlinerWidget.IsValid())
		{
			// Clear the outliner tree view
			OutlinerWidget->SetPromotedFrame(nullptr, INDEX_NONE, EIdentityPoseType::Invalid);
		}

		if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
		{
			UMetaHumanFootageComponent* FootageComponent = Cast<UMetaHumanFootageComponent>(Pose->CaptureDataSceneComponent);
			UMetaHumanFootageComponent* FootageComponentInstance = Cast<UMetaHumanFootageComponent>(IdentityPartsEditor->GetPrimitiveComponent(FootageComponent, true));
			if (FootageComponentInstance)
			{
				FootageComponentInstance->SetMediaTextures(ColourMediaTexture, DepthMediaTexture);
			}

			// Restore the depth media texture in the depth mesh component
			SetDepthMeshTexture(DepthMediaTexture);
		}
	}

	GetMetaHumanIdentityViewportClient()->UpdateABVisibility();
}

void FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameAdded(UMetaHumanIdentityPromotedFrame* InPromotedFrame)
{
	FFrameTrackingContourData Contours = GetPoseSpecificContourDataForPromotedFrame(InPromotedFrame, SelectedIdentityPose);
	const FString ConfigVersion = FMetaHumanContourDataVersion::GetContourDataVersionString();
	InPromotedFrame->InitializeMarkersFromParsedConfig(Contours, ConfigVersion);

	if (IsUsingFootageData() && ColourMediaTrack != nullptr)
	{
		FFrameNumber CurrentFrame = GetCurrentFrameNumber();
		UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(InPromotedFrame);
		FootageFrame->FrameNumber = CurrentFrame.Value;

		FFrameTime FrameTime = TimelineSequencer->GetGlobalTime().Time;

		// Key can be added directly in via sequencer track or promoted frame button. Need to avoid infinite loop
		if (!ChannelContainsKey(ColourMediaTrack, FrameTime.GetFrame()))
		{
			UMovieSceneSection* Section = ColourMediaTrack->GetAllSections().Last();
			Section->Modify();
			Section->GetChannelProxy().GetChannels<FMetaHumanMovieSceneChannel>()[0]->GetData().AddKey(FrameTime.GetFrame(), true);
		}

		InPromotedFrame->SetNavigationLocked(true);
		TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

		TArray<FColor> LocalSamples;
		FString DepthFramePath;
		FIntPoint ImageSize;
		if (CaptureSceneForPromotedFrame(InPromotedFrame, ImageSize, LocalSamples, DepthFramePath))
		{
			if (Identity->GetMetaHumanAuthoringObjectsPresent())
			{
				if (FMetaHumanSupportedRHI::IsSupported())
				{
					TrackPromotedFrame(InPromotedFrame, LocalSamples, ImageSize.X, ImageSize.Y, DepthFramePath);
				}
				else
				{
					FSuppressableWarningDialog::FSetupInfo Info(
						FText::Format(LOCTEXT("UnsupportedRHIMessage", "Unable to track the promoted frame with the current RHI. To enable tracking make sure the RHI is set to {0}."), FMetaHumanSupportedRHI::GetSupportedRHINames()),
						LOCTEXT("UnsupportedRHITitle", "Unable to track"),
						TEXT("SupressUnsupportedRHIMessage"));

					Info.ConfirmText = LOCTEXT("OkText", "OK");

					FSuppressableWarningDialog UnsupportedRHIDialog{ Info };
					UnsupportedRHIDialog.ShowModal();
				}
			}
			else
			{
				FSuppressableWarningDialog::FSetupInfo Info(
					LOCTEXT("MissingAuthoringObjectsMessage", "Unable to track the promoted frame since authoring objects are not present"),
					LOCTEXT("MissingAuthoringObjectsTitle", "Unable to track"),
					TEXT("SupressMissingAuthoringObjectsMessage"));

				Info.ConfirmText = LOCTEXT("OkText", "OK");

				FSuppressableWarningDialog MissingAuthoringObjectsDialog{ Info };
				MissingAuthoringObjectsDialog.ShowModal();
			}
		}
	}

/*  NOTE: This handler is called from OnPromotedFrameAdded in PromotedFramesEditor, before Promoted Frame selection is set
	To avoid a wrong PromotedFrame hash being calculated, seting the hash is moved from here to UMetaHumanPromotedFramesEditor::HandleOnAddPromotedFrameClicked*/
}

void FMetaHumanIdentityAssetEditorToolkit::HandlePromotedFrameRemoved(UMetaHumanIdentityPromotedFrame* InPromotedFrame)
{
	TArray<FFrameNumber> OurKeyTimes;
	TArray<FKeyHandle> OurKeyHandles;
	TRange<FFrameNumber> CurrentFrameRange;

	if (UMetaHumanIdentityFootageFrame* Frame = Cast<UMetaHumanIdentityFootageFrame>(InPromotedFrame))
	{
		// Remove the keys from the track only if there are no other promoted frames with the same frame number
		bool bShouldRemoveKeys = false;
		if (PromotedFramesEditorWidget)
		{
			if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
			{
				// Find the promoted frames with the same keyframe as the one being removed
				const int32 FrameNumberToBeRemoved = Frame->FrameNumber;
				TArray<TObjectPtr<UMetaHumanIdentityPromotedFrame>> PromotedFramesWithRemovedFrameNumber = Pose->PromotedFrames.FilterByPredicate(
					[FrameNumberToBeRemoved](const UMetaHumanIdentityPromotedFrame* InPromotedFrame)
					{
						if (const UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(InPromotedFrame))
						{
							return FootageFrame->FrameNumber == FrameNumberToBeRemoved;
						}

						return false;
					});

				bShouldRemoveKeys = PromotedFramesWithRemovedFrameNumber.IsEmpty();

				if (Pose->PromotedFrames.IsEmpty())
				{
					// If the last promoted frame has been removed, reset the head meshes transforms
					if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
					{
						Face->ResetTemplateMeshTransform();
					}
				}
			}
		}

		if (bShouldRemoveKeys && ColourMediaTrack != nullptr)
		{
			const FFrameRate SourceRate = TimelineSequencer->GetRootDisplayRate();
			const FFrameTime TickFrameNumber = FFrameRate::TransformTime(
				FFrameTime{ Frame->FrameNumber }, SourceRate, TimelineSequencer->GetRootTickResolution());

			CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>(TickFrameNumber.FrameNumber));
			CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>(TickFrameNumber.FrameNumber));

			if (!ColourMediaTrack->GetAllSections().IsEmpty())
			{
				UMovieSceneSection* Section = ColourMediaTrack->GetAllSections().Last();
				Section->Modify();

				TArrayView<FMetaHumanMovieSceneChannel*> MediaTrackChannel = Section->GetChannelProxy().GetChannels<FMetaHumanMovieSceneChannel>();
				if (!MediaTrackChannel.IsEmpty())
				{
					TMovieSceneChannelData<bool> ChannelData = MediaTrackChannel.Last()->GetData();
					ChannelData.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);

					MediaTrackChannel.Last()->DeleteKeys(OurKeyHandles);
				}
			}
		}

		/*  NOTE: This handler is called from OnPromotedFrameRemoved in PromotedFramesEditor, before Promoted Frame selection is set
			To avoid a wrong PromotedFrame hash being calculated, seting the hash is moved from here to UMetaHumanPromotedFramesEditor::HandleOnRemovePromotedFrameClicked */
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleCameraStopped()
{
	if (PromotedFramesEditorWidget.IsValid())
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (UMetaHumanIdentityPromotedFrame* PromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame())
			{
				FFrameTrackingContourData Contours = GetPoseSpecificContourDataForPromotedFrame(PromotedFrame, SelectedIdentityPose);
				PromotedFrame->UpdateContourDataFromFrameTrackingContours(Contours);

				if (PromotedFrame->IsTrackingOnChange())
				{
					HandleTrackCurrent();
				}

				GetMetaHumanIdentityViewportClient()->UpdateABVisibility();
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleTrackCurrent()
{
	if (UMetaHumanIdentityPromotedFrame* PromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame())
	{
		// Force the curves, points and neutral pose to be displayed in the currently active view
		TSharedRef<FMetaHumanIdentityViewportClient> IdentityViewportClient = GetMetaHumanIdentityViewportClient();

		if (IdentityViewportClient->IsShowingSingleView())
		{
			const EABImageViewMode ABViewMode = IdentityViewportClient->GetABViewMode();

			// Force undistortion to be unchecked as we want to display the curves
			if (IdentityViewportClient->IsShowingUndistorted(ABViewMode))
			{
				IdentityViewportClient->ToggleDistortion(ABViewMode);
			}

			if (!IdentityViewportClient->IsShowingCurves(ABViewMode))
			{
				IdentityViewportClient->ToggleShowCurves(ABViewMode);
			}

			if (!IdentityViewportClient->IsShowingControlVertices(ABViewMode))
			{
				IdentityViewportClient->ToggleShowControlVertices(ABViewMode);
			}
		}

		// Only create a transaction if we are tracking manually
		const bool bShouldTransact = PromotedFrame->IsTrackingManually();
		const FScopedTransaction Transaction{ UMetaHumanIdentity::IdentityTransactionContext, LOCTEXT("TrackCurrentTransactionLabel", "Track Promoted Frame"), PromotedFrame, bShouldTransact };

		PromotedFrame->Modify();

		TArray<FColor> LocalSamples;
		FString DepthFramePath;
		FIntPoint ImageSize;
		if (CaptureSceneForPromotedFrame(PromotedFrame, ImageSize, LocalSamples, DepthFramePath))
		{
			TrackPromotedFrame(PromotedFrame, LocalSamples, ImageSize.X, ImageSize.Y, DepthFramePath);

			// If tracking manually lock the navigation after tracking
			if (PromotedFrame->IsTrackingManually())
			{
				PromotedFrame->SetNavigationLocked(true);
				GetMetaHumanIdentityViewportClient()->SetNavigationLocked(true);
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleConform()
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (!ActiveCurvesAreValidForConforming())
		{
			const FText MessageText = LOCTEXT("UnableToConformMeshMessage", "Some active curves are placed outside the promoted frame area.");
			const FText TitleText = LOCTEXT("CurvesInvalid", "Unable to solve");
			FMessageDialog::Open(EAppMsgType::Ok, MessageText, TitleText);
			return;
		}

		// Check if the footage data have a valid Device Class set as this affects the config used for solving
		FString ConfigName;
		if (IsUsingFootageData() && !Face->DefaultSolver->GetConfigDisplayName(GetFootageCaptureData(), ConfigName))
		{
			if (!WarnUnknownDeviceModelDialog())
			{
				UE_LOG(LogMetaHumanIdentity, Display, TEXT("Conforming cancelled by user"));
				return;
			}
		}

		Face->Modify();
		
		const EIdentityErrorCode Conformed = Face->Conform();
		if (Conformed != EIdentityErrorCode::None)
		{
			UMetaHumanIdentity::HandleError(Conformed);
			return;
		}

		// Reproject the the points for each Promoted Frame so they are shown on top of the template mesh
		if (Face->bIsConformed)
		{
			if (IdentityPartsEditor.IsValid())
			{
				// Call NotifyMeshChangedUpdate in the Template Mesh that is being displayed so the new conformed mesh is reflected in the viewport
				const bool bInstance = true;
				if (UDynamicMeshComponent* TemplateMeshComponentInstance = Cast<UDynamicMeshComponent>(IdentityPartsEditor->GetSceneComponentOfType(EIdentityTreeNodeIdentifier::TemplateMesh, bInstance)))
				{
					TemplateMeshComponentInstance->NotifyMeshUpdated();
				}
			}

			constexpr bool bUpdateRigTransform = true;
			UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame();
			if (SelectedPromotedFrame && SelectedPromotedFrame->bIsHeadAlignmentSet)
			{
				Face->SetTemplateMeshTransform(SelectedPromotedFrame->HeadAlignment, bUpdateRigTransform);
			}
			else
			{
				// Use the head alignment of the frontal frame
				Face->SetTemplateMeshTransform(Face->GetFrontalViewFrameTransform(), bUpdateRigTransform);
			}

			if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
			{
				UpdateContourDataAfterHeadAlignment(NeutralPose);
			}

			// We need to refresh the viewport visibility of components as they might have moved after conforming but because of
			// caching in the MetaHuman scene capture component they might not get redrawn in the new positions
			GetMetaHumanIdentityViewportClient()->UpdateABVisibility();
		}

		IdentityStateValidator->MeshConformedStateUpdate();
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleResetTemplateMesh()
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		const FScopedTransaction Transaction{ LOCTEXT("ResetTemplateMeshTransaction", "Reset Template Mesh" ) };
		Face->Modify();
		// Reset the conformed state before calling ResetTemplateMesh as ResetTemplateMesh will use this to determine how to reset the transform of the template mesh component
		Face->bIsConformed = false;
		Face->ResetTemplateMesh();
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleTogglePlayback(EABImageViewMode)
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->RigComponent != nullptr)
		{
			USkeletalMeshComponent* PrimitiveComponent = Cast<USkeletalMeshComponent>(IdentityPartsEditor->GetPrimitiveComponent(Face->RigComponent, true));

			if (PrimitiveComponent != nullptr)
			{
				if (PrimitiveComponent->IsPlaying())
				{
					PrimitiveComponent->Stop();
				}
				else
				{
					PrimitiveComponent->Play(true);
				}
			}
		}
	}
}

bool FMetaHumanIdentityAssetEditorToolkit::CanTogglePlayback(EABImageViewMode) const
{
	return true;
}

void FMetaHumanIdentityAssetEditorToolkit::HandleSubmitToAutoRigging()
{
	if (Identity->IsAutoRiggingInProgress())
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Autorigging service is already running for this MetaHuman Identity"));
		return;
	}

	bool bLogOnly = false;
	Identity->CreateDNAForIdentity(bLogOnly);
}

void FMetaHumanIdentityAssetEditorToolkit::HandlePredictiveSolverTraining()
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->RigComponent)
		{
			FOnPredictiveSolversProgress OnProgressCallback;
			FOnPredictiveSolversCompleted OnCompletedCallback;
			// Binding non-weak lambda here because we want this callback to happen always, even when the user closes the toolkit window during cancellation
			OnCompletedCallback.BindLambda([this](FPredictiveSolversResult InResult)
			{
				bool bWasCancelled = false;

				if (UMetaHumanIdentityFace* FaceInner = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
				{
					bWasCancelled = FaceInner->IsAsyncPredictiveSolverTrainingCancelling();
				}

				if (bWasCancelled)
				{
					if (PredictiveSolversTaskProgressNotification.IsValid())
					{
						PredictiveSolversTaskProgressNotification.Pin()->SetText(LOCTEXT("PredictiveSolversTrainingCancelled", "Preparing for Performance cancelled."));
						PredictiveSolversTaskProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);

						PredictiveSolversTaskProgressNotification.Pin()->ExpireAndFadeout();
					}
				}
				else
				{
					bool bSuccess = false;

					// Apply results if successful
					if (InResult.bSuccess)
					{
						if (UMetaHumanIdentityFace* FaceInner = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
						{
							if (FaceInner->RigComponent)
							{
								bSuccess = true;
								FaceInner->SetPredictiveSolvers(InResult.PredictiveSolvers);
								FaceInner->SetPredictiveWithoutTeethSolver(InResult.PredictiveWithoutTeethSolver);

								Identity->MarkPackageDirty();
							}
						}
					}

					// Notification settings
					FNotificationInfo ResultInfo(FText::GetEmpty());
					ResultInfo.bFireAndForget = true;

					SNotificationItem::ECompletionState Status = SNotificationItem::CS_None;

					if (bSuccess)
					{
						ResultInfo.Text = LOCTEXT("PredictiveSolversTrainingCompleted", "Preparing for Performance completed.");
						Status = SNotificationItem::CS_Success;
						
						IdentityStateValidator->MeshPreparedForPerformanceUpdate();
					}
					else
					{
						ResultInfo.Text = LOCTEXT("PredictiveSolversTrainingFailed", "Preparing for Performance failed!");
						Status = SNotificationItem::CS_Fail;
					}

					// Show result notification
					TWeakPtr<class SNotificationItem> SolveNotification = FSlateNotificationManager::Get().AddNotification(ResultInfo);
					SolveNotification.Pin()->SetCompletionState(Status);
					SolveNotification.Pin()->ExpireAndFadeout();
				}
			});

			if (!Face->RunAsyncPredictiveSolverTraining(OnProgressCallback, OnCompletedCallback))
			{
				return;
			}

			// Progress dialog
			{
				FScopedSlowTask Dialog = FScopedSlowTask(100.0f, LOCTEXT("TrainingProgress", "Training the MetaHuman Identity for Performance processing..."));
				float CurrentProgress = 0.0f;
				float PrevProgress = 0.0f;

				Dialog.MakeDialog(true);

				while (true)
				{
					if (!Face->IsAsyncPredictiveSolverTrainingActive())
					{
						break;
					}

					if (Dialog.ShouldCancel())
					{
						Face->CancelAsyncPredictiveSolverTraining();
						break;
					}

					FPlatformProcess::Sleep(0.2f);

					if (Face->PollAsyncPredictiveSolverTrainingProgress(CurrentProgress))
					{
						CurrentProgress *= 100.0f;
					}

					if (CurrentProgress > PrevProgress)
					{
						float ExpectedWorkThisFrame = CurrentProgress - PrevProgress;
						PrevProgress = CurrentProgress;
						Dialog.EnterProgressFrame(ExpectedWorkThisFrame);
					}
					else
					{
						Dialog.TickProgress();
					}
				}
			}

			if (Face->IsAsyncPredictiveSolverTrainingCancelling())
			{
				FNotificationInfo CancelInfo(FText::Format(LOCTEXT("PredictiveSolversTrainingCancelling", "Cancelling preparing {0}..."), FText::FromString(GetNameSafe(Identity))));
				CancelInfo.bFireAndForget = false;

				PredictiveSolversTaskProgressNotification = FSlateNotificationManager::Get().AddNotification(CancelInfo);

				if (PredictiveSolversTaskProgressNotification.IsValid())
				{
					PredictiveSolversTaskProgressNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
				}
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleImportDNA()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	TArray<FString> DNAFilenames, BrowsFilenames;

	if (DesktopPlatform->OpenFileDialog(nullptr, "Select DNA file", "", "", "*.dna", 0, DNAFilenames))
	{
		if (DesktopPlatform->OpenFileDialog(nullptr, "Select Brows file", "", "", "*.json", 0, BrowsFilenames))
		{
			if (DNAFilenames.Num() == 1 && BrowsFilenames.Num() == 1)
			{
				const EIdentityErrorCode ImportDNA = Identity->ImportDNAFile(*DNAFilenames[0], EDNADataLayer::All, *BrowsFilenames[0]);
				if (ImportDNA == EIdentityErrorCode::MLRig)
				{
					const FText MessageText = LOCTEXT("ImportDNAMLRigText", "Selected DNA file contains an ML rig; ML rigs may be used in Identity assets but this functionality is considered experimental.");
					const FText TitleText = LOCTEXT("ImportDNAMLRigTitle", "ML Rig");
					FMessageDialog::Open(EAppMsgType::Ok, MessageText, TitleText);
				}
				else if (ImportDNA != EIdentityErrorCode::None)
				{
					UMetaHumanIdentity::HandleError(ImportDNA, true);

					const FText MessageText = LOCTEXT("ImportDNAIncompatibleText", "The selected DNA file is not compatible with the chosen Skeletal Mesh, please check the UE log for more details.");
					const FText TitleText = LOCTEXT("ImportDNAIncompatibleTitle", "DNA Incompatible");
					FMessageDialog::Open(EAppMsgType::Ok, MessageText, TitleText);
				}

				// We need to refresh the viewport visibility of components as they might have moved but because of
				// caching in the MetaHuman scene capture component they might not get redrawn in the new positions
				GetMetaHumanIdentityViewportClient()->UpdateABVisibility();
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleExportDNA()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	TArray<FString> DNAFilenames, BrowsFilenames;

	if (DesktopPlatform->SaveFileDialog(nullptr, "Select DNA file", "", "", TEXT("DNA File (*.dna)|*.dna"), 0, DNAFilenames))
	{
		if (DesktopPlatform->SaveFileDialog(nullptr, "Select Brows file", "", "", TEXT("Brows data (*.json)|*.json"), 0, BrowsFilenames))
		{
			if (DNAFilenames.Num() == 1 && BrowsFilenames.Num() == 1)
			{
				Identity->ExportDNADataToFiles(DNAFilenames.Last(), BrowsFilenames.Last());
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleFitTeeth()
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		// Check if the footage data have a valid Device Class set as this affects the config used for solving
		FString ConfigName;
		if (IsUsingFootageData() && !Face->DefaultSolver->GetConfigDisplayName(GetFootageCaptureData(), ConfigName))
		{
			if (!WarnUnknownDeviceModelDialog())
			{
				UE_LOG(LogMetaHumanIdentity, Display, TEXT("Teeth fitting cancelled by user"));
				return;
			}
		}
	}

	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		const EIdentityErrorCode FitTeeth = Face->FitTeeth();
		if (FitTeeth != EIdentityErrorCode::None)
		{
			UMetaHumanIdentity::HandleError(FitTeeth);
			return;
		}

		constexpr bool bUpdateRigTransform = true;
		UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame();
		if (SelectedPromotedFrame && SelectedPromotedFrame->bIsHeadAlignmentSet)
		{
			Face->SetTemplateMeshTransform(SelectedPromotedFrame->HeadAlignment, bUpdateRigTransform);
		}
		else
		{
			// Use the head alignment of the frontal frame
			Face->SetTemplateMeshTransform(Face->GetFrontalViewFrameTransform(), bUpdateRigTransform);
		}

		if (SelectedIdentityPose.IsValid())
		{
			Face->ShowHeadMeshForPose(SelectedIdentityPose->PoseType);
		}

		if (UMetaHumanIdentityPose* TeethPose = Face->FindPoseByType(EIdentityPoseType::Teeth))
		{
			UpdateContourDataAfterHeadAlignment(TeethPose);
		}
		
		IdentityStateValidator->TeethFittedUpdate();
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleExportTemplateMeshClicked()
{
	// Initialize SaveAssetDialog config
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SelectDestination", "Select Destination");
	SaveAssetDialogConfig.DefaultPath = "/Game";
	SaveAssetDialogConfig.AssetClassNames.Add(UStaticMesh::StaticClass()->GetClassPathName());
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	const FString SaveObjectPath = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	if (!SaveObjectPath.IsEmpty())
	{
		const FString PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		const FString ObjectName = FPackageName::ObjectPathToObjectName(SaveObjectPath);

		UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>();
		Face->ExportTemplateMesh(PackageName, ObjectName);
	}
}

bool FMetaHumanIdentityAssetEditorToolkit::CanExportTemplateMesh() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return Face->bIsConformed;
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanActivateMarkersForCurrent() const
{
	if (PromotedFramesEditorWidget.IsValid())
	{
		if (UMetaHumanIdentityPromotedFrame* SelectedPromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame())
		{
			return SelectedPromotedFrame->FrameContoursContainActiveData() && !SelectedPromotedFrame->bUseToSolve;
		}
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanActivateMarkersForAll() const
{
	if (PromotedFramesEditorWidget.IsValid())
	{
		if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
		{
			for (UMetaHumanIdentityPromotedFrame* PromotedFrame : Pose->PromotedFrames)
			{
				if (PromotedFrame != nullptr)
				{
					if (PromotedFrame->FrameContoursContainActiveData() && !PromotedFrame->bUseToSolve)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanTrackCurrent() const
{
	if (Identity->IsFrameTrackingPipelineProcessing() || !Identity->GetMetaHumanAuthoringObjectsPresent() || !FMetaHumanSupportedRHI::IsSupported())
	{
		return false;
	}

	if (PromotedFramesEditorWidget.IsValid())
	{
		if (UMetaHumanIdentityPromotedFrame* PromotedFrame = PromotedFramesEditorWidget->GetSelectedPromotedFrame())
		{
			return PromotedFrame->CanTrack();
		}
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanTrackAll() const
{
	if(Identity->IsFrameTrackingPipelineProcessing())
	{
		return false;
	}

	if (PromotedFramesEditorWidget.IsValid())
	{
		if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
		{
			for (UMetaHumanIdentityPromotedFrame* PromotedFrame : Pose->PromotedFrames)
			{
				if (PromotedFrame->CanTrack())
				{
					// If at least one Promoted Frame can be tracked we enable the Track All button
					return true;
				}
			}
		}
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanConform() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return Face->CanConform();
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanResetTemplateMesh() const
{
	// If we can conform we can reset
	return CanConform();
}

bool FMetaHumanIdentityAssetEditorToolkit::CanSubmitToAutoRigging() const
{
	if (Identity->IsAutoRiggingInProgress())
	{
		// Don't allow multiple submissions to the AutoRigging service
		return false;
	}

	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		// Only enables AutoRigging if the Face was conformed successfully
		return Face->CanSubmitToAutorigging();
	}

	// NOTE: if the user is not logged in or has not accepted the EULA the solve request will trigger that flow itself.
	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanImportDNA() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return true;
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanExportDNA() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return Face->HasDNABuffer();
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanFitTeeth() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return Face->CanFitTeeth();
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::CanRunSolverTraining() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return bDepthProcessingEnabled && Face->bIsAutoRigged && !Face->IsAsyncPredictiveSolverTrainingActive() && !Face->IsAsyncPredictiveSolverTrainingCancelling() &&
			Face->DefaultSolver && Face->DefaultSolver->CanProcess();
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::FaceIsConformed() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		return Face->bIsConformed;
	}

	return false;
}

// TODO: Instead of checking for consistency we should set up a filter in customization to only select compatible assets
bool FMetaHumanIdentityAssetEditorToolkit::CaptureDataIsConsistentForPoses(const UCaptureData* InCaptureData) const
{
	bool bSelectedIsMeshData = InCaptureData->IsA<UMeshCaptureData>();

	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		for (EIdentityPoseType PoseType : TEnumRange<EIdentityPoseType>())
		{
			if (UMetaHumanIdentityPose* Pose = Face->FindPoseByType(PoseType))
			{
				if (UCaptureData* CaptureData = Pose->GetCaptureData())
				{
					bool ExistingPoseIsMesh = CaptureData->IsA<UMeshCaptureData>();

					if (bSelectedIsMeshData != ExistingPoseIsMesh)
					{	
						FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("CaptureSource", "Incompatible selection",
							"Selected CaptureData is incompatible. \n Please select CaptureData sources of the same type for all poses"));
						
						SelectedIdentityPose->SetCaptureData(nullptr);
						return false;
					}
				}
			}
		}
	}
	
	return true;
}

FFrameTrackingContourData FMetaHumanIdentityAssetEditorToolkit::GetPoseSpecificContourDataForPromotedFrame(UMetaHumanIdentityPromotedFrame* InPromotedFrame, 
	TWeakObjectPtr<UMetaHumanIdentityPose> InPose, bool bInProjectFootage) const
{
	FFrameTrackingContourData ContourData;

	if (InPose.IsValid())
	{
		EIdentityPoseType PoseType = InPose->PoseType;

		if (UMetaHumanIdentityCameraFrame* CameraFrame = Cast<UMetaHumanIdentityCameraFrame>(InPromotedFrame))
		{
			FMinimalViewInfo ViewInfo = CameraFrame->GetMinimalViewInfo();

			UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>();
			const TMap<EIdentityPartMeshes, TArray<FVector>> Vertices = Face->GetConformalVerticesWorldPos(PoseType);

			FIntRect ViewRect = { 0, 0, UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.X, UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.Y };
			ECurvePresetType CurvePreset = LandmarkConfigHelper->GetCurvePresetFromIdentityPose(PoseType);
			ContourData = LandmarkConfigHelper->ProjectPromotedFrameCurvesOnTemplateMesh(ViewInfo, Vertices, CurvePreset, ViewRect);
		}
		else if (InPromotedFrame->IsA<UMetaHumanIdentityFootageFrame>())
		{
			if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(InPose->GetCaptureData()))
			{
				ECurvePresetType CurvePreset = LandmarkConfigHelper->GetCurvePresetFromIdentityPose(PoseType);

				if (bInProjectFootage)
				{
					FMinimalViewInfo ViewInfo;
					SceneCaptureComponent->GetCameraView(0, ViewInfo);

					FVector2D WidgetSize = GetMetaHumanIdentityViewportClient()->GetWidgetSize();
					FIntRect ViewRect = FIntRect(0, 0, WidgetSize.X, WidgetSize.Y);
					ViewInfo.AspectRatio = WidgetSize.X / WidgetSize.Y;
					ViewInfo.FOV = ViewportClient->ViewFOV;

					UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>();
					FTransform PoseMeshTransform = InPromotedFrame->HeadAlignment;
					const TMap<EIdentityPartMeshes, TArray<FVector>> Vertices = Face->GetConformalVerticesForTransform(PoseMeshTransform, PoseType);
					ContourData = LandmarkConfigHelper->ProjectPromotedFrameCurvesOnTemplateMesh(ViewInfo, Vertices, CurvePreset, ViewRect);

					// Convert all points from widget space to texture space
					for (TPair<FString, FTrackingContour>& Data : ContourData.TrackingContours)
					{
						for (FVector2D& Point : Data.Value.DensePoints)
						{
							Point = GetMetaHumanIdentityViewportClient()->GetPointPositionOnImage(Point);
						}
					}
				}
				else
				{
					const FIntPoint TextureResolution = FootageCaptureData->GetFootageColorResolution();
					ContourData = LandmarkConfigHelper->GetDefaultContourDataFromConfig(FVector2D(TextureResolution.X, TextureResolution.Y), CurvePreset);
				}				
			}
		}
	}

	return ContourData;
}

const TSharedPtr<SMetaHumanIdentityPartsEditor> FMetaHumanIdentityAssetEditorToolkit::GetIdentityPartsEditor() const
{
	return IdentityPartsEditor;
}

bool FMetaHumanIdentityAssetEditorToolkit::ActiveCurvesAreValidForConforming() const
{
	FBox2D TexCanvas = FBox2D(FVector2D(0,0), FVector2D(UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.X, UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize.Y));
	if (IsUsingFootageData())
	{
		const UFootageCaptureData* FootageData = Cast<UFootageCaptureData>(GetAvailableCaptureDataFromExistingPoses());
		const FIntPoint TextureResolution = FootageData->GetFootageColorResolution();
		TexCanvas = FBox2D(FVector2D(0, 0), FVector2D(TextureResolution.X, TextureResolution.Y));
	}

	if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
	{
		for (const UMetaHumanIdentityPromotedFrame* PromotedFrame : Pose->PromotedFrames)
		{
			if (!PromotedFrame->AreActiveCurvesValidForConforming(TexCanvas))
			{
				return false;
			}
		}
	}

	return true;
}

void FMetaHumanIdentityAssetEditorToolkit::TrackPromotedFrame(UMetaHumanIdentityPromotedFrame* InPromotedFrame, const TArray<FColor>& InImageData, int32 InWidth, int32 InHeight, const FString& InDepthFramePath)
{
	if (!InPromotedFrame->ContourTracker || Identity->IsFrameTrackingPipelineProcessing() || !Identity->GetMetaHumanAuthoringObjectsPresent() || !FMetaHumanSupportedRHI::IsSupported())
	{
		return;
	}

	const bool bShowProgress = true;
	Identity->StartFrameTrackingPipeline(InImageData, InWidth, InHeight, InDepthFramePath, SelectedIdentityPose.Get(), InPromotedFrame, bShowProgress);
}

bool FMetaHumanIdentityAssetEditorToolkit::CaptureSceneForPromotedFrame(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, FIntPoint& OutImageSize, TArray<FColor>& OutLocalSamples, FString & OutDepthFramePath)
{
	if (UMetaHumanIdentityCameraFrame* CameraFrame = Cast<UMetaHumanIdentityCameraFrame>(InPromotedFrame))
	{
		OutDepthFramePath = FString{};

		// Hide components that shouldn't appear in the captured screenshot
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			constexpr bool bInstance = true;
			if (UPrimitiveComponent* TemplateMeshComponentInstance = IdentityPartsEditor->GetPrimitiveComponent(Face->TemplateMeshComponent, bInstance))
			{
				SceneCaptureComponent->HideComponent(TemplateMeshComponentInstance);

				// For the template mesh we need to add all child components as the scene capture components doesn't work with sub components
				constexpr bool bIncludeAllDescendants = true;
				TArray<USceneComponent*> ChildComponents;
				TemplateMeshComponentInstance->GetChildrenComponents(bIncludeAllDescendants, ChildComponents);

				for (USceneComponent* ChildComponent : ChildComponents)
				{
					if (UPrimitiveComponent* ChildPrimitiveComponent = Cast<UPrimitiveComponent>(ChildComponent))
					{
						SceneCaptureComponent->HideComponent(ChildPrimitiveComponent);
					}
				}
			}

			if (UPrimitiveComponent* RigComponentInstance = IdentityPartsEditor->GetPrimitiveComponent(Face->RigComponent, bInstance))
			{
				SceneCaptureComponent->HideComponent(RigComponentInstance);
			}
		}

		// Set the camera transform in the scene capture component
		SceneCaptureComponent->FOVAngle = CameraFrame->CameraViewFOV;
		SceneCaptureComponent->SetWorldTransform(CameraFrame->GetCameraTransform());

		// Recreate the ShowFlags for the scene capture component to avoid getting in a state where flags are not reset properly
		SceneCaptureComponent->ShowFlags = FEngineShowFlags{ ESFIM_Editor };
		SceneCaptureComponent->ShowFlags.SetSelectionOutline(GetDefault<ULevelEditorViewportSettings>()->bUseSelectionOutline);
		SceneCaptureComponent->ShowFlags.SetAntiAliasing(false);

		// Apply the ViewMode from the ViewportClient to make sure the capture is consistent with what is in the view
		const EViewModeIndex ViewMode = CameraFrame->ViewMode;
		ensure(ViewMode == EViewModeIndex::VMI_Lit || ViewMode == EViewModeIndex::VMI_Unlit || ViewMode == EViewModeIndex::VMI_LightingOnly); // Scene capture component does not support other modes
		const bool bCanDisableToneMapping = false;
		EngineShowFlagOverride(ESFIM_Editor, ViewMode, SceneCaptureComponent->ShowFlags, bCanDisableToneMapping);

		// Set the post process settings in the scene capture component to match what we are seeing on screen
		SceneCaptureComponent->PostProcessSettings = GetMetaHumanIdentityViewportClient()->GetPostProcessSettingsForCurrentView();

		SceneCaptureComponent->CaptureScene();
		SceneCaptureComponent->ClearHiddenComponents();

		if (UKismetRenderingLibrary::ReadRenderTarget(SceneCaptureComponent->TextureTarget, SceneCaptureComponent->TextureTarget, OutLocalSamples))
		{
			OutImageSize = UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize;

			return true;
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to read image for tracking from Promoted Frame '%s'"), *InPromotedFrame->GetName());
		}
	}
	else if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(InPromotedFrame))
	{
		UFootageCaptureData* FootageCaptureData = GetFootageCaptureData();
		if (FootageCaptureData)
		{
			UFootageCaptureData::FVerifyResult Result = FootageCaptureData->VerifyData(UCaptureData::EInitializedCheck::Full);

			if (!Result.HasError())
			{
				FString ImagePath = UPromotedFrameUtils::GetImagePathForFrame(FootageCaptureData, GetCamera(), FootageFrame->FrameNumber, true /* image sequence */, GetTimecodeAlignment());
				OutDepthFramePath = UPromotedFrameUtils::GetImagePathForFrame(FootageCaptureData, GetCamera(), FootageFrame->FrameNumber, false /* depth sequence */, GetTimecodeAlignment());

				if (!ImagePath.IsEmpty())
				{
					return UPromotedFrameUtils::GetPromotedFrameAsPixelArrayFromDisk(ImagePath, OutImageSize, OutLocalSamples);
				}
			}
			else
			{
				UE_LOG(LogMetaHumanIdentity, Error, TEXT("Footage Capture Data asset doesn't contain valid data: '%s'"), *Result.StealError());
			}
		}
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::UpdatePromotedFrameTexture(const FFrameNumber& InFrameNumber)
{	
	bool bSuccess = false;
	FString FilePath;
	if (PopulateImageTextureFromDisk(InFrameNumber, FilePath))
	{
		if (PopulateDepthTextureFromDisk(InFrameNumber, FilePath))
		{
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to load the depth texture: '%s'"), *FilePath);
		}
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to load the image texture: '%s'"), *FilePath);
	}

	return bSuccess;
}

bool FMetaHumanIdentityAssetEditorToolkit::PopulateImageTextureFromDisk(const FFrameNumber& InFrameNumber, FString& OutTexturePath)
{
	UFootageCaptureData* FootageCaptureData = GetFootageCaptureData();
	if (FootageCaptureData)
	{
		UFootageCaptureData::FVerifyResult Result = FootageCaptureData->VerifyData(UCaptureData::EInitializedCheck::Full);

		if (!Result.HasError())
		{
			const bool bIsImageSequence = true;
			OutTexturePath = UPromotedFrameUtils::GetImagePathForFrame(FootageCaptureData, GetCamera(), InFrameNumber.Value, bIsImageSequence, GetTimecodeAlignment());

			if (!OutTexturePath.IsEmpty())
			{
				if (UTexture2D* LoadedTex = UPromotedFrameUtils::GetBGRATextureFromFile(OutTexturePath))
				{
					PromotedFrameTexture.Key = LoadedTex;
					return true;
				}
			}
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Footage Capture Data asset doesn't contain valid data: '%s'"), *Result.StealError());
		}
	}

	PromotedFrameTexture.Key = nullptr;
	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::PopulateDepthTextureFromDisk(const FFrameNumber& InFrameNumber, FString& OutTexturePath)
{
	bool bSuccess = false;

	UFootageCaptureData* FootageCaptureData = GetFootageCaptureData();
	if (FootageCaptureData)
	{
		UFootageCaptureData::FVerifyResult Result = FootageCaptureData->VerifyData(UCaptureData::EInitializedCheck::Full);

		if (!Result.HasError())
		{
			const bool bIsImageSequence = false;
			OutTexturePath = UPromotedFrameUtils::GetImagePathForFrame(FootageCaptureData, GetCamera(), InFrameNumber.Value, bIsImageSequence, GetTimecodeAlignment());

			if (!OutTexturePath.IsEmpty())
			{
				PromotedFrameTexture.Value = UPromotedFrameUtils::GetDepthTextureFromFile(OutTexturePath);
				bSuccess = PromotedFrameTexture.Value != nullptr;
			}
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Footage Capture Data asset doesn't contain valid data: '%s'"), *Result.StealError());
		}
	}

	return bSuccess;
}

void FMetaHumanIdentityAssetEditorToolkit::MakeMeshAssetPickerMenu(UToolMenu* InToolMenu, TFunction<void(const FAssetData& InAssetData)> InCallbackFunction) const
{
	if (Identity->FindPartOfClass<UMetaHumanIdentityFace>() != nullptr)
	{
		TSharedRef<SWidget> WarningMessageBox = SNew(SBox)
			.Padding(FMargin{ 0.0f, 4.0f })
			[
				SNew(SWarningOrErrorBox)
				.MessageStyle(EMessageStyle::Warning)
				.Message(LOCTEXT("CantSelectMeshMessage", "This MetaHuman Identity already has a Face part. Remove it first to use this functionality"))
			];

		// If we have a Face already display a message to the user
		InToolMenu->AddMenuEntry(TEXT("CantSelectMesh"), FToolMenuEntry::InitMenuEntry(TEXT("CantSelectMesh"), FToolUIActionChoice{}, WarningMessageBox));
	}
	else
	{
		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get();

		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;

		AssetPickerConfig.Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		AssetPickerConfig.Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());

		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

		auto HandleAssetSelected = [InCallbackFunction](const FAssetData& InAssetData)
		{
			InCallbackFunction(InAssetData);

			FSlateApplication::Get().DismissAllMenus();
		};

		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(HandleAssetSelected);

		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateLambda([HandleAssetSelected](const TArray<FAssetData>& InAssetDataList)
		{
			if (!InAssetDataList.IsEmpty())
			{
				HandleAssetSelected(InAssetDataList[0].GetAsset());
			}
		});

		TSharedRef<SWidget> AssetPicker = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(400.0f)
			.Padding(FMargin{ 10.0f })
			[
				ContentBrowser.CreateAssetPicker(AssetPickerConfig)
			];

		InToolMenu->AddMenuEntry(TEXT("SelectMeshMenu"), FToolMenuEntry::InitMenuEntry(TEXT("MeshAssetPicker"), FToolUIActionChoice{}, AssetPicker));
	}
}

TSharedRef<SWidget> FMetaHumanIdentityAssetEditorToolkit::MakeAssetPickerForCaptureDataType(UClass* InCaptureDataClass)
{
	if (!CanCreateComponents())
	{
		return SNew(SBox)
			.Padding(FMargin{ 0.0f, 4.0f })
			[
				SNew(SWarningOrErrorBox)
				.MessageStyle(EMessageStyle::Warning)
				.Message(LOCTEXT("CantCreateComponentsMessage", "This MetaHuman Identity already has a Face part. Remove it first to use this functionality"))
			];
	}

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get();

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;

	if (InCaptureDataClass->IsChildOf<UMeshCaptureData>())
	{
		// For mesh capture data we filter for static and skeletal meshes
		AssetPickerConfig.Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		AssetPickerConfig.Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	}
	else
	{
		// For footage we filter for all available footage capture data
		AssetPickerConfig.Filter.ClassPaths.Add(InCaptureDataClass->GetClassPathName());
	}

	AssetPickerConfig.Filter.bRecursiveClasses = true;
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	auto HandleAssetSelected = [this](const FAssetData& InAssetData)
	{
		IdentityPartsEditor->AddPartsFromAsset(InAssetData.GetAsset());

		FSlateApplication::Get().DismissAllMenus();
	};

	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(HandleAssetSelected);

	AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateLambda([HandleAssetSelected](const TArray<FAssetData>& InAssetDataList)
	{
		if (!InAssetDataList.IsEmpty())
		{
			HandleAssetSelected(InAssetDataList[0].GetAsset());
		}
	});

	return SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(400.0f)
		.Padding(FMargin{ 10.0f })
		[
			ContentBrowser.CreateAssetPicker(AssetPickerConfig)
		];
}

void FMetaHumanIdentityAssetEditorToolkit::MakeCreateComponentsMenu(UToolMenu* InToolMenu)
{
	FToolMenuSection& CreateComponentsSection = InToolMenu->AddSection(TEXT("CreateComponentsSection"), LOCTEXT("CreateComponentsSection", "Create Components"));
	{
		CreateComponentsSection.AddSubMenu(TEXT("FromMeshSubMenu"),
											LOCTEXT("ComponentsFromMeshSubMenuLabel", "From Mesh"),
											TAttribute<FText>::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::GetComponentsFromMeshTooltip),
											FNewToolMenuChoice{ FOnGetContent::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::MakeAssetPickerForCaptureDataType, UMeshCaptureData::StaticClass()) },
											false,
											FSlateIcon(TEXT("MetaHumanIdentityStyle"), TEXT("Identity.Tools.ComponentsFromMesh"), TEXT("Identity.Tools.ComponentsFromMesh"))
		);

		CreateComponentsSection.AddSubMenu(TEXT("FromFootageSubMenu"),
											LOCTEXT("ComponentsFromFootageSubMenuLabel", "From Footage"),
											TAttribute<FText>::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::GetComponentsFromFootageTooltip),
											FNewToolMenuChoice{ FOnGetContent::CreateSP(this, &FMetaHumanIdentityAssetEditorToolkit::MakeAssetPickerForCaptureDataType, UFootageCaptureData::StaticClass()) },
											false,
											FSlateIcon(TEXT("MetaHumanIdentityStyle"), TEXT("Identity.Tools.ComponentsFromFootage"), TEXT("Identity.Tools.ComponentsFromFootage"))
		);
	}
}

FText FMetaHumanIdentityAssetEditorToolkit::GetComponentsFromMeshTooltip() const
{
	if (CanCreateComponents())
	{
		return LOCTEXT("FromMeshSubMenuTooltip", "Create all the required components for this MetaHuman Identity from a Static or Skeletal Mesh");
	}
	else
	{
		return LOCTEXT("FromMeshSubMenuTooltipDisabled", "Remove existing Face component to enable this option");
	}
}

FText FMetaHumanIdentityAssetEditorToolkit::GetComponentsFromFootageTooltip() const
{
	if (CanCreateComponents())
	{
		return LOCTEXT("FromFootageSubMenuTooltip", "Create all the required components for this MetaHuman Identity from a Capture Data (Footage)");
	}
	else
	{
		return LOCTEXT("FromFootageSubMenuTooltipDisabled", "Remove existing Face component to enable this option");
	}
}

bool FMetaHumanIdentityAssetEditorToolkit::IsUsingFootageData() const
{
	if (UCaptureData* CaptureData = GetAvailableCaptureDataFromExistingPoses())
	{
		return CaptureData->IsA<UFootageCaptureData>();
	}

	return false;
}

bool FMetaHumanIdentityAssetEditorToolkit::IsUsingMeshData() const
{
	if (UCaptureData* CaptureData = GetAvailableCaptureDataFromExistingPoses())
	{
		return CaptureData->IsA<UMeshCaptureData>();
	}

	return false;
}

UFootageCaptureData* FMetaHumanIdentityAssetEditorToolkit::GetFootageCaptureData() const
{
	if (UCaptureData* CaptureData = GetAvailableCaptureDataFromExistingPoses())
	{
		if (UFootageCaptureData* FootageData = Cast<UFootageCaptureData>(CaptureData))
		{
			return FootageData;
		}
	}

	return nullptr;
}

ETimecodeAlignment FMetaHumanIdentityAssetEditorToolkit::GetTimecodeAlignment() const
{
	if (const UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData())
	{
		return Pose->TimecodeAlignment;
	}

	return ETimecodeAlignment::None;
}

FString FMetaHumanIdentityAssetEditorToolkit::GetCamera() const
{
	if (const UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData())
	{
		return Pose->Camera;
	}

	return TEXT("");
}

UCaptureData* FMetaHumanIdentityAssetEditorToolkit::GetAvailableCaptureDataFromExistingPoses() const
{
	if (const UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData())
	{
		if (Pose->GetCaptureData() != nullptr)
		{
			return Pose->GetCaptureData();
		}
	}
	
	return nullptr;
}

UMetaHumanIdentityPose* FMetaHumanIdentityAssetEditorToolkit::GetAvailablePoseWithCaptureData() const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		// Check selected pose first
		if (SelectedIdentityPose.IsValid() && SelectedIdentityPose->GetCaptureData())
		{
			return SelectedIdentityPose.Get();
		}

		for (EIdentityPoseType PoseType : TEnumRange<EIdentityPoseType>())
		{
			if (UMetaHumanIdentityPose* Pose = Face->FindPoseByType(PoseType))
			{
				if (Pose->GetCaptureData() != nullptr)
				{
					return Pose;
				}
			}
		}
	}
	return nullptr;
}

void FMetaHumanIdentityAssetEditorToolkit::SetUpEditorForCaptureDataType()
{
	bool bShowTimeline = false;

	if (UCaptureData* CaptureData = GetAvailableCaptureDataFromExistingPoses())
	{
		UpdatedViewportForCaptureData(CaptureData, GetTimecodeAlignment(), GetCamera());

		if (CaptureData->IsA<UFootageCaptureData>())
		{
			GetMetaHumanIdentityViewportClient()->UpdateABVisibility();

			bShowTimeline = true;
		}
	}

	UpdateTimelineTabVisibility(bShowTimeline);
}

void FMetaHumanIdentityAssetEditorToolkit::HandleCaptureDataChanged(UCaptureData* InCaptureData, ETimecodeAlignment InTimecodeAlignment, const FString& InCamera, bool bInResetRanges)
{
	// No easy way of telling if Data change came from undo/redo so need to check if viewport actually needs updating
	bool bUpdateCurrentSelection = SelectedIdentityPose.IsValid() && SelectedIdentityPose->GetCaptureData() == InCaptureData;

	// TODO: Make Depth Mesh component work when Capture Data is cleared
	if (InCaptureData == nullptr || bUpdateCurrentSelection)
	{
		ClearMediaTracks();
		DestroyDepthMeshComponent();
		UpdatedViewportForCaptureData(InCaptureData, InTimecodeAlignment, InCamera);

		if (IsUsingFootageData() && bInResetRanges)
		{
			FFrameNumber FirstFrameInRange = Sequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
			FFrameTime FirstFrameAsTime = FFrameTime{ FirstFrameInRange };
			TimelineSequencer->SetGlobalTime(FirstFrameAsTime);
			HandleSequencerGlobalTimeChanged();
		}
	}

	UpdateTimelineTabVisibility(IsUsingFootageData());
}

void FMetaHumanIdentityAssetEditorToolkit::HandleIdentityPartRemoved(UMetaHumanIdentityPart* InIdentityPart)
{
	if (InIdentityPart != nullptr && InIdentityPart->IsA<UMetaHumanIdentityFace>())
	{
		HandleCaptureDataChanged(nullptr, ETimecodeAlignment::None, TEXT(""), true);
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleIdentityPoseAdded(UMetaHumanIdentityPose* InIdentityPose, UMetaHumanIdentityPart* InIdentityPart)
{
	if (InIdentityPose != nullptr && InIdentityPose->PoseType == EIdentityPoseType::Teeth)
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
			{
				InIdentityPose->SetCaptureData(NeutralPose->GetCaptureData());
				InIdentityPose->Camera = NeutralPose->Camera;
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleIdentityPoseRemoved(UMetaHumanIdentityPose* InIdentityPose, UMetaHumanIdentityPart* InIdentityPart)
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->GetPoses().IsEmpty())
		{
			HandleCaptureDataChanged(nullptr, ETimecodeAlignment::None, TEXT(""), true);
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleSequencerMovieSceneDataChanged(EMovieSceneDataChangeType InDataChangeType)
{
	MHA_CPUPROFILER_EVENT_SCOPE(FMetaHumanIdentityAssetEditorToolkit::HandleSequencerMovieSceneDataChanged);

	if (PromotedFramesEditorWidget && IsUsingFootageData())
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		check(MovieScene);

		//TODO: This function is called a lot. Add a check if buttons actually need re-creating
		if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
		{
			PromotedFramesEditorWidget->RecreateAllPromotedFramesButtons();
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::UpdateTimelineTabVisibility(bool InIsCaptureFootage)
{
	if (TabManager.IsValid())
	{
		TSharedPtr<SDockTab> ActiveTab = TabManager->FindExistingLiveTab(TimelineTabId);

		if (ActiveTab && !InIsCaptureFootage)
		{
			ActiveTab->RequestCloseTab();
		}
		else if (!ActiveTab && InIsCaptureFootage)
		{
			TabManager->TryInvokeTab(TimelineTabId);
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleSequencerKeyAdded(FMovieSceneChannel* InChannel, const TArray<FKeyAddOrDeleteEventItem>& InItems)
{
	if (PromotedFramesEditorWidget && InItems.Num() == 1)
	{
		TArrayView<const FKeyHandle> Handles;
		TArrayView<FFrameNumber> FrameNumbers;
		InChannel->GetKeyTimes(Handles, FrameNumbers);

		FMetaHumanMovieSceneChannel* MetaHumanChannel = static_cast<FMetaHumanMovieSceneChannel*>(InChannel);
		if (MetaHumanChannel)
		{
			int32 CurrentKeyIndex = MetaHumanChannel->GetTimes().Find(InItems.Last().Frame);
			bool CreatedWithWidgetButton = MetaHumanChannel->GetValues()[CurrentKeyIndex];

			if (!CreatedWithWidgetButton)
			{
				PromotedFramesEditorWidget->HandleOnAddPromotedFrameClicked();
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleSequencerKeyRemoved(FMovieSceneChannel* InChannel, const TArray<FKeyAddOrDeleteEventItem>& InItems)
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);
	if (PromotedFramesEditorWidget)
	{
		if (UMetaHumanIdentityPose* Pose = PromotedFramesEditorWidget->GetIdentityPose())
		{
			for (const FKeyAddOrDeleteEventItem& Item : InItems)
			{
				const FFrameTime FrameTime = FFrameRate::TransformTime(Item.Frame.Value, MovieScene->GetTickResolution(), MovieScene->GetDisplayRate());

				// Find the promoted frame that corresponds to the key that was removed
				TObjectPtr<UMetaHumanIdentityPromotedFrame>* FrameToRemove = Pose->PromotedFrames.FindByPredicate([FrameTime](const UMetaHumanIdentityPromotedFrame* InPromotedFrame)
				{
					if (const UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(InPromotedFrame))
					{
						return FootageFrame->FrameNumber == FrameTime.FrameNumber.Value;
					}

					return false;
				});

				if (FrameToRemove != nullptr)
				{
					if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(*FrameToRemove))
					{
						PromotedFramesEditorWidget->HandlePromotedFrameRemovedFromSequencer(FootageFrame->FrameNumber);
					}
				}
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleFootageDepthDataChanged(float InNear, float InFar)
{
	if (UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData())
	{
		UMetaHumanFootageComponent* FootageComponent = CastChecked<UMetaHumanFootageComponent>(Pose->CaptureDataSceneComponent);

		if (UMetaHumanFootageComponent* FootageComponentInstance = Cast<UMetaHumanFootageComponent>(IdentityPartsEditor->GetPrimitiveComponent(FootageComponent, true)))
		{
			FootageComponentInstance->SetDepthRange(InNear, InFar);

			// Use the base class HandleSequencerGlobalTimeChanged here. This prevents the ViewportSettings stored frame number
			// be overwritten with an incorrect value when changing pose. See MH-9851.
			// When changing poses this function will be called as part of HandleIdentityTreeSelectionChanged but its called at a point
			// where the sequencer frame number is still that of the old (previously selected) pose and so we dont want that value
			// stored as the current frame for the newly selected pose.
			FMetaHumanToolkitBase::HandleSequencerGlobalTimeChanged();

			GetMetaHumanIdentityViewportClient()->Invalidate();
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::HandleUndoOrRedoTransaction(const FTransaction* InTransaction)
{
	// Let the widgets handle the undo/redo transaction first

	bool bIdentityPartsEditorModified = false;
	if (IdentityPartsEditor.IsValid())
	{
		bIdentityPartsEditorModified = IdentityPartsEditor->HandleUndoOrRedoTransaction(InTransaction);
	}

	if (PromotedFramesEditorWidget.IsValid())
	{
		PromotedFramesEditorWidget->HandleUndoOrRedoTransaction(InTransaction);

		if (!PromotedFramesEditorWidget->GetSelectedPromotedFrame())
		{
			OutlinerWidget->SetPromotedFrame(nullptr, INDEX_NONE, EIdentityPoseType::Invalid);
		}
	}

	// If the parts editor was modified and using footage data, update the sequencer tracks
	if (bIdentityPartsEditorModified && IsUsingFootageData())
	{
		UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData();

		ClearMediaTracks();

		UpdateTimelineTabVisibility(IsUsingFootageData());

		if (PromotedFramesEditorWidget.IsValid())
		{
			PromotedFramesEditorWidget->SetIdentityPose(SelectedIdentityPose.IsValid() ? SelectedIdentityPose.Get() : nullptr);
		}

		UpdateTimelineForFootage(Cast<UFootageCaptureData>(Pose->GetCaptureData()), Pose->TimecodeAlignment, Pose->Camera);
		GetMetaHumanIdentityViewportClient()->UpdateABVisibility();
	}
}

bool FMetaHumanIdentityAssetEditorToolkit::IsTimelineEnabled() const
{
	MHA_CPUPROFILER_EVENT_SCOPE(FMetaHumanIdentityAssetEditorToolkit::IsTimelineEnabled);

	bool TimelineEnabled = false;

	if (Sequence && Sequence->GetMovieScene() && ColourMediaTrack)
	{
		if (SelectedIdentityPose.IsValid() && SelectedIdentityPose->IsCaptureDataValid())
		{
			TimelineEnabled = PromotedFramesEditorWidget->GetSelectedPromotedFrame() == nullptr;
		}
	}

	return TimelineEnabled;
}

void FMetaHumanIdentityAssetEditorToolkit::UpdateKeysForSelectedPose()
{
	if (ColourMediaTrack && !ColourMediaTrack->GetAllSections().IsEmpty())
	{
		UMovieSceneSection* Section = ColourMediaTrack->GetAllSections().Last();
		check(Section);
		Section->Modify();
		TArrayView<FMetaHumanMovieSceneChannel*> MediaTrackChannel = Section->GetChannelProxy().GetChannels<FMetaHumanMovieSceneChannel>();
		MediaTrackChannel.Last()->Reset();

		if (SelectedIdentityPose.IsValid())
		{
			for (UMetaHumanIdentityPromotedFrame* PromotedFrame : SelectedIdentityPose->PromotedFrames)
			{
				if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(PromotedFrame))
				{
					int32 FrameNumber = FootageFrame->FrameNumber;
					AddSequencerKeyForFrameNumber(FrameNumber);
				}
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::AddSequencerKeyForFrameNumber(int32 InFrameNumber)
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	FFrameRate TickRate = MovieScene->GetTickResolution();
	FFrameRate SourceRate = MovieScene->GetDisplayRate();
	const FFrameTime FrameTime = FFrameRate::TransformTime(FFrameTime{ InFrameNumber }, SourceRate, TickRate);

	if (!ChannelContainsKey(ColourMediaTrack, FrameTime.GetFrame()))
	{
		UMovieSceneSection* Section = ColourMediaTrack->GetAllSections().Last();
		Section->Modify();
		Section->GetChannelProxy().GetChannels<FMetaHumanMovieSceneChannel>()[0]->GetData().AddKey(FrameTime.GetFrame(), true);
	}
}

void FMetaHumanIdentityAssetEditorToolkit::UpdateTimelineForFootage(UFootageCaptureData* InFootageCaptureData, ETimecodeAlignment InTimecodeAlignment, const FString& InCamera)
{
	ProcessingFrameRange = TRange<FFrameNumber>(0, 0);
	MediaFrameRanges.Reset();

	if (InFootageCaptureData != nullptr)
	{
		UFootageCaptureData::FVerifyResult Result = InFootageCaptureData->VerifyData(UCaptureData::EInitializedCheck::Full);

		int32 ViewIndex = -1;

		if (!Result.HasError() && InFootageCaptureData)
		{
			ViewIndex = InFootageCaptureData->GetViewIndexByCameraName(InCamera);
		}

		if (ViewIndex >= 0 && ViewIndex < InFootageCaptureData->ImageSequences.Num() && ViewIndex < InFootageCaptureData->DepthSequences.Num())
		{
			// Set a suitable tick rate for the footage
			Sequence->SetTickRate(InFootageCaptureData);

			// Clear read only to update the media tracks
			Sequence->GetMovieScene()->SetReadOnly(false);

			UMovieScene* MovieScene = Sequence->GetMovieScene();
			check(MovieScene);

			const FFrameRate TickRate = MovieScene->GetTickResolution();

			TRange<FFrameNumber> MaxFrameRange;
			InFootageCaptureData->GetFrameRanges(TickRate, InTimecodeAlignment, false, MediaFrameRanges, ProcessingFrameRange, MaxFrameRange);

			TObjectPtr<class UImgMediaSource> ImageSequence = InFootageCaptureData->ImageSequences[ViewIndex];
			TObjectPtr<class UImgMediaSource> DepthSequence = InFootageCaptureData->DepthSequences[ViewIndex];

			FTimecode ImageTimecode = UImageSequenceTimecodeUtils::GetTimecode(ImageSequence.Get());
			FTimecode DepthTimecode = UImageSequenceTimecodeUtils::GetTimecode(DepthSequence.Get());

			const TRange<FFrameNumber>& ImageFrameRange = MediaFrameRanges[ImageSequence];
			const TRange<FFrameNumber>& DepthFrameRange = MediaFrameRanges[DepthSequence];

			// Set the colour and depth tracks in the timeline
			SetMediaTrack(EMediaTrackType::Colour, UMetaHumanMovieSceneMediaTrack::StaticClass(), ImageSequence, ImageTimecode, ImageFrameRange.GetLowerBoundValue());
			SetMediaTrack(EMediaTrackType::Depth, UMetaHumanMovieSceneMediaTrack::StaticClass(), DepthSequence, DepthTimecode, DepthFrameRange.GetLowerBoundValue());

			// Set the view range to match the maximum extent of the tracks
			FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();
			const float ViewTimeOffset = .1f;
			EditorData.WorkStart = TickRate.AsSeconds(MaxFrameRange.GetLowerBoundValue()) - ViewTimeOffset;
			EditorData.WorkEnd = TickRate.AsSeconds(MaxFrameRange.GetUpperBoundValue()) + ViewTimeOffset;
			EditorData.ViewStart = EditorData.WorkStart;
			EditorData.ViewEnd = EditorData.WorkEnd;

			MovieScene->SetPlaybackRange(ProcessingFrameRange);

			// Done with frame ranges. Recalculate processing frame range in terms of identity frames (not sequencer ticks)
			InFootageCaptureData->GetFrameRanges(ImageSequence->FrameRateOverride, InTimecodeAlignment, false, MediaFrameRanges, ProcessingFrameRange, MaxFrameRange);

			UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData();

			if (Pose && Pose->CaptureDataSceneComponent != nullptr)
			{
				UMetaHumanFootageComponent* FootageComponent = CastChecked<UMetaHumanFootageComponent>(Pose->CaptureDataSceneComponent);
				FootageComponent->SetCamera(Pose->Camera);
				FootageComponent->SetMediaTextures(ColourMediaTexture, DepthMediaTexture);

				UMetaHumanFootageComponent* FootageComponentInstance = Cast<UMetaHumanFootageComponent>(IdentityPartsEditor->GetPrimitiveComponent(FootageComponent, true));
				if (FootageComponentInstance)
				{
					FootageComponentInstance->SetCamera(Pose->Camera);
					// New tracks have been created so the texture material needs to be notified of the change
					constexpr bool bNotifyMaterial = false;
					FootageComponentInstance->SetMediaTextures(ColourMediaTexture, DepthMediaTexture, bNotifyMaterial);
				}

				// Set the depth texture to be displayed by the depth mesh component
				SetDepthMeshTexture(DepthMediaTexture);
			}

			TimelineSequencer->RefreshTree();

			GetMetaHumanIdentityViewportClient()->SetTrackerImageSize(InFootageCaptureData->GetFootageColorResolution());

			UpdateKeysForSelectedPose();

			// Set read only so that the tracks can not be modified by the user
			Sequence->GetMovieScene()->SetReadOnly(true);
		}
		else
		{
			UE_LOG(LogMetaHumanIdentity, Error, TEXT("Footage Capture Data asset doesn't contain valid data: '%s'"), Result.HasError() ? *Result.StealError() : TEXT("Bad camera"));
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::UpdateContourDataAfterHeadAlignment(const TWeakObjectPtr<UMetaHumanIdentityPose> InPose)
{
	if (InPose.IsValid())
	{
		for (UMetaHumanIdentityPromotedFrame* PromotedFrame : InPose->PromotedFrames)
		{
			FFrameTrackingContourData ReprojectedContours = GetPoseSpecificContourDataForPromotedFrame(PromotedFrame, InPose, true);
			for (const TPair<FString, FTrackingContour>& Contours : PromotedFrame->GetFrameTrackingContourData()->TrackingContours)
			{
				if (Contours.Value.State.bActive)
				{
					ReprojectedContours.TrackingContours.Remove(Contours.Key);
				}
			}

			if (ReprojectedContours.ContainsData())
			{
				PromotedFrame->UpdateContourDataForIndividualCurves(ReprojectedContours);
			}
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::AddTemplateToMetaHumanToAssetMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	const FName AssetMainMenuName = UToolMenus::JoinMenuPaths(GetToolMenuName(), TEXT("Asset"));
	const FName SectionName = UToolMenus::JoinMenuPaths(AssetMainMenuName, TEXT("DynamicIdentityAssetMenuSection"));

	if (UToolMenu* AssetMenu = ToolMenus->ExtendMenu(AssetMainMenuName))
	{
		// Define the dynamic section only once and use the UMetaHumanIdentityAssetEditorContext to get the state of the open asset
		if (!AssetMenu->FindSection(SectionName))
		{
			AssetMenu->AddDynamicSection(SectionName, FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
				{
					const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();
					UMetaHumanIdentityAssetEditorContext* Context = InMenu->FindContext<UMetaHumanIdentityAssetEditorContext>();
					if (Context && Context->MetaHumanIdentityAssetEditor.IsValid())
					{
						FMetaHumanIdentityAssetEditorToolkit* MetaHumanIdentityAssetEditor = Context->MetaHumanIdentityAssetEditor.Pin().Get();

						const FName MenuName = TEXT("AddComponentsFromConformedMeshMenu");
						TFunction<void(const FAssetData&)> MeshSelectedCallback = [MetaHumanIdentityAssetEditor](const FAssetData& InAssetData)
						{
							if (MetaHumanIdentityAssetEditor->IdentityPartsEditor.IsValid())
							{
								const bool bIsInputConformed = true;
								MetaHumanIdentityAssetEditor->IdentityPartsEditor->AddPartsFromAsset(InAssetData.GetAsset(), bIsInputConformed);
							}
						};

						FToolMenuSection& Section = InMenu->AddSection(TEXT("MetaHumanIdentityAssetActions"), LOCTEXT("MetaHumanIdentityAssetActionsSection", "MetaHuman Identity"));
						Section.AddEntry(FToolMenuEntry::InitSubMenu(MenuName,
							LOCTEXT("AddComponentsFromConformedMesh", "Configure Components from Conformed"),
							TAttribute<FText>::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::GetConfigureComponentsFromConformedTooltipText),
							FNewToolMenuDelegate::CreateSP(MetaHumanIdentityAssetEditor, &FMetaHumanIdentityAssetEditorToolkit::MakeMeshAssetPickerMenu, MeshSelectedCallback),
							false,
							FSlateIcon(TEXT("MetaHumanIdentityStyle"), TEXT("Identity.Tools.ComponentsFromConformed"), TEXT("Identity.Tools.ComponentsFromConformed")))
						);
					}
				}));
		}
	}
}

FText FMetaHumanIdentityAssetEditorToolkit::GetConfigureComponentsFromConformedTooltipText() const
{
	if (CanCreateComponents())
	{
		return LOCTEXT("AddComponentsFromConformedMeshTooltip", "Configure all the components in this MetaHuman Identity using a mesh already conformed to the MetaHuman topology");
	}
	else
	{
		return LOCTEXT("AddComponentsFromConformedMeshDisabledTooltip", "Remove existing Face component to enable this option");
	}
}

void FMetaHumanIdentityAssetEditorToolkit::UpdatedViewportForCaptureData(UCaptureData* InCaptureData, ETimecodeAlignment InTimecodeAlignment, const FString& InCamera)
{
	if (InCaptureData != nullptr && CaptureDataIsConsistentForPoses(InCaptureData))
	{
		if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(InCaptureData))
		{
			UFootageCaptureData::FVerifyResult Result = FootageCaptureData->VerifyData(UCaptureData::EInitializedCheck::Full);
			if (!Result.HasError())
			{
				CreateDepthMeshComponent(FootageCaptureData->CameraCalibrations[0]);
			}
			else
			{
				UE_LOG(LogMetaHumanIdentity, Error, TEXT("Footage Capture Data asset doesn't contain valid data: '%s'"), *Result.StealError());
			}

			UpdateTimelineForFootage(FootageCaptureData, InTimecodeAlignment, InCamera);
		}
		else if (InCaptureData->IsA<UMeshCaptureData>())
		{
			GetMetaHumanIdentityViewportClient()->SetTrackerImageSize(UMetaHumanIdentityPromotedFrame::DefaultTrackerImageSize);
		}
	}
}

FText FMetaHumanIdentityAssetEditorToolkit::GetMeshToMetaHumanDNAOnlyButtonTooltip() const
{
	FText MeshToMetaHumanDNAOnlyButtonTooltipText = LOCTEXT("MeshToMetaHumanDNAOnlyButtonTooltip", "Submit the Template to the Mesh to MetaHuman Service for auto-rigging.");

	bool bFullMetaHuman = false;
	FText TooltipTextWithAfterProcessingInfo = GetMeshToMetaHumanButtonTooltipWithAfterProcessingInfo(MeshToMetaHumanDNAOnlyButtonTooltipText, bFullMetaHuman);

	return GetMeshToMetaHumanButtonTooltipWithEnableInstructionsAdded(TooltipTextWithAfterProcessingInfo);
}

FText FMetaHumanIdentityAssetEditorToolkit::GetMeshToMetaHumanButtonTooltipWithAfterProcessingInfo(FText Tooltip, bool bFullMetaHuman) const
{
	FText AfterProcessingText = FText::Format(LOCTEXT("MeshToMetaHumanAfterProcessingCommonInfoTooltip", "{0}\n\nAfter the processing is finished, a Skeletal Mesh matching the Identity\nwill appear in the Content Browser. Having a MetaHuman DNA embedded\ninside, it can be used for solving the animation in the Performance asset."), Tooltip);
	if (bFullMetaHuman)
	{
		AfterProcessingText = FText::Format(LOCTEXT("MeshToMetaHumanAfterProcessingFullMetaHumanTooltip", "{0}\n\nAlso, a full MetaHuman will appear in MetaHuman Creator for further editing.\nIt can be imported into Unreal Editor through Quixel Bridge.\n\nNOTE: The retrieved Skeletal Mesh can be further processed to fit its teeth\nto actor's using the Fit Teeth command, so it will differ from the downloaded\nMetaHuman, which should NOT be used for solving the Performance."), AfterProcessingText);
	}
	return AfterProcessingText;
}

FText FMetaHumanIdentityAssetEditorToolkit::GetMeshToMetaHumanButtonTooltipWithEnableInstructionsAdded(FText MainTooltipText) const
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->CanSubmitToAutorigging())
		{
			return MainTooltipText;
		}
		else
		{
			return FText::Format(LOCTEXT("MeshToMetaHumanToolbarButtonNotConformedTooltip", "{0}\n\nTo enable this option, the Template needs to be conformed to the given\nMetaHuman Identity by using MetaHuman Identity Solve button on the Toolbar,\nand the Neutral Pose must have a valid Capture Data set."), MainTooltipText);
		}
	}
	else
	{
		return FText::Format(LOCTEXT("MeshToMetaHumanToolbarButtonNoFaceTooltip", "{0}\n\nTo enable this option, first add Face Part to the MetaHuman Identity treeview\nby using Add(+) or Create Components button on the Toolbar"), MainTooltipText);
	}
}

FText FMetaHumanIdentityAssetEditorToolkit::GetPrepareForPerformanceButtonTooltip() const
{
	if (!bDepthProcessingEnabled)
	{
		return LOCTEXT("PrepareForPerformanceButtonDisabledNoPluginTooltip", "To enable this option please make sure Depth Processing plugin is enabled. (Available on Fab)");
	}

	const FMetaHumanIdentityEditorCommands& Commands = FMetaHumanIdentityEditorCommands::Get();
	FText PrepareForPerformanceTooltip = Commands.PrepareForPerformance->GetDescription();
	if (CanRunSolverTraining())
	{
		return PrepareForPerformanceTooltip;
	}
	else {
		return FText::Format(LOCTEXT("PrepareForPerformanceButtonDisabledTooltip", "{0}\nTo enable this option, first send a conformed Template to the Mesh to MetaHuman Service\nusing commands from the drop-down menu in the Mesh to MetaHuman button\non the toolbar"), PrepareForPerformanceTooltip);
	}
}

UMetaHumanIdentityPose::ECurrentFrameValid FMetaHumanIdentityAssetEditorToolkit::GetIsCurrentFrameValid() const
{
	if (UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData())
	{
		return Pose->GetIsFrameValid(GetCurrentFrameNumber().Value, ProcessingFrameRange, MediaFrameRanges);
	}

	return UMetaHumanIdentityPose::ECurrentFrameValid::Invalid_NoCaptureData;
}

void FMetaHumanIdentityAssetEditorToolkit::HandleAutoriggingServiceFinished(bool InSuccess)
{
	if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		IdentityStateValidator->MeshAutoriggedUpdate();
		//if autorigging succeeded and there is the teeth pose present with at least one promoted frame, do FitTeeth automatically
		UMetaHumanIdentityPose* TeethPose = Face->FindPoseByType(EIdentityPoseType::Teeth);
		if (InSuccess && TeethPose != nullptr
			&& TeethPose->PromotedFrames.Num() > 0)
		{
			HandleFitTeeth();
		}
	}
}

void FMetaHumanIdentityAssetEditorToolkit::GetExcludedFrameInfo(FFrameRate& OutSourceRate, FFrameRangeMap& OutExcludedFramesMap, int32& OutRGBMediaStartFrame, TRange<FFrameNumber>& OutProcessingLimit) const
{
	UFootageCaptureData* FootageCaptureData = GetFootageCaptureData();
	if (FootageCaptureData && !FootageCaptureData->ImageSequences.IsEmpty() && FootageCaptureData->ImageSequences[0] && MediaFrameRanges.Contains(FootageCaptureData->ImageSequences[0]))
	{
		const FFrameRate ProcessingFrameRate = FootageCaptureData->ImageSequences[0]->FrameRateOverride;
		OutSourceRate = ProcessingFrameRate.IsValid() ? ProcessingFrameRate : TimelineSequencer->GetRootDisplayRate();

		OutExcludedFramesMap.Add(EFrameRangeType::CaptureExcluded, GetFootageCaptureData()->CaptureExcludedFrames);

		OutRGBMediaStartFrame = MediaFrameRanges[FootageCaptureData->ImageSequences[0]].GetLowerBoundValue().Value;

		if (UMetaHumanIdentityPose* Pose = GetAvailablePoseWithCaptureData())
		{
			TArray<FFrameRange> RateMatchingExcludedFrames = Pose->GetRateMatchingExcludedFrameRanges();
			if (!RateMatchingExcludedFrames.IsEmpty())
			{
				OutExcludedFramesMap.Add(EFrameRangeType::RateMatchingExcluded, RateMatchingExcludedFrames);
			}
		}

		OutProcessingLimit = ProcessingFrameRange;
	}
}

#undef LOCTEXT_NAMESPACE

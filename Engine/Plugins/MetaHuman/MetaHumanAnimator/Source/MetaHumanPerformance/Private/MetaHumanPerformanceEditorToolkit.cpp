// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceEditorToolkit.h"
#include "Components/SkeletalMeshComponent.h"
#include "MetaHumanPerformanceEditor.h"
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceViewportSettings.h"
#include "MetaHumanPerformanceLog.h"
#include "MetaHumanPerformerCommands.h"
#include "MetaHumanToolkitCommands.h"
#include "MetaHumanEditorSettings.h"
#include "MetaHumanFaceAnimationSolver.h"
#include "UI/MetaHumanPerformanceStyle.h"
#include "MetaHumanTrace.h"

#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanPerformanceEditorContext.h"
#include "MetaHumanComponentBase.h"
#include "CoreUtils.h"
#include "CaptureData.h"
#include "CaptureDataUtils.h"
#include "ImageSequenceUtils.h"
#include "UI/MetaHumanPerformanceControlRigComponent.h"
#include "UI/MetaHumanPerformanceViewportClient.h"
#include "UI/MetaHumanPerformanceControlRigViewportClient.h"
#include "MetaHumanFootageComponent.h"
#include "MetaHumanSequence.h"
#include "MetaHumanCurveDataController.h"
#include "LandmarkConfigIdentityHelper.h"
#include "Sequencer/MetaHumanPerformanceMovieSceneMediaTrack.h"
#include "Sequencer/MetaHumanPerformanceMovieSceneMediaSection.h"
#include "Sequencer/MetaHumanPerformanceMovieSceneAudioTrack.h"
#include "Sequencer/MetaHumanPerformanceMovieSceneAudioSection.h"
#include "ISequencer.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Nodes/FaceTrackerNode.h"
#include "Sections/MovieSceneAudioSection.h"
#include "MediaTexture.h"
#include "ImgMediaSource.h"
#include "Sound/SoundWave.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"
#include "AdvancedPreviewScene.h"
#include "Editor/Transactor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Engine/StaticMeshActor.h"
#include "ControlRigBlueprint.h"
#include "ControlRigObjectBinding.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Components/StaticMeshComponent.h"
#include "IDetailsView.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "EditorViewportCommands.h"
#include "ScopedTransaction.h"
#include "EngineAnalytics.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "ImageSequenceTimecodeUtils.h"
#include "MetaHumanPerformanceExportUtils.h"


#define LOCTEXT_NAMESPACE "MetaHumanPerformanceEditorToolkit"

const FName FMetaHumanPerformanceEditorToolkit::ImageReviewTabId(TEXT("ImageReview"));
const FName FMetaHumanPerformanceEditorToolkit::ControlRigTabId(TEXT("ControlRig"));

FMetaHumanPerformanceEditorToolkit::FMetaHumanPerformanceEditorToolkit(UAssetEditor* InOwningAssetEditor)
	: FMetaHumanToolkitBase{ InOwningAssetEditor }
	, Performance{ nullptr }
{
	// Get the Performance from the asset editor
	UMetaHumanPerformanceEditor *PerformanceEditor = Cast<UMetaHumanPerformanceEditor>(InOwningAssetEditor);
	check(PerformanceEditor);

	// Register the commands that are used in this editor
	FMetaHumanPerformanceCommands::Register();

	TArray<UObject*> ObjectsToEdit;
	PerformanceEditor->GetObjectsToEdit(ObjectsToEdit);
	check(!ObjectsToEdit.IsEmpty() && ObjectsToEdit[0]);
	Performance = CastChecked<UMetaHumanPerformance>(ObjectsToEdit[0]);
	check(Performance);

	DisplayContourData = NewObject<UMetaHumanContourData>();
	CurveDataController = MakeShared<FMetaHumanCurveDataController>(DisplayContourData, ECurveDisplayMode::Visualization);

	// Initialization
	InitPerformerViewport();

	// Create the layout of our custom asset editor. The parent class provides a basic layout with a details panel and
	// a 3d viewport. We keep the details panel using DetailsTabID but we create a custom 3D viewport that will display an
	// IPersonaPreviewScene
	FString LayoutString = TEXT("Standalone_MetaHumanPerformanceEditor_Layout_v1");
	StandaloneDefaultLayout = FTabManager::NewLayout(FName(LayoutString))
		->AddArea
		(
			// Create a vertical area and spawn the toolbar
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				// Split the tab and pass the tab id to the tab spawner
				FTabManager::NewSplitter()
				->Split
				(
					FTabManager::NewStack()
					->AddTab(ImageReviewTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(ViewportTabID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.4)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetHideTabWell(true)
						->AddTab(DetailsTabID, ETabState::OpenedTab)
						->AddTab(PreviewSettingsTabId, ETabState::ClosedTab)
						->SetHideTabWell(false)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetHideTabWell(true)
						->AddTab(ControlRigTabId, ETabState::OpenedTab)
					)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->SetHideTabWell(true)
				->AddTab(TimelineTabId, ETabState::OpenedTab)
			)
		);
}

FMetaHumanPerformanceEditorToolkit::~FMetaHumanPerformanceEditorToolkit()
{
	DetailsView.Reset();

	if (Performance)
	{
		// Need to cancel the pipeline when closing the editor or processing will continue in the background
		Performance->CancelPipeline();
	}

	if (TimelineSequencer)
	{
		TimelineSequencer->Close();
	}

	if (FootageComponent)
	{
		for (UPrimitiveComponent* FootagePlaneComponent : FootageComponent->GetFootagePlaneComponents())
		{
			PreviewScene->RemoveComponent(FootagePlaneComponent);
		}
	}
}

FName FMetaHumanPerformanceEditorToolkit::GetToolkitFName() const
{
	return TEXT("MetaHumanPerformanceEditor");
}

FText FMetaHumanPerformanceEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Performance Editor Toolkit");
}

FText FMetaHumanPerformanceEditorToolkit::GetToolkitToolTipText() const
{
	FText AssetName = FText::FromString(Performance->GetName());
	return FText::Format(LOCTEXT("PerformanceToolkitToolTipTextExtended", "Asset: {0} (Performance)"), AssetName);
}

FString FMetaHumanPerformanceEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "MetaHuman ").ToString();
}

FLinearColor FMetaHumanPerformanceEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FColor::White;
}

void FMetaHumanPerformanceEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	// Add a new workspace menu category to the tab manager
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Performance Editor"));

	// We register the tab manager to the asset editor toolkit so we can use it in this editor
	FMetaHumanToolkitBase::RegisterTabSpawners(InTabManager);

	// The property tab spawner is registered by the parent class

	// We provide the function with the identifier for this tab and a shared pointer to the
	// SpawnPropertiesTab function within this editor class
	// Additionally, we provide a name to be displayed, a category and the tab icon

	// Image Review
	InTabManager->RegisterTabSpawner(ImageReviewTabId, FOnSpawnTab::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::SpawnImageReviewTab))
		.SetDisplayName(LOCTEXT("ImageReviewTab", "Image Review"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FMetaHumanPerformanceStyle::Get().GetStyleSetName(), TEXT("Performance.Tabs.ImageReview")));

	InTabManager->RegisterTabSpawner(ControlRigTabId, FOnSpawnTab::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::SpawnControlRigTab))
		.SetDisplayName(LOCTEXT("ControlRigTab", "Control Rig"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FMetaHumanPerformanceStyle::Get().GetStyleSetName(), TEXT("Performance.Tabs.ControlRig")));
}

void FMetaHumanPerformanceEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	// Unregister the tab manager from the asset editor toolkit
	FMetaHumanToolkitBase::UnregisterTabSpawners(InTabManager);

	// Unregister our custom tab from the tab manager, making sure it is cleaned up when the editor gets destroyed
	InTabManager->UnregisterTabSpawner(ImageReviewTabId);
}

void FMetaHumanPerformanceEditorToolkit::AddReferencedObjects(FReferenceCollector& InCollector)
{
	FMetaHumanToolkitBase::AddReferencedObjects(InCollector);

	if (RecordControlRig != nullptr)
	{
		InCollector.AddReferencedObject(RecordControlRig);
	}

	if (DisplayContourData != nullptr)
	{
		InCollector.AddReferencedObject(DisplayContourData);
	}
}

FString FMetaHumanPerformanceEditorToolkit::GetReferencerName() const
{
	return TEXT("FMetaHumanPerformanceEditorToolkit");
}

FString FMetaHumanPerformanceEditorToolkit::GetPossessableNameSkeletalMesh() const
{
	return TEXT("Face");
}

FString FMetaHumanPerformanceEditorToolkit::GetPossessableNameActor() const
{
	return Performance->GetName() + TEXT(" Actor");
}

void FMetaHumanPerformanceEditorToolkit::UpdateVisualizationMesh(USkeletalMesh* InVisualizeMesh)
{
	USkeletalMesh* Mesh = InVisualizeMesh;

	if (!Mesh && Performance->Identity)
	{
		if (UMetaHumanIdentityFace* Face = Performance->Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->RigComponent)
			{
				Mesh = Face->RigComponent->GetSkeletalMeshAsset();
			}
		}
	}

	if (Mesh != FaceSkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		// Set the mesh in the RigComponent we are visualizing
		FaceSkeletalMeshComponent->SetSkeletalMesh(Mesh);
		FPropertyChangedEvent SkelMeshChangedEvent(USkeletalMeshComponent::StaticClass()->FindPropertyByName(TEXT("SkeletalMeshAsset")));
		FaceSkeletalMeshComponent->PostEditChangeProperty(SkelMeshChangedEvent);

		if (Mesh)
		{
			if (Mesh->GetPostProcessAnimBlueprint() == nullptr)
			{
				// If the Skeletal Mesh doesn't have a post process animation blueprint set,
				// use the Face_PostProcess_AnimBP to make sure animation will play in the preview mesh
				// This may happen if the user selects a Face Mesh that doesn't have the Post Process AnimBP set
				// or in UEFN, where MetaHuman Faces don't have the blueprint set since the Animation Blueprints
				// are not supported

				TArray<FAssetData> AnimBPData;
				IAssetRegistry::GetChecked().GetAssetsByPackageName(TEXT("/" UE_PLUGIN_NAME "/IdentityTemplate/Face_PostProcess_AnimBP"), AnimBPData);
				if (!AnimBPData.IsEmpty())
				{
					const FAssetData& AnimBPAsset = AnimBPData[0];
					if (AnimBPAsset.IsValid())
					{
						if (AnimBPAsset.IsInstanceOf(UAnimBlueprint::StaticClass()))
						{
							// UE editor is going through this route
							UAnimBlueprint* LoadedAnimBP = Cast<UAnimBlueprint>(AnimBPAsset.GetAsset());
							FaceSkeletalMeshComponent->SetOverridePostProcessAnimBP(LoadedAnimBP->GetAnimBlueprintGeneratedClass());
						}
						else if (AnimBPAsset.IsInstanceOf(UAnimBlueprintGeneratedClass::StaticClass()))
						{
							// Cooked UEFN seems to be going via this route
							UAnimBlueprintGeneratedClass* LoadedAnimBP = Cast<UAnimBlueprintGeneratedClass>(AnimBPAsset.GetAsset());
							FaceSkeletalMeshComponent->SetOverridePostProcessAnimBP(LoadedAnimBP);
						}
					}
				}
			}
			else
			{
				// Clear the Override Post Process AnimBP and use the one from the Mesh
				FaceSkeletalMeshComponent->SetOverridePostProcessAnimBP(nullptr);
			}

			// Force all materials in the Skeletal Mesh Component to be the ones coming from the Mesh to avoid issues with material slots not updating
			const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
			{
				FaceSkeletalMeshComponent->SetMaterial(MaterialIndex, Materials[MaterialIndex].MaterialInterface);
			}
			FaceSkeletalMeshComponent->MarkRenderStateDirty();
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleUndoOrRedoTransaction(const FTransaction* InTransaction)
{
	UpdateVisualizationMesh(Performance->VisualizationMesh);

	// Possessable will lose its binding after undo, so we want to reassign it
	RebindSequencerPossessableObjects();

	// Only recreate footage component if the footage related data changed
	bool bDataInputTypeChanged = HasPropertyChanged(InTransaction, GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, InputType));
	bool bFootageCaptureDataChanged = HasPropertyChanged(InTransaction, GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, FootageCaptureData));
	bool bAudioChanged = HasPropertyChanged(InTransaction, GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, Audio));
	bool bCameraChanged = HasPropertyChanged(InTransaction, GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, Camera));
	bool bTimecodeAlignmentChanged = HasPropertyChanged(InTransaction, GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, TimecodeAlignment));

	if (bDataInputTypeChanged || bFootageCaptureDataChanged || bAudioChanged || bCameraChanged || bTimecodeAlignmentChanged)
	{
		HandleSourceDataChanged(Performance->FootageCaptureData, Performance->GetAudioForProcessing(), false);

		if (bDataInputTypeChanged || bFootageCaptureDataChanged || bAudioChanged || bTimecodeAlignmentChanged) // Will need to set the current frame to ensure its valid
		{
			UMovieScene* MovieScene = Sequence->GetMovieScene();
			check(MovieScene);

			const FFrameRate TickRate = MovieScene->GetTickResolution();
			TRange<FFrameNumber> ProcessingFrameRange(0, 0);

			if (Performance->InputType == EDataInputType::Audio)
			{
				TObjectPtr<class USoundWave> AudioForProcessing = Performance->GetAudioForProcessing();
				ProcessingFrameRange = UFootageCaptureData::GetAudioFrameRange(TickRate, Performance->TimecodeAlignment, AudioForProcessing, Performance->GetAudioMediaTimecode(), Performance->GetAudioMediaTimecodeRate());
			}
			else if (Performance->FootageCaptureData)
			{
				TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;
				TRange<FFrameNumber> MaxFrameRange;

				Performance->FootageCaptureData->GetFrameRanges(TickRate, Performance->TimecodeAlignment, true, MediaFrameRanges, ProcessingFrameRange, MaxFrameRange);
			}

			TimelineSequencer->SetGlobalTime(ProcessingFrameRange.GetLowerBoundValue());
		}

		GetMetaHumanPerformerViewportClient()->UpdateABVisibility();
	}
}

bool FMetaHumanPerformanceEditorToolkit::HasPropertyChanged(const FTransaction* InTransaction, const FName& InPropertyName) const
{
	const FTransactionDiff Diff = InTransaction->GenerateDiff();

	TArray<UObject*> AffectedObjects;
	InTransaction->GetTransactionObjects(AffectedObjects);

	for (const TPair<FName, TSharedPtr<FTransactionObjectEvent>>& DiffMapPair : Diff.DiffMap)
	{
		const FString ObjectName = DiffMapPair.Key.ToString();
		const TSharedPtr<FTransactionObjectEvent>& TransactionObjectEvent = DiffMapPair.Value;

		if (TransactionObjectEvent->HasPropertyChanges())
		{
			const int32 ObjectIndex = AffectedObjects.IndexOfByPredicate([this](const UObject* InObject)
			{
				return InObject != nullptr && Performance;
			});

			if (ObjectIndex != INDEX_NONE)
			{
				for (const FName& PropertyNameThatChanged : TransactionObjectEvent->GetChangedProperties())
				{
					if (InPropertyName == PropertyNameThatChanged)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void FMetaHumanPerformanceEditorToolkit::RebindSequencerPossessableObjects()
{
	check(Sequence);

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	// Update actor object binding
	if (MovieScene->FindPossessable(PerformerActorBindingId) == nullptr)
	{
		const FString ActorPossessableName = GetPossessableNameActor();
		FMovieScenePossessable* ActorPossessable = MovieScene->FindPossessable([ActorPossessableName](const FMovieScenePossessable& Possessable)
		{
			return Possessable.GetName() == ActorPossessableName;
		});

		if (ActorPossessable)
		{
			PerformerActorBindingId = ActorPossessable->GetGuid();
			Sequence->BindPossessableObject(PerformerActorBindingId, *PreviewActor, PreviewActor);
		}
		else
		{
			PerformerActorBindingId.Invalidate();
		}
	}

	// Update face component object binding
	if (MovieScene->FindPossessable(PerformerFaceBindingId) == nullptr)
	{
		const FString SkeletalMeshPossessableName = GetPossessableNameSkeletalMesh();
		FMovieScenePossessable* FaceComponentPossessable = MovieScene->FindPossessable([SkeletalMeshPossessableName](const FMovieScenePossessable& Possessable)
		{
			return Possessable.GetName() == SkeletalMeshPossessableName;
		});

		if (FaceComponentPossessable)
		{
			PerformerFaceBindingId = FaceComponentPossessable->GetGuid();
			Sequence->BindPossessableObject(PerformerFaceBindingId, *FaceSkeletalMeshComponent, PreviewActor);
		}
		else
		{
			PerformerFaceBindingId.Invalidate();
		}
	}
}

TSharedRef<SDockTab> FMetaHumanPerformanceEditorToolkit::SpawnImageReviewTab(const FSpawnTabArgs& InArgs)
{
	const FEditorViewportCommands& Commands = FEditorViewportCommands::Get();
	TSharedPtr<FUICommandList> ImageReviewCommandList = MakeShared<FUICommandList>();
	ImageReviewCommandList->MapAction(Commands.FocusViewportToSelection,
									  FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleImageReviewFocus));

	check(InArgs.GetTabId() == ImageReviewTabId);
	ImageViewer = SNew(SMetaHumanOverlayWidget<SMetaHumanImageViewer>)
		.CommandList(ImageReviewCommandList);
	ImageViewer->SetImage(&ImageViewerBrush);
	ImageViewer->SetNonConstBrush(&ImageViewerBrush);
	ImageViewerBrush.SetUVRegion(FBox2f{ FVector2f{ 0.0f, 0.0f }, FVector2f{ 1.0f, 1.0f } });
	// Lambda that reacts to inputs in the image viewer, used for zooming and panning
	ImageViewer->OnViewChanged.AddLambda([this](FBox2f InUV)
	{
		ImageViewerBrush.SetUVRegion(InUV);
	});

	HandleSequencerGlobalTimeChanged();

	return SNew(SDockTab)
		.Label(LOCTEXT("ImageReviewTabTitle", "Image Review"))
		.ToolTipText(LOCTEXT("ImageReviewTabTooltip", "Use this to review the original footage"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				ImageViewer.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FMetaHumanPerformanceEditorToolkit::SpawnControlRigTab(const FSpawnTabArgs& InArgs)
{
	check(InArgs.GetTabId() == ControlRigTabId);

	TSharedRef<SDockTab> ControlRigTab = SNew(SDockTab)
		.Label(LOCTEXT("ControlRigTabTitle", "Control Rig"))
		.ToolTipText(LOCTEXT("ControlRigTabTooltip", "Use this to review how the solved animation behaves on the Control Rig."))
		.TabColorScale(GetTabColorScale());

	ControlRigManager.InitializeControlRigTabContents(ControlRigTab);

	return ControlRigTab;
}

void FMetaHumanPerformanceEditorToolkit::PostInitAssetEditor()
{
	FMetaHumanToolkitBase::PostInitAssetEditor();

	Performance->OnDataInputTypeChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleDataInputTypeChanged);
	Performance->OnSourceDataChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleSourceDataChanged);
	Performance->OnIdentityChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleIdentityChanged);
	Performance->OnVisualizeMeshChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleVisualizeMeshChanged);
	Performance->OnControlRigClassChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleControlRigClassChanged);
	Performance->OnHeadMovementModeChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleHeadMovementModeChanged);
	Performance->OnHeadMovementReferenceFrameChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleHeadMovementReferenceFrameChanged);
	Performance->OnNeutralPoseCalibrationChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleNeutralPoseCalibrationChanged);
	Performance->OnFrameRangeChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleFrameRangeChanged);
	Performance->OnRealtimeAudioChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleRealtimeAudioChanged);
	Performance->OnFrameProcessed().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleFrameProcessed);
	Performance->OnProcessingFinished().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleProcessingFinished);
	Performance->OnStage1ProcessingFinished().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleStage1ProcessingFinished);
	Performance->OnExcludedFramesChanged().AddSP(this, &FMetaHumanPerformanceEditorToolkit::HandleSequencerGlobalTimeChanged);
	Performance->OnGetCurrentFrame().BindSP(this, &FMetaHumanPerformanceEditorToolkit::GetCurrentFrameNumber);

	Sequence->GetExcludedFrameInfo.BindSP(this, &FMetaHumanPerformanceEditorToolkit::GetExcludedFrameInfo);

	// Creates a root transaction that encapsulates all transactions generated
	// in the scope. When this variable goes out of scope it will discard the root
	// transaction along with all child transactions. This effectively removes
	// an unnecessary undo operations on the stack generated by the sequencer
	// calls below (e.g. add actors to the timeline sequencer).
	struct FScopedTransactionDiscard
	{
		FScopedTransactionDiscard()
			: Trans(FScopedTransaction(FText::GetEmpty()))
		{
		}

		~FScopedTransactionDiscard()
		{
			Trans.Cancel();
		}

		FScopedTransaction Trans;
	} TransactionDiscard;

	HandleSourceDataChanged(Performance->FootageCaptureData, Performance->GetAudioForProcessing(), true);
	HandleFrameRangeChanged(Performance->StartFrameToProcess, Performance->EndFrameToProcess);
	HandleVisualizeMeshChanged(Performance->VisualizationMesh);
	HandleSequencerGlobalTimeChanged();

	ExtendToolBar();
	ExtendMenu();

	// Disable editing of curves and points by the user
	GetMetaHumanPerformerViewportClient()->SetEditCurvesAndPointsEnabled(false);

	// Update the visibility to force a refresh if any 3d elements are visible when opening the asset
	GetMetaHumanPerformerViewportClient()->UpdateABVisibility();

	// Set the data controller in Image Viewer for curve visualization
	GetMetaHumanPerformerViewportClient()->SetCurveDataController(CurveDataController);

	// Restore the sequencer time
	TimelineSequencer->SetGlobalTime(Performance->ViewportSettings->CurrentFrameTime);

	if (!IPluginManager::Get().FindEnabledPlugin("MetaHumanDepthProcessing").IsValid())
	{
		UEnum* DataInputTypeEnum = StaticEnum<EDataInputType>();
		int64 DepthFootageIndex = DataInputTypeEnum->GetIndexByValue(static_cast<int64>(EDataInputType::DepthFootage));
		DataInputTypeEnum->SetMetaData(TEXT("Hidden"), TEXT("true"), DepthFootageIndex);
			}

	// Mark the toolkit as not being initialized anymore so the viewport settings can start being updated
	bIsToolkitInitializing = false;
		}

void FMetaHumanPerformanceEditorToolkit::InitPerformerViewport()
{
	check(PreviewActor);

	FaceSkeletalMeshComponent = NewObject<USkeletalMeshComponent>(PreviewActor, *GetPossessableNameSkeletalMesh());
	PreviewActor->AddInstanceComponent(FaceSkeletalMeshComponent);
	FaceSkeletalMeshComponent->AttachToComponent(PreviewActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	// Move skeletal mesh backwards so it can "fit" the screen. Mesh transformation will conform to the footage once we start processing
	FaceSkeletalMeshComponent->SetRelativeLocation(FVector(85.0, 0.0, 0.0));
	FaceSkeletalMeshComponent->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	FaceSkeletalMeshComponent->RegisterComponent();

	ControlRigComponent = NewObject<UMetaHumanPerformanceControlRigComponent>(PreviewActor, TEXT("Face Control Rig"));
	PreviewActor->AddInstanceComponent(ControlRigComponent);
	ControlRigComponent->AttachToComponent(FaceSkeletalMeshComponent, FAttachmentTransformRules::KeepRelativeTransform);
	ControlRigComponent->RegisterComponent();
}

TSharedPtr<FEditorViewportClient> FMetaHumanPerformanceEditorToolkit::CreateEditorViewportClient() const
{
	TSharedRef<FMetaHumanPerformanceViewportClient> PerformanceViewportClient = MakeShared<FMetaHumanPerformanceViewportClient>(PreviewScene.Get(), Performance);

	// Setting the components as attributes allows them to be changed without the need to be reset in the client
	PerformanceViewportClient->SetRigComponent(TAttribute<USkeletalMeshComponent*>::CreateLambda([this]
	{
		return FaceSkeletalMeshComponent;
	}));

	PerformanceViewportClient->SetFootageComponent(TAttribute<UMetaHumanFootageComponent*>::CreateLambda([this]
	{
		return FootageComponent;
	}));

	PerformanceViewportClient->SetControlRigComponent(TAttribute<UMetaHumanPerformanceControlRigComponent*>::CreateLambda([this]
	{
		return ControlRigComponent;
	}));

	return PerformanceViewportClient;
}

void FMetaHumanPerformanceEditorToolkit::BindCommands()
{
	FMetaHumanToolkitBase::BindCommands();

	const FMetaHumanPerformanceCommands& Commands = FMetaHumanPerformanceCommands::Get();

	ToolkitCommands->MapAction(Commands.StartProcessingShot,
							   FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleProcessButtonClicked),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::CanProcess));
	ToolkitCommands->MapAction(Commands.CancelProcessingShot,
							   FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleCancelButtonClicked),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::CanCancel));
	ToolkitCommands->MapAction(Commands.ExportAnimation,
							   FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleExportAnimationClicked),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::CanExportAnimation));
	ToolkitCommands->MapAction(Commands.ExportLevelSequence,
							   FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleExportLevelSequenceClicked),
							   FCanExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::CanExportAnimation));

	ABCommandList.MapAction(Commands.ToggleRig,
							GetMetaHumanPerformerViewportClient(),
							&FMetaHumanPerformanceViewportClient::ToggleRigVisibility,
							&FMetaHumanPerformanceViewportClient::CanExecuteAction,
							&FMetaHumanPerformanceViewportClient::IsRigVisible);

	ABCommandList.MapAction(Commands.ToggleFootage,
							GetMetaHumanPerformerViewportClient(),
							&FMetaHumanPerformanceViewportClient::ToggleFootageVisibility,
							&FMetaHumanPerformanceViewportClient::CanExecuteAction,
							&FMetaHumanPerformanceViewportClient::IsFootageVisible);

	ABCommandList.MapAction(Commands.ToggleControlRigDisplay,
							GetMetaHumanPerformerViewportClient(),
							&FMetaHumanPerformanceViewportClient::ToggleControlRigVisibility,
							&FMetaHumanPerformanceViewportClient::CanExecuteAction,
							&FMetaHumanPerformanceViewportClient::IsControlRigVisible);

	check(Commands.ViewSetupStore.Num() == Commands.ViewSetupRestore.Num());

	for (int32 ViewSetupSlot = 0; ViewSetupSlot < Commands.ViewSetupStore.Num(); ++ViewSetupSlot)
	{
		ToolkitCommands->MapAction(Commands.ViewSetupStore[ViewSetupSlot],
								   FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleViewSetupClicked, ViewSetupSlot, true));

		ToolkitCommands->MapAction(Commands.ViewSetupRestore[ViewSetupSlot],
								   FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleViewSetupClicked, ViewSetupSlot, false));
	}

	ToolkitCommands->MapAction(Commands.ToggleShowFramesAsTheyAreProcessed,
							   FExecuteAction::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::HandleShowFramesAsTheyAreProcessed));
}

TSharedRef<FMetaHumanPerformanceViewportClient> FMetaHumanPerformanceEditorToolkit::GetMetaHumanPerformerViewportClient() const
{
	return StaticCastSharedPtr<FMetaHumanPerformanceViewportClient>(ViewportClient).ToSharedRef();
}

void FMetaHumanPerformanceEditorToolkit::ExtendToolBar()
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
					const FMetaHumanPerformanceCommands& Commands = FMetaHumanPerformanceCommands::Get();
					UMetaHumanPerformanceEditorContext* Context = InMenu->FindContext<UMetaHumanPerformanceEditorContext>();
					if (Context && Context->MetaHumanPerformanceEditorToolkit.IsValid())
					{
						FMetaHumanPerformanceEditorToolkit* MetaHumanPerformanceEditorToolkit = Context->MetaHumanPerformanceEditorToolkit.Pin().Get();

						FToolMenuSection& ProcessingSection = InMenu->AddSection(TEXT("Processing"));
						{
							ProcessingSection.AddEntry(
								FToolMenuEntry::InitToolBarButton(
									Commands.StartProcessingShot,
									Commands.StartProcessingShot->GetLabel(),
									TAttribute<FText>::CreateSP(MetaHumanPerformanceEditorToolkit, &FMetaHumanPerformanceEditorToolkit::GetStartProcessingShotButtonTooltipText),
									FSlateIcon{ FMetaHumanPerformanceStyle::Get().GetStyleSetName(), TEXT("Performance.Toolbar.StartProcessingShot") }
								)
							);
							ProcessingSection.AddEntry(
								FToolMenuEntry::InitToolBarButton(
									Commands.CancelProcessingShot,
									Commands.CancelProcessingShot->GetLabel(),
									TAttribute<FText>::CreateSP(MetaHumanPerformanceEditorToolkit, &FMetaHumanPerformanceEditorToolkit::GetCancelProcessingShotButtonTooltipText),
									FSlateIcon{ FMetaHumanPerformanceStyle::Get().GetStyleSetName(), TEXT("Performance.Toolbar.CancelProcessingShot") }
								)
							);
						}

						FToolMenuSection& ExportSection = InMenu->AddSection(TEXT("Export"));
						{
							ExportSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.ExportAnimation,
																					 TAttribute<FText>{},
																					 TAttribute<FText>{},
																					 FSlateIcon{ FMetaHumanPerformanceStyle::Get().GetStyleSetName(), TEXT("Performance.Toolbar.ExportAnimation") }));

							ExportSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.ExportLevelSequence,
																					 TAttribute<FText>{},
																					 TAttribute<FText>{},
																					 FSlateIcon{ FMetaHumanPerformanceStyle::Get().GetStyleSetName(), TEXT("Performance.Toolbar.ExportLevelSequence") }));
						}
					}
				})
			);
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::ExtendMenu()
{
	const FMetaHumanPerformanceCommands& Commands = FMetaHumanPerformanceCommands::Get();

	const FName PerformanceMenuName = UToolMenus::JoinMenuPaths(GetToolMenuAppName(), TEXT("Performance"));

	UToolMenus* ToolMenus = UToolMenus::Get();

	if (!ToolMenus->IsMenuRegistered(PerformanceMenuName))
	{
		UToolMenu* PerformanceMenu = ToolMenus->RegisterMenu(PerformanceMenuName);

		FToolMenuSection& ProcessingSection = PerformanceMenu->AddSection(TEXT("PerformanceMenuProcessing"), LOCTEXT("PerformanceMenuProcessingSection", "Processing"));
		{
			ProcessingSection.AddMenuEntry(
				Commands.StartProcessingShot,
				Commands.StartProcessingShot->GetLabel(),
				TAttribute<FText>::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::GetStartProcessingShotButtonTooltipText),
				Commands.StartProcessingShot->GetIcon()
			);
			ProcessingSection.AddMenuEntry(
				Commands.CancelProcessingShot,
				Commands.CancelProcessingShot->GetLabel(),
				TAttribute<FText>::CreateSP(this, &FMetaHumanPerformanceEditorToolkit::GetCancelProcessingShotButtonTooltipText),
				Commands.CancelProcessingShot->GetIcon()
			);
		}
		FToolMenuSection& ExportAnimationSection = PerformanceMenu->AddSection(TEXT("PerformanceMenuExportAnimation"), LOCTEXT("PerformanceMenuExportAnimationSection", "Animation Export"));
		{
			ExportAnimationSection.AddMenuEntry(Commands.ExportAnimation);
			ExportAnimationSection.AddMenuEntry(Commands.ExportLevelSequence);
		}
	}

	const FName PerformanceMainMenuName = UToolMenus::JoinMenuPaths(GetToolMenuName(), TEXT("Performance"));

	if (!ToolMenus->IsMenuRegistered(PerformanceMainMenuName))
	{
		ToolMenus->RegisterMenu(PerformanceMainMenuName, PerformanceMenuName);
	}

	if (UToolMenu* MainMenu = ToolMenus->ExtendMenu(GetToolMenuName()))
	{
		const FToolMenuInsert MenuInsert{ TEXT("Tools"), EToolMenuInsertType::After };

		FToolMenuSection& Section = MainMenu->FindOrAddSection(NAME_None);

		FToolMenuEntry& PerformanceEntry = Section.AddSubMenu(TEXT("Performance"),
			LOCTEXT("PerformanceEditorPerformanceMenuLabel", "MetaHuman Animator"),
			LOCTEXT("PerformanceEditorPerformanceMenuTooltip", "Commands used in MetaHuman Animator workflow"),
			FNewToolMenuChoice{});

		PerformanceEntry.InsertPosition = MenuInsert;
	}
};

void FMetaHumanPerformanceEditorToolkit::HandleDataInputTypeChanged(EDataInputType InDataInputType)
{
	// Refresh the customization
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FMetaHumanPerformanceEditorToolkit::HandleSourceDataChanged(UFootageCaptureData* InFootageCaptureData, USoundWave* InAudio, bool bInResetRanges)
{
	check(TimelineSequencer.IsValid());
	check(Sequence);

	ClearMediaTracks();

	DestroyDepthMeshComponent();

	if (FootageComponent != nullptr)
	{
		for (UStaticMeshComponent* FootagePlaneComponent : FootageComponent->GetFootagePlaneComponents())
		{
			PreviewScene->RemoveComponent(FootagePlaneComponent);
			FootagePlaneComponent->DestroyComponent();
		}

		FootageComponent->DestroyComponent();
		FootageComponent = nullptr;
	}


	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	Sequence->SetTickRate(InFootageCaptureData);

	const FFrameRate TickRate = MovieScene->GetTickResolution();
	TMap<TWeakObjectPtr<UObject>, TRange<FFrameNumber>> MediaFrameRanges;
	TRange<FFrameNumber> ProcessingFrameRange(0, 0);
	TRange<FFrameNumber> MaxFrameRange;

	if (Performance->InputType == EDataInputType::Audio)
	{
		if (InAudio)
		{
			TRange<FFrameNumber> AudioFrameRange = UFootageCaptureData::GetAudioFrameRange(TickRate, Performance->TimecodeAlignment, InAudio, Performance->GetAudioMediaTimecode(), Performance->GetAudioMediaTimecodeRate());
			MaxFrameRange = AudioFrameRange;
			ProcessingFrameRange = AudioFrameRange;
			MediaFrameRanges.Add(InAudio, AudioFrameRange);
		}
	}
	else if (InFootageCaptureData && InFootageCaptureData->IsInitialized(UFootageCaptureData::EInitializedCheck::ImageSequencesOnly))
	{
		InFootageCaptureData->GetFrameRanges(TickRate, Performance->TimecodeAlignment, true, MediaFrameRanges, ProcessingFrameRange, MaxFrameRange);
	}

	if (Performance->InputType != EDataInputType::Audio && InFootageCaptureData && InFootageCaptureData->IsInitialized(UFootageCaptureData::EInitializedCheck::ImageSequencesOnly))
	{
		// Get the view index
		int32 ViewIndex = InFootageCaptureData->GetViewIndexByCameraName(Performance->Camera);

		FIntPoint TrackerImageSize;

		// Set the colour and depth tracks
		if (ViewIndex >= 0 && ViewIndex < InFootageCaptureData->ImageSequences.Num())
		{
			const TObjectPtr<class UImgMediaSource> ImageSequence = InFootageCaptureData->ImageSequences[ViewIndex];

			if (ImageSequence)
			{
				FTimecode ImageTimecode = UImageSequenceTimecodeUtils::GetTimecode(ImageSequence.Get());

				const TRange<FFrameNumber>& ImageFrameRange = MediaFrameRanges[ImageSequence];
				SetMediaTrack(EMediaTrackType::Colour, UMetaHumanPerformanceMovieSceneMediaTrack::StaticClass(), ImageSequence, ImageTimecode, ImageFrameRange.GetLowerBoundValue());

				// Set the Performance in all sections in sections
				for (UMovieSceneSection* MediaSection : ColourMediaTrack->GetAllSections())
				{
					if (UMetaHumanPerformanceMovieSceneMediaSection* PerformanceMediaSection = Cast<UMetaHumanPerformanceMovieSceneMediaSection>(MediaSection))
					{
						PerformanceMediaSection->PerformanceShot = Performance;
					}
				}

				FIntPoint OLDImageDimensions = InFootageCaptureData->GetFootageColorResolution();
				int32 NumImageFrames = 0;
				FIntVector2 ImDims;
				FImageSequenceUtils::GetImageSequenceInfoFromAsset(ImageSequence, ImDims, NumImageFrames);
				TrackerImageSize = FIntPoint(ImDims.X, ImDims.Y);

				// Update Image Review texture
				ImageViewerBrush.SetResourceObject(ColourMediaTexture);
				ImageViewerBrush.SetImageSize(FVector2f(ImDims.X, ImDims.Y));
				if (ImageViewer.IsValid())
				{
					ImageViewer->ResetView();
				}
			}
			else
			{
				ImageViewerBrush.SetResourceObject(nullptr);
			}
		}

		if (Performance->InputType == EDataInputType::DepthFootage && ViewIndex >= 0 && ViewIndex < InFootageCaptureData->DepthSequences.Num())
		{
			const TObjectPtr<class UImgMediaSource> DepthSequence = InFootageCaptureData->DepthSequences[ViewIndex];

			if (DepthSequence)
			{
				FTimecode DepthTimecode = UImageSequenceTimecodeUtils::GetTimecode(DepthSequence.Get());

				const TRange<FFrameNumber>& DepthFrameRange = MediaFrameRanges[DepthSequence];
				SetMediaTrack(EMediaTrackType::Depth, UMetaHumanPerformanceMovieSceneMediaTrack::StaticClass(), DepthSequence, DepthTimecode, DepthFrameRange.GetLowerBoundValue());

				for (UMovieSceneSection* MediaSection : DepthMediaTrack->GetAllSections())
				{
					if (UMetaHumanPerformanceMovieSceneMediaSection* PerformanceMediaSection = Cast<UMetaHumanPerformanceMovieSceneMediaSection>(MediaSection))
					{
						PerformanceMediaSection->PerformanceShot = Performance;
					}
				}
			}
		}

		// Add the footage component to the scene
		if (ColourMediaTrack)
		{
			if (USceneComponent* PreviewComponent = MetaHumanCaptureDataUtils::CreatePreviewComponent(InFootageCaptureData, PreviewActor))
			{
				FootageComponent = Cast<UMetaHumanFootageComponent>(PreviewComponent);
				if (FootageComponent != nullptr)
				{
					FootageComponent->SetCamera(Performance->Camera);
					FootageComponent->SetMediaTextures(ColourMediaTexture, DepthMediaTexture);

					PreviewActor->AddOwnedComponent(FootageComponent);
					for (UPrimitiveComponent* FootagePlaneComponent : FootageComponent->GetFootagePlaneComponents())
					{
						PreviewScene->AddComponent(FootagePlaneComponent, FootagePlaneComponent->GetComponentTransform());
					}
				}
			}
		}

		GetMetaHumanPerformerViewportClient()->SetTrackerImageSize(TrackerImageSize);

		FLandmarkConfigIdentityHelper ConfigHelper;
		FFrameTrackingContourData ConfigData = ConfigHelper.GetDefaultContourDataFromConfig(FVector2D(TrackerImageSize.X, TrackerImageSize.Y), ECurvePresetType::Performance);

		// TODO: add actual config version to data initialization when curve editing becomes available in performance
		const FString ContourDataConfigVersion = "";
		CurveDataController->InitializeContoursFromConfig(ConfigData, ContourDataConfigVersion);

		if (!InFootageCaptureData->CameraCalibrations.IsEmpty())
		{
			CreateDepthMeshComponent(InFootageCaptureData->CameraCalibrations[0]);
			SetDepthMeshTexture(DepthMediaTexture);
		}
	}
	else
	{
		CurveDataController->ClearContourData();
		ImageViewerBrush.SetResourceObject(nullptr);
	}

	// Set the audio track
	if (InAudio)
	{
		const TRange<FFrameNumber>& AudioFrameRange = MediaFrameRanges[InAudio];
		SetMediaTrack(UMetaHumanPerformanceMovieSceneAudioTrack::StaticClass(), InAudio, Performance->GetAudioMediaTimecode(), AudioFrameRange.GetLowerBoundValue());

		for (UMovieSceneSection* AudioSection : AudioMediaTrack->GetAllSections())
		{
			if (UMetaHumanPerformanceMovieSceneAudioSection* PerformanceAudioSection = Cast<UMetaHumanPerformanceMovieSceneAudioSection>(AudioSection))
			{
				PerformanceAudioSection->PerformanceShot = Performance;
			}
		}
	}

	if (bInResetRanges && MaxFrameRange.HasLowerBound() && MaxFrameRange.HasUpperBound())
	{
		// Set the view range to match the maximum extent of the tracks
		FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();
		const float ViewTimeOffset = .1f;
		EditorData.WorkStart = TickRate.AsSeconds(MaxFrameRange.GetLowerBoundValue()) - ViewTimeOffset;
		EditorData.WorkEnd = TickRate.AsSeconds(MaxFrameRange.GetUpperBoundValue()) + ViewTimeOffset;
		EditorData.ViewStart = EditorData.WorkStart;
		EditorData.ViewEnd = EditorData.WorkEnd;

		MovieScene->SetPlaybackRange(MaxFrameRange);
	}

	// Need to refresh Sequencer so the new playback ranges are updated accordingly in the UI
	TimelineSequencer->RefreshTree();

	if (bInResetRanges)
	{
		TimelineSequencer->SetGlobalTime(ProcessingFrameRange.GetLowerBoundValue());
	}

	HandleSequencerGlobalTimeChanged();
}

void FMetaHumanPerformanceEditorToolkit::HandleIdentityChanged(UMetaHumanIdentity* InIdentity)
{
	HandleVisualizeMeshChanged(Performance->VisualizationMesh);

	HandleSequencerGlobalTimeChanged();

	GetMetaHumanPerformerViewportClient()->ResetABWipePostion();
}

void FMetaHumanPerformanceEditorToolkit::HandleVisualizeMeshChanged(USkeletalMesh* InVisualizeMesh)
{
	check(TimelineSequencer.IsValid());
	check(Sequence);

	UpdateVisualizationMesh(Performance->VisualizationMesh);

	MeshOffset = FVector::ZeroVector;

	UMovieScene* MovieScene = Sequence->GetMovieScene();

	// Remove the corresponding tracks from sequencer
	for (const FGuid& PossessableToRemove : { PerformerActorBindingId, PerformerFaceBindingId })
	{
		if (PossessableToRemove.IsValid())
		{
			MovieScene->RemovePossessable(PossessableToRemove);
			Sequence->UnbindPossessableObjects(PossessableToRemove);
		}
	}

	PerformerActorBindingId.Invalidate();
	PerformerFaceBindingId.Invalidate();

	// This needs to be called to inform sequencer that something has changed or it will crash
	TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemRemoved);

	if (FaceSkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		FVector SolverPosition = FVector::ZeroVector;
		FVector MeshPosition = FVector::ZeroVector;
		FName NoseBoneName = "FACIAL_C_12IPV_NoseUpper2"; // Bone used to account for variations in the heights of MetaHumans

		if (Performance->Identity)
		{
			if (UMetaHumanIdentityFace* Face = Performance->Identity->FindPartOfClass<UMetaHumanIdentityFace>())
			{
				if (Face->RigComponent)
				{
					SolverPosition = UMetaHumanPerformance::GetSkelMeshReferenceBoneLocation(Face->RigComponent, NoseBoneName);
				}
			}

			// Set the actor label to be the name of the identity as sequencer uses this label to name the actor track
			PreviewActor->SetActorLabel(Performance->Identity->GetName());
		}
		else
		{
			FTransform NoseBoneTransform;
			USkeleton* MetaHumanSkeleton = LoadObject<USkeleton>(nullptr, TEXT("/MetaHuman/IdentityTemplate/Face_Archetype_Skeleton.Face_Archetype_Skeleton"));

			if (MetaHumanSkeleton)
			{
				if (UMetaHumanPerformanceExportUtils::GetBoneGlobalTransform(MetaHumanSkeleton, NoseBoneName, NoseBoneTransform))
				{
					SolverPosition = NoseBoneTransform.GetLocation();
				}
			}
		}

		const TArray<TWeakObjectPtr<AActor>> Actors = { PreviewActor };
		// We need to ensure that external systems will not hold any references to the PreviewActor since
		// the actor lifetime is loosely managed by the performance editor (effectively left to the GC for cleanup),
		// and any reference outside the editor will be invalidated when the asset is deleted.
		// By disabling this flag, the Sequencer will not set the actor as the globally (editor wide) selected object,
		// while the Sequencer functionality should be the same. The only noticeable difference is that the
		// rig mesh visualization is missing the orange selection outline.
		const bool bSelectActors = false;
		const TArray<FGuid> ActorIds = TimelineSequencer->AddActors(Actors, bSelectActors);
		if (!ActorIds.IsEmpty() && ActorIds[0].IsValid())
		{
			PerformerActorBindingId = ActorIds[0];

			if (FMovieScenePossessable* ActorPossessable = MovieScene->FindPossessable(PerformerActorBindingId))
			{
				ActorPossessable->SetName(GetPossessableNameActor());
				TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::RefreshTree);
			}

			// Find the Skeletal Mesh component to create a transform track for it
			FMovieScenePossessable* FaceComponentPossessable = MovieScene->FindPossessable([this](const FMovieScenePossessable& Possessable)
			{
				return Possessable.GetName() == FaceSkeletalMeshComponent->GetName();
			});

			if (FaceComponentPossessable)
			{
				PerformerFaceBindingId = FaceComponentPossessable->GetGuid();

				// Set the name of the possessable that will be reflected in the Sequencer tree view
				FaceComponentPossessable->SetName(GetPossessableNameSkeletalMesh());
			}

			// An offset to apply to the visualization mesh to attempt to make it appear at the
			// same position as the solver model would appear. Can only be approximate due to different
			// geometry between solver model and visualization mesh. But this is enough to largely account
			// for the any height difference between the two.
			MeshPosition = UMetaHumanPerformance::GetSkelMeshReferenceBoneLocation(FaceSkeletalMeshComponent, NoseBoneName);
			
			if (!SolverPosition.IsZero() && !MeshPosition.IsZero())
			{
				MeshOffset = SolverPosition - MeshPosition;

				// Better results if you just account for height
				MeshOffset.X = 0;
				MeshOffset.Y = 0;
			}
		}

		TimelineSequencer->RefreshTree();
	}

	HandleSequencerGlobalTimeChanged();

	// This needs to be called as the performer actor binding changes when the mesh being visualized is replaced
	HandleControlRigClassChanged(Performance->ControlRigClass);

	// Updates the initial placement of the mesh
	HandleHeadMovementModeChanged(Performance->HeadMovementMode);

	// Hack to force a refresh of the rig - without this the rig is correctly positioned, but not animated
	const FFrameTime CurrentGlobalTime = TimelineSequencer->GetGlobalTime().ConvertTo(MovieScene->GetTickResolution());
	TimelineSequencer->SetGlobalTime(CurrentGlobalTime + 1);
	TimelineSequencer->SetGlobalTime(CurrentGlobalTime);
}

void FMetaHumanPerformanceEditorToolkit::HandleControlRigClassChanged(TSubclassOf<UControlRig> InControlRigClass)
{
	RecordControlRig = nullptr;

	if (InControlRigClass != nullptr)
	{
		if (PerformerFaceBindingId.IsValid())
		{
			UMovieScene* MovieScene = Sequence->GetMovieScene();

			// Remove the existing control rig track, a new one will be created next
			if (UMovieSceneControlRigParameterTrack* ControlRigTrack = MovieScene->FindTrack<UMovieSceneControlRigParameterTrack>(PerformerFaceBindingId))
			{
				MovieScene->RemoveTrack(*ControlRigTrack);
			}

			// Create the ControlRig instance to generate sequencer keys
			// Using this separate control rig instance prevents data races to evaluate the control rig stored in the sequencer section
			RecordControlRig = NewObject<UControlRig>(Performance, InControlRigClass);
			if (RecordControlRig != nullptr)
			{
				RecordControlRig->Initialize();
				RecordControlRig->Evaluate_AnyThread();
			}

			if (UMovieSceneControlRigParameterTrack* ControlRigTrack = MovieScene->AddTrack<UMovieSceneControlRigParameterTrack>(PerformerFaceBindingId))
			{
				UClass* ControlRigClass = InControlRigClass;
				FString ObjectName = (ControlRigClass->GetName());
				ObjectName.RemoveFromEnd(TEXT("_C"));

				UControlRig* ControlRig = NewObject<UControlRig>(ControlRigTrack, ControlRigClass, FName(*ObjectName), RF_Transactional);
				ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
				ControlRig->GetObjectBinding()->BindToObject(FaceSkeletalMeshComponent);
				ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
				ControlRig->Initialize();
				ControlRig->Evaluate_AnyThread();

				ControlRigTrack->Modify();
				ControlRigTrack->SetTrackName(FName{ *ObjectName });
				ControlRigTrack->SetDisplayName(FText::FromString(ObjectName));

				constexpr bool bSequencerOwnsControlRig = true;
				UMovieSceneControlRigParameterSection* ControlRigSection = CastChecked<UMovieSceneControlRigParameterSection>(ControlRigTrack->CreateControlRigSection(0, ControlRig, bSequencerOwnsControlRig));
				ControlRigSection->Modify();

				TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
				TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
				TimelineSequencer->ObjectImplicitlyAdded(ControlRig);

				// Repopulate the control rig track with existing animation data, if any
				const TArray64<FFrameAnimationData>& AnimationData = Performance->AnimationData;
				const int32 ProcessingLimitStartFrame = Performance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value;
				const FTransform ReferenceTransform = Performance->CalculateReferenceFramePose();
				for (int32 AnimationFrameIndex = 0; AnimationFrameIndex < AnimationData.Num(); ++AnimationFrameIndex)
				{
					if (AnimationData[AnimationFrameIndex].ContainsData())
					{
						UMetaHumanPerformanceExportUtils::BakeControlRigAnimationData(Performance, Sequence, AnimationFrameIndex + ProcessingLimitStartFrame, ControlRigSection, ReferenceTransform, GetInterpolationMode(AnimationFrameIndex + ProcessingLimitStartFrame), RecordControlRig, MeshOffset);
					}
				}

				UMetaHumanPerformanceExportUtils::SetHeadControlSwitchEnabled(ControlRigTrack, Performance->HeadMovementMode == EPerformanceHeadMovementMode::ControlRig);

				TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

				// Spawn all the control rig shapes in both viewports
				ControlRigManager.SetControlRig(ControlRig);
				ControlRigManager.SetFaceBoardShapeColor(FLinearColor::Gray);
				ControlRigComponent->SetControlRig(ControlRig);
			}
		}
		else
		{
			// If we are here it means that we don't have an identity in the scene, so we destroy all control rig shapes
			ControlRigManager.SetControlRig(nullptr);
			ControlRigComponent->SetControlRig(nullptr);
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleHeadMovementReferenceFrameChanged(bool bInAutoChooseHeadMovementReferenceFrame, uint32 InHeadMovementReferenceFrame)
{
	UpdateControlRigHeadPose();
}

void FMetaHumanPerformanceEditorToolkit::HandleHeadMovementModeChanged(EPerformanceHeadMovementMode InHeadMovementMode)
{
	check(Sequence);

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	const FTransform ReferenceFrameTransform = Performance->CalculateReferenceFramePose();

	if (InHeadMovementMode == EPerformanceHeadMovementMode::Disabled || ReferenceFrameTransform.Equals(FTransform::Identity))
	{
		// Default transform is used at height of nose tip bone
		FName NoseBoneName = "FACIAL_C_12IPV_NoseTip2";
		FVector SkelMeshPosition = UMetaHumanPerformance::GetSkelMeshReferenceBoneLocation(FaceSkeletalMeshComponent, NoseBoneName);
		FTransform DefaultRootTransform;
		DefaultRootTransform.SetTranslation(FVector(50, 0, -SkelMeshPosition.Z));
		DefaultRootTransform.SetRotation(FQuat(FRotator(0, 90, 0)));
		FaceSkeletalMeshComponent->SetWorldTransform(DefaultRootTransform);
	}
	else
	{
		// Set transform using reference frame
		FaceSkeletalMeshComponent->SetWorldTransform(ReferenceFrameTransform);
	}

	if (PerformerFaceBindingId.IsValid())
	{
		// Remove any existing transform track
		if (UMovieScene3DTransformTrack* HeadTransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(PerformerFaceBindingId))
		{
			MovieScene->RemoveTrack(*HeadTransformTrack);
		}

		if (InHeadMovementMode == EPerformanceHeadMovementMode::TransformTrack)
		{
			if (UMovieScene3DTransformTrack* TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(PerformerFaceBindingId))
			{
				if (UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection()))
				{
					TransformSection->Modify();
					TransformSection->SetMask(FMovieSceneTransformMask{ EMovieSceneTransformChannel::All });

					const FVector Location = FaceSkeletalMeshComponent->GetComponentLocation();
					const FVector Rotation = FaceSkeletalMeshComponent->GetComponentRotation().Euler();
					const FVector Scale = FaceSkeletalMeshComponent->GetComponentScale();

					TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
					DoubleChannels[0]->SetDefault(Location.X);
					DoubleChannels[1]->SetDefault(Location.Y);
					DoubleChannels[2]->SetDefault(Location.Z);

					DoubleChannels[3]->SetDefault(Rotation.X);
					DoubleChannels[4]->SetDefault(Rotation.Y);
					DoubleChannels[5]->SetDefault(Rotation.Z);

					DoubleChannels[6]->SetDefault(Scale.X);
					DoubleChannels[7]->SetDefault(Scale.Y);
					DoubleChannels[8]->SetDefault(Scale.Z);

					TransformSection->SetRange(TRange<FFrameNumber>::All());

					TransformTrack->AddSection(*TransformSection);

					// Populate the transform track with existing animation data, if any
					const TArray64<FFrameAnimationData>& AnimationData = Performance->AnimationData;
					const int32 ProcessingLimitStartFrame = Performance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value;
					for (int32 AnimationFrameIndex = 0; AnimationFrameIndex < AnimationData.Num(); ++AnimationFrameIndex)
					{
						if (AnimationData[AnimationFrameIndex].ContainsData())
						{
							UMetaHumanPerformanceExportUtils::BakeTransformAnimationData(Performance, Sequence, AnimationFrameIndex + ProcessingLimitStartFrame, TransformSection, GetInterpolationMode(AnimationFrameIndex + ProcessingLimitStartFrame), FTransform::Identity, MeshOffset);
						}
					}

					TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
				}
			}
		}

		// Enable or disable the Control Rig track Head Control Switch based on the head movement mode
		if (UMovieSceneControlRigParameterTrack* ControlRigTrack = MovieScene->FindTrack<UMovieSceneControlRigParameterTrack>(PerformerFaceBindingId))
		{
			UMetaHumanPerformanceExportUtils::SetHeadControlSwitchEnabled(ControlRigTrack, Performance->HeadMovementMode == EPerformanceHeadMovementMode::ControlRig);
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleNeutralPoseCalibrationChanged()
{
	UpdateControlRigHeadPose(); // A means for rebaking the control rig animation data for all frames
}

void FMetaHumanPerformanceEditorToolkit::HandleFootageDepthDataChanged(float InNear, float InFar)
{
	if (FootageComponent != nullptr)
	{
		FootageComponent->SetDepthRange(InNear, InFar);

		HandleSequencerGlobalTimeChanged();
		GetMetaHumanPerformerViewportClient()->Invalidate();
	}
}

void FMetaHumanPerformanceEditorToolkit::InitToolMenuContext(struct FToolMenuContext& InMenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(InMenuContext);

	UMetaHumanPerformanceEditorContext* Context = NewObject<UMetaHumanPerformanceEditorContext>();
	Context->MetaHumanPerformanceEditorToolkit = SharedThis(this);
	InMenuContext.AddObject(Context);
}

void FMetaHumanPerformanceEditorToolkit::HandleFrameRangeChanged(int32 InStartFrame, int32 InEndFrame)
{
	check(Sequence);

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	const FFrameNumber StartFrameNumber{ InStartFrame };
	const FFrameNumber EndFrameNumber{ InEndFrame };

	const FFrameRate TickRate = MovieScene->GetTickResolution();
	const FFrameRate FrameRate = GetFrameRate();

	const FFrameTime TransformedStartFrameNumber = FFrameRate::TransformTime(StartFrameNumber, FrameRate, TickRate);
	const FFrameTime TransformedEndFrameNumber = FFrameRate::TransformTime(EndFrameNumber, FrameRate, TickRate);

	const TRange<FFrameNumber> TransformedFrameRange{ TransformedStartFrameNumber.GetFrame(), TransformedEndFrameNumber.GetFrame() };

	MovieScene->SetPlaybackRange(TransformedFrameRange);
}

void FMetaHumanPerformanceEditorToolkit::HandleRealtimeAudioChanged(bool bInRealtimeAudio)
{
	// Refresh the customization
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FMetaHumanPerformanceEditorToolkit::HandleFrameProcessed(int32 InFrameNumber)
{
	MHA_CPUPROFILER_EVENT_SCOPE(FMetaHumanPerformanceEditorToolkit::HandleFrameProcessed);

	const FFrameNumber CurrentFrameNumber = GetCurrentFrameNumber();
	if (CurrentFrameNumber.Value == InFrameNumber)
	{
		HandleSequencerGlobalTimeChanged();
	}

	UpdateSequencerAnimationData(InFrameNumber);

	if (bShowFramesAsTheyAreProcessed)
	{
		if (Performance->GetPipelineStage() == 0 || Performance->GetPipelineStage() == 1)
		{
			const FFrameRate TickRate = Sequence->GetMovieScene()->GetTickResolution();
			const FFrameRate FrameRate = GetFrameRate();
			const FFrameTime FrameTime = FFrameRate::TransformTime(InFrameNumber, FrameRate, TickRate);
			TimelineSequencer->SetGlobalTime(FrameTime);
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleStage1ProcessingFinished()
{
	// rebake to controlrig if needed as we can now calculate the best frame needed to define the head transform
	if (Performance->HeadMovementMode == EPerformanceHeadMovementMode::ControlRig)
	{
		UpdateControlRigHeadPose();
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleProcessingFinished(TSharedPtr<const UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	if (InPipelineData->GetExitStatus() != UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok &&
		InPipelineData->GetExitStatus() != UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted)
	{
		if (!InPipelineData->GetErrorNodeName().IsEmpty() && InPipelineData->GetErrorNodeName().Equals("Solver", ESearchCase::IgnoreCase) &&
			InPipelineData->GetErrorNodeCode() == UE::MetaHuman::Pipeline::FFaceTrackerIPhoneManagedNode::ErrorCode::UntrainedSolvers)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PipelineUntrainedSolversError", "The Performance Pipeline cannot be processed because the MetaHuman Identity has not been prepared to process performances.\n\nUse the \"Prepare for Performance\" button in the MetaHuman Identity editor to prepare the MetaHuman Identity."));
		}
		else if (!InPipelineData->GetErrorNodeName().IsEmpty() && InPipelineData->GetErrorNodeName().Equals("Solver", ESearchCase::IgnoreCase) &&
			InPipelineData->GetErrorNodeCode() == UE::MetaHuman::Pipeline::FFaceTrackerIPhoneManagedNode::ErrorCode::NoContourData)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("PipelineFrameContourTrackingFailedError", "The Performance Pipeline has failed because it failed to detect a face in frame {0}.\n\nTry excluding this frame and any others where the face is occluded or not visible from the Processing Range."),
					InPipelineData->GetFrameNumber()));
		}
		else
		{
			// TODO a more specific error is needed here
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PipelineProcessingError", "The Processing Pipeline failed with an error."));
		}
		UE_LOG(LogMetaHumanPerformance, Warning, TEXT("The Processing Pipeline failed with an error: %s"), *InPipelineData->GetErrorMessage());
	}
	else
	{
		check(Performance);

		if (InPipelineData->GetExitStatus() != UE::MetaHuman::Pipeline::EPipelineExitStatus::Aborted)
		{
			FText DiagnosticsWarningMessage;
			if (Performance->DiagnosticsIndicatesProcessingIssue(DiagnosticsWarningMessage))
			{
				FMessageDialog::Open(EAppMsgType::Ok, DiagnosticsWarningMessage, LOCTEXT("PipelineProcessingDiagnosticsWarningTitle", "Processing Pipeline Diagnostics Warning"));
				UE_LOG(LogMetaHumanPerformance, Warning, TEXT("The Processing Pipeline diagnostics check found a potential issue with the data: %s"), *DiagnosticsWarningMessage.ToString());
			}
		}
	}

	// rebake to controlrig if needed as we can now calculate the best frame needed to define the head transform
	if (Performance->HeadMovementMode == EPerformanceHeadMovementMode::ControlRig)
	{
		UpdateControlRigHeadPose();
	}

	// rebake to controlrig now we have calculated the neutral pose calibration frame animation values
	if (Performance->bNeutralPoseCalibrationEnabled)
	{
		HandleNeutralPoseCalibrationChanged();
	}

	SetControlsEnabled(true);
}

void FMetaHumanPerformanceEditorToolkit::UpdateControlRigHeadPose()
{
	check(Performance);
	if (Performance->ContainsAnimationData() && PerformerFaceBindingId.IsValid())
	{
		check(TimelineSequencer);
		check(Sequence);

		UMovieScene* MovieScene = Sequence->GetMovieScene();
		check(MovieScene);

		if (UMovieSceneControlRigParameterTrack* ControlRigTrack = MovieScene->FindTrack<UMovieSceneControlRigParameterTrack>(PerformerFaceBindingId))
		{
			check(!ControlRigTrack->GetAllSections().IsEmpty());
			if (UMovieSceneControlRigParameterSection* ControlRigSection = Cast<UMovieSceneControlRigParameterSection>(ControlRigTrack->GetAllSections()[0]))
			{
				// The sequencer should not update the viewport while we are updating the keys
				TimelineSequencer->EnterSilentMode();
				FTransform ReferenceTransform = Performance->CalculateReferenceFramePose();
				const TArray64<FFrameAnimationData>& AnimationData = Performance->AnimationData;
				const int32 ProcessingLimitStartFrame = Performance->GetProcessingLimitFrameRange().GetLowerBoundValue().Value;

				ControlRigSection->Modify();

				for (int32 AnimationFrameIndex = 0; AnimationFrameIndex < AnimationData.Num(); ++AnimationFrameIndex)
				{
					if (AnimationData[AnimationFrameIndex].ContainsData())
					{
						UMetaHumanPerformanceExportUtils::BakeControlRigAnimationData(Performance, Sequence, AnimationFrameIndex + ProcessingLimitStartFrame, ControlRigSection, ReferenceTransform, GetInterpolationMode(AnimationFrameIndex + ProcessingLimitStartFrame), RecordControlRig, MeshOffset);
					}
				}

				// Re-enable the sequencer updates
				TimelineSequencer->ExitSilentMode();

				// Finally notify sequencer that a value changed so it can refresh the UI
				TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
			}
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::UpdatePostProcessAnimBP()
{
	if (Performance->HeadMovementMode == EPerformanceHeadMovementMode::ControlRig)
	{
		// The head movement IK global switch is disable by default, enable it here
		// in case the user selects ControlRig as the active head movement
		if (UAnimInstance* PostProcessInstance = FaceSkeletalMeshComponent->GetPostProcessInstance())
		{
			MetaHumanComponentHelpers::ConnectVariable<FBoolProperty>(PostProcessInstance, TEXT("Enable Head Movement IK"), true);
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleGetViewABMenuContents(EABImageViewMode InViewMode, FMenuBuilder& InMenuBuilder)
{
	const FMetaHumanToolkitCommands& BaseCommands = FMetaHumanToolkitCommands::Get();
	const FMetaHumanPerformanceCommands& Commands = FMetaHumanPerformanceCommands::Get();

	InMenuBuilder.BeginSection(TEXT("GeometryExtensionsHook"), LOCTEXT("GeometrySectionLabel", "Geometry"));
	{
		InMenuBuilder.AddMenuEntry(Commands.ToggleRig);
		InMenuBuilder.AddMenuEntry(Commands.ToggleControlRigDisplay);

		if (Performance->InputType == EDataInputType::DepthFootage)
		{
			InMenuBuilder.AddMenuEntry(BaseCommands.ToggleDepthMesh);
		}
	}
	InMenuBuilder.EndSection();

	if (Performance->InputType == EDataInputType::DepthFootage)
	{
		InMenuBuilder.BeginSection(TEXT("FootageExtensionsHook"), LOCTEXT("FootageSectionLabel", "Video"));
		{
			InMenuBuilder.AddMenuEntry(Commands.ToggleFootage);
			InMenuBuilder.AddMenuEntry(BaseCommands.ToggleUndistortion);
		}
		InMenuBuilder.EndSection();
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleSequencerMovieSceneDataChanged(EMovieSceneDataChangeType InDataChangeType)
{
	check(Sequence);

	// When a value in the track changes or is undone
	if (InDataChangeType == EMovieSceneDataChangeType::TrackValueChanged || InDataChangeType == EMovieSceneDataChangeType::Unknown)
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		check(MovieScene);

		const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
		const FFrameNumber StartFrameNumber = PlaybackRange.GetLowerBoundValue();
		const FFrameNumber EndFrameNumber = PlaybackRange.GetUpperBoundValue();

		const FFrameRate TickRate = MovieScene->GetTickResolution();
		const FFrameRate FrameRate = GetFrameRate();

		const FFrameTime TransformedStartFrameNumber = FFrameRate::TransformTime(StartFrameNumber, TickRate, FrameRate);
		const FFrameTime TransformedEndFrameNumber = FFrameRate::TransformTime(EndFrameNumber, TickRate, FrameRate);

		const int32 ExistingStartFrameToProcess = Performance->StartFrameToProcess;
		const int32 ExistingEndFrameToProcess = Performance->EndFrameToProcess;

		const TRange<FFrameNumber> FrameRange = Performance->GetProcessingLimitFrameRange();
		Performance->StartFrameToProcess = FMath::Clamp(TransformedStartFrameNumber.FrameNumber.Value, FrameRange.GetLowerBoundValue().Value, FrameRange.GetUpperBoundValue().Value);
		Performance->EndFrameToProcess = FMath::Clamp(TransformedEndFrameNumber.FrameNumber.Value, FrameRange.GetLowerBoundValue().Value, FrameRange.GetUpperBoundValue().Value);

		if (Performance->StartFrameToProcess != ExistingStartFrameToProcess || Performance->EndFrameToProcess != ExistingEndFrameToProcess)
		{
			Performance->MarkPackageDirty();
		}

		HandleFrameRangeChanged(Performance->StartFrameToProcess, Performance->EndFrameToProcess);

		// change the head movement reference frame if this is now outside the valid range.
		uint32 HeadMovementReferenceFrame = FMath::Clamp(Performance->HeadMovementReferenceFrame, Performance->StartFrameToProcess, Performance->EndFrameToProcess);
		if (HeadMovementReferenceFrame != Performance->HeadMovementReferenceFrame)
		{
			Performance->HeadMovementReferenceFrame = HeadMovementReferenceFrame;
			HandleHeadMovementReferenceFrameChanged(Performance->bAutoChooseHeadMovementReferenceFrame, HeadMovementReferenceFrame);
		}

		// change the neutral pose calibration frame if this is now outside the valid range.
		uint32 NeutralPoseCalibrationFrame = FMath::Clamp(Performance->NeutralPoseCalibrationFrame, Performance->StartFrameToProcess, Performance->EndFrameToProcess);
		if (NeutralPoseCalibrationFrame != Performance->NeutralPoseCalibrationFrame)
		{
			Performance->NeutralPoseCalibrationFrame = NeutralPoseCalibrationFrame;
			HandleNeutralPoseCalibrationChanged();
		}

		// Something changed in the Movie Scene, so we force the UI to be locked if the pipeline is running
		if (Performance->IsProcessing())
		{
			SetControlsEnabled(false);
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleSequencerGlobalTimeChanged()
{
	FMetaHumanToolkitBase::HandleSequencerGlobalTimeChanged();

	if (CurveDataController.IsValid())
	{
		bool bUpdatedContourData = false;
		const FFrameNumber CurrentAnimationFrameNumber = GetCurrentAnimationFrameNumber();
		TArray64<FFrameTrackingContourData>& ContourTrackingResults = Performance->ContourTrackingResults;
		if (ContourTrackingResults.IsValidIndex(CurrentAnimationFrameNumber.Value))
		{
			const FFrameTrackingContourData& ContourData = ContourTrackingResults[CurrentAnimationFrameNumber.Value];

			if ((ContourData.Camera.IsEmpty() || ContourData.Camera == Performance->Camera) && ContourData.ContainsData())
			{
				CurveDataController->UpdateFromContourData(ContourData, true);
				bUpdatedContourData = true;
			}

		}
		if (!bUpdatedContourData)
		{
			CurveDataController->ClearDrawData();
		}
	}

	if (ControlRigComponent != nullptr)
	{
		ControlRigComponent->UpdateControlRigShapes();
	}
	ControlRigManager.UpdateControlRigShapes();

	if (!bIsToolkitInitializing)
	{
		// Only store the current sequencer time if not initializing the toolkit to prevent
		// the initialization code from overriding the stored frame time
		Performance->ViewportSettings->CurrentFrameTime = TimelineSequencer->GetGlobalTime().Time;
	}

	EFrameRangeType ExcludedFrameRangeType = Performance->GetExcludedFrame(GetCurrentFrameNumber().Value);
	FText Overlay;

	if (ExcludedFrameRangeType != EFrameRangeType::None)
	{
		// We don't show an overlay in the case of a frame being excluded due to frame rate matching. The reasoning
		// here is that this would happen every second frame for a 30/60 fps mismatch and the result would be very 
		// jarring for a user scrubbing through the timeline.
		if (ExcludedFrameRangeType != EFrameRangeType::RateMatchingExcluded)
		{
			UEnum::GetDisplayValueAsText(ExcludedFrameRangeType, Overlay);
		}
	}

	if (ImageViewer.IsValid())
	{
		ImageViewer->SetOverlay(Overlay);
	}

	if (ViewportClient && ViewportClient->GetEditorViewportWidget())
	{
		GetMetaHumanPerformerViewportClient()->SetOverlay(Overlay);
	}

	UpdatePostProcessAnimBP();
}

void FMetaHumanPerformanceEditorToolkit::DeleteSequencerKeysInProcessingRange()
{
	check(Sequence);

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	const FFrameRate FrameRate = GetFrameRate();
	const FFrameRate TickRate = MovieScene->GetTickResolution();
	const FFrameTime StartFrameTime = FFrameRate::TransformTime(static_cast<int32>(Performance->StartFrameToProcess), FrameRate, TickRate);
	const FFrameTime EndFrameTime = FFrameRate::TransformTime(static_cast<int32>(Performance->EndFrameToProcess - 1), FrameRate, TickRate);
	const TRange<FFrameNumber> FrameRange{ StartFrameTime.GetFrame(), EndFrameTime.GetFrame() };

	// Find a Section of type UMovieSceneControlRigParameterSection
	const TArray<UMovieSceneSection*>& Sections = MovieScene->GetAllSections();
	for (UMovieSceneSection* Section : Sections)
	{
		FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();

		for (FMovieSceneFloatChannel* FloatChannel : ChannelProxy.GetChannels<FMovieSceneFloatChannel>())
		{
			TArray<FKeyHandle> KeyHandles;
			FloatChannel->GetKeys(FrameRange, nullptr, &KeyHandles);
			FloatChannel->DeleteKeys(KeyHandles);
		}

		for (FMovieSceneDoubleChannel* DoubleChannel : ChannelProxy.GetChannels<FMovieSceneDoubleChannel>())
		{
			TArray<FKeyHandle> KeyHandles;
			DoubleChannel->GetKeys(FrameRange, nullptr, &KeyHandles);
			DoubleChannel->DeleteKeys(KeyHandles);
		}
	}

	// Notify sequencer that something has changed so it can be updated
	TimelineSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
}

void FMetaHumanPerformanceEditorToolkit::UpdateSequencerAnimationData(int32 InFrameNumber)
{
	MHA_CPUPROFILER_EVENT_SCOPE(FMetaHumanPerformanceEditorToolkit::UpdateSequencerAnimationData);

	check(TimelineSequencer);
	check(Performance);
	check(Sequence);

	// The sequencer should not update the viewport while we are processing the control rig and the recorded keys
	TimelineSequencer->EnterSilentMode();

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	// just use the first valid animation pose if we haven't calculated it from the full sequence
	FTransform ReferenceTransform;
	if (Performance->HeadMovementReferenceFrameCalculated == -1)
	{
		ReferenceTransform = Performance->GetFirstValidAnimationPose() ;
	}
	else
	{
		ReferenceTransform = Performance->AnimationData[Performance->HeadMovementReferenceFrameCalculated].Pose;
	}

	if (PerformerFaceBindingId.IsValid())
	{
		if (UMovieSceneControlRigParameterTrack* ControlRigTrack = MovieScene->FindTrack<UMovieSceneControlRigParameterTrack>(PerformerFaceBindingId))
		{
			check(!ControlRigTrack->GetAllSections().IsEmpty());
			if (UMovieSceneControlRigParameterSection* ControlRigSection = Cast<UMovieSceneControlRigParameterSection>(ControlRigTrack->GetAllSections()[0]))
			{
				ControlRigSection->Modify();
				UMetaHumanPerformanceExportUtils::BakeControlRigAnimationData(Performance, Sequence, InFrameNumber, ControlRigSection, ReferenceTransform, GetInterpolationMode(InFrameNumber), RecordControlRig, MeshOffset);
			}
		}
	}

	if (Performance->HeadMovementMode == EPerformanceHeadMovementMode::TransformTrack)
	{
		if (PerformerFaceBindingId.IsValid())
		{
			if (UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(PerformerFaceBindingId))
			{
				check(!TransformTrack->GetAllSections().IsEmpty());
				if (UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0]))
				{
					TransformSection->Modify();
					UMetaHumanPerformanceExportUtils::BakeTransformAnimationData(Performance, Sequence, InFrameNumber, TransformSection, GetInterpolationMode(InFrameNumber), FTransform::Identity, MeshOffset);
				}
			}
		}
	}

	// Re-enable the sequencer updates
	TimelineSequencer->ExitSilentMode();
}

FFrameRate FMetaHumanPerformanceEditorToolkit::GetFrameRate() const
{
	if (Performance)
	{
		const FFrameRate FrameRate = Performance->GetFrameRate();
		if (FrameRate.IsValid())
		{
			return FrameRate;
		}
	}

	// If the frame rate can't be determined, return the current display frame rate
	check(Sequence);

	if (UMovieScene* MovieScene = Sequence->GetMovieScene())
	{
		return MovieScene->GetDisplayRate();
	}

	return FFrameRate{};
}

FFrameNumber FMetaHumanPerformanceEditorToolkit::GetCurrentFrameNumber() const
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	const FFrameRate TickRate = MovieScene->GetTickResolution();
	const FFrameRate FrameRate = GetFrameRate();

	// This will be the current frame number being displayed by sequencer
	const FFrameTime CurrentFrameTime = TimelineSequencer->GetGlobalTime().ConvertTo(FrameRate);

	return CurrentFrameTime.GetFrame();
}

FFrameNumber FMetaHumanPerformanceEditorToolkit::GetCurrentAnimationFrameNumber() const
{
	return GetCurrentFrameNumber() - Performance->GetProcessingLimitFrameRange().GetLowerBoundValue();
}

void FMetaHumanPerformanceEditorToolkit::SetControlsEnabled(bool bIsEnabled)
{
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}

	if (Sequence)
	{
		Sequence->GetMovieScene()->SetReadOnly(!bIsEnabled);
	}
}

bool FMetaHumanPerformanceEditorToolkit::CanProcess() const
{
	return Performance && Performance->CanProcess();
}

bool FMetaHumanPerformanceEditorToolkit::CanCancel() const
{
	return Performance && Performance->IsProcessing();
}

bool FMetaHumanPerformanceEditorToolkit::CanExportAnimation() const
{
	return Performance && Performance->CanExportAnimation();
}

void FMetaHumanPerformanceEditorToolkit::HandleProcessButtonClicked()
{
	if (!Performance->IsProcessing())
	{
		bool bShouldStartProcessing = true;

		if (Performance->InputType == EDataInputType::DepthFootage)
		{
			// Warn the user if the Device Class has not been set in the footage data
			FString ConfigName;
			bool bDeviceModelSet = Performance->DefaultSolver->GetConfigDisplayName(Performance->FootageCaptureData, ConfigName);
			bool bDepthCameraConsistentWithRGBCamera = Performance->DepthCameraConsistentWithRGBCameraOrDiagnosticsNotEnabled();

			if (!bDeviceModelSet || !bDepthCameraConsistentWithRGBCamera)
			{
				bShouldStartProcessing = DisplayWarningsBeforeProcessing(bDeviceModelSet, bDepthCameraConsistentWithRGBCamera);
			}
		}

		if (bShouldStartProcessing)
		{
			const bool bIsScriptedProcessing = false;
			EStartPipelineErrorType StartPipelineError = Performance->StartPipeline(bIsScriptedProcessing);

			if (StartPipelineError == EStartPipelineErrorType::None)
			{
				DeleteSequencerKeysInProcessingRange();

				SetControlsEnabled(false);

				bShowFramesAsTheyAreProcessed = Performance->bShowFramesAsTheyAreProcessed;
			}
			else if (StartPipelineError == EStartPipelineErrorType::NoFrames)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PipelineNoFramesError", "No frames for processing have been selected"));
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PipelineUnknownError", "Unknown error starting processing pipeline"));
			}
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleCancelButtonClicked()
{
	if (Performance->IsProcessing())
	{
		const EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ShouldCancelProcessingPipeline", "Cancel processing the current shot?"));
		if (Response == EAppReturnType::Yes)
		{
			Performance->CancelPipeline();
		}
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleExportAnimationClicked()
{
	check(Sequence);

	UMovieScene* MovieScene = Sequence->GetMovieScene();

	const FFrameRate ProcessingRate = GetFrameRate();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	if (ProcessingRate != DisplayRate)
	{
		UE_LOG(LogMetaHumanPerformance, Warning, TEXT("The shot frame rate of %s doesn't match the current display frame rate of %s. The animation will be exported using the display rate."), *ProcessingRate.ToPrettyText().ToString(), *DisplayRate.ToPrettyText().ToString());
	}

	UMetaHumanPerformanceExportAnimationSettings* ExportSettings = UMetaHumanPerformanceExportUtils::GetExportAnimationSequenceSettings(Performance);
	ExportSettings->bShowExportDialog = true;

	UMetaHumanPerformanceExportUtils::ExportAnimationSequence(Performance, ExportSettings);

	if (GEngine->AreEditorAnalyticsEnabled() && FEngineAnalytics::IsAvailable())
	{
		bool bIsAnimationSequence = true;
		bool bIsWholeSequence = ExportSettings->ExportRange == EPerformanceExportRange::WholeSequence;
		SendTelemetryForPerformanceExportRequest(bIsAnimationSequence, bIsWholeSequence);
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleExportLevelSequenceClicked()
{
	if (!CanExportAnimation())
	{
		return;
	}

	UMetaHumanPerformanceExportLevelSequenceSettings* ExportSettings = UMetaHumanPerformanceExportUtils::GetExportLevelSequenceSettings(Performance);
	ExportSettings->bShowExportDialog = true;

	UMetaHumanPerformanceExportUtils::ExportLevelSequence(Performance, ExportSettings);

	if (GEngine->AreEditorAnalyticsEnabled() && FEngineAnalytics::IsAvailable())
	{
		const bool bIsAnimationSequence = false;
		const bool bIsWholeSequence = ExportSettings->ExportRange == EPerformanceExportRange::WholeSequence;
		SendTelemetryForPerformanceExportRequest(bIsAnimationSequence, bIsWholeSequence); //for level sequences, we always export whole sequence, so the second argument is true
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleImageReviewFocus()
{
	if (ImageViewer.IsValid())
	{
		ImageViewer->ResetView();
	}
}

bool FMetaHumanPerformanceEditorToolkit::DisplayWarningsBeforeProcessing(bool bInDeviceModelIsSet, bool bInConsistentRGBAndDepthCameras) const
{
	FText PerformanceWarningMessage;

	if (!bInDeviceModelIsSet)
	{
		PerformanceWarningMessage = FText(LOCTEXT("PerformanceWarnUnknownDeviceModelDialog_Message", "The Device Model in the footage has not been set. Default settings will be used and processing quality may be affected."));
	}
	if (!bInConsistentRGBAndDepthCameras)
	{
		FText SlowDiagnosticsMessage = FText(LOCTEXT("SlowDiagnosticsDialog_Message", "Setting the camera view to the non-default view will result in very slow processing if Processing Diagnostics are enabled. Please set 'Skip Diagnostics' in the Details Panel to disable Processing Diagnostics."));

		if (PerformanceWarningMessage.ToString().Len() > 0)
		{
			PerformanceWarningMessage = FText::FromString(PerformanceWarningMessage.ToString() + TEXT("\n\n") + SlowDiagnosticsMessage.ToString());
		}
		else
		{
			PerformanceWarningMessage = SlowDiagnosticsMessage;
		}
	}
	FSuppressableWarningDialog::FSetupInfo Info(
		PerformanceWarningMessage,
		LOCTEXT("PerformanceProcessingWarningDialog_Title", "Processing Warning"),
		TEXT("PerformanceProcessingWarningDialog"));
	Info.ConfirmText = LOCTEXT("PerformanceProcessingWarningDialog_ConfirmText", "Continue Processing");
	Info.CancelText = LOCTEXT("PerformanceProcessingWarningDialog_CancelText", "Cancel");

	FSuppressableWarningDialog ShouldRecordDialog(Info);
	FSuppressableWarningDialog::EResult UserInput = ShouldRecordDialog.ShowModal();

	return UserInput == FSuppressableWarningDialog::EResult::Cancel ? false : true;
}

void FMetaHumanPerformanceEditorToolkit::HandleViewSetupClicked(int32 InSlotIndex, bool bInStore)
{
	UMetaHumanEditorSettings* Settings = GetMutableDefault<UMetaHumanEditorSettings>();

	TArray<TMap<FString, FString>*> ViewSetupSlots = { &Settings->PerformanceViewSetupSlot1, &Settings->PerformanceViewSetupSlot2, &Settings->PerformanceViewSetupSlot3, &Settings->PerformanceViewSetupSlot4 };

	check(InSlotIndex >= 0 && InSlotIndex < ViewSetupSlots.Num());
	TMap<FString, FString>* ViewSetup = ViewSetupSlots[InSlotIndex];

	TSharedRef<FMetaHumanPerformanceViewportClient> PerformerViewportClient = GetMetaHumanPerformerViewportClient();

	if (bInStore)
	{
		ViewSetup->Reset();
	}

	for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
	{
		FString SideName = SideIndex == 0 ? TEXT("A") : TEXT("B");
		EABImageViewMode SideViewMode = SideIndex == 0 ? EABImageViewMode::A : EABImageViewMode::B;

		auto Item = [&](const FString& InKey, TFunction<bool()> InIsVisible, TFunction<void()> InToggleVisibility)
		{
			if (bInStore)
			{
				ViewSetup->Add(InKey + SideName, InIsVisible() ? TEXT("true") : TEXT("false"));
			}
			else
			{
				FString* Value = ViewSetup->Find(InKey + SideName);
				if (Value && ((*Value == TEXT("true") && !InIsVisible()) || (*Value == TEXT("false") && InIsVisible())))
				{
					InToggleVisibility();
				}
			}
		};

		Item("Footage", [&]() { return PerformerViewportClient->IsFootageVisible(SideViewMode); }, [&]() { PerformerViewportClient->ToggleFootageVisibility(SideViewMode); });
		Item("SkeletalMesh", [&]() { return PerformerViewportClient->IsRigVisible(SideViewMode); }, [&]() { PerformerViewportClient->ToggleRigVisibility(SideViewMode); });
		Item("DepthMesh", [&]() { return PerformerViewportClient->IsDepthMeshVisible(SideViewMode); }, [&]() { PerformerViewportClient->ToggleDepthMeshVisible(SideViewMode); });
		Item("ControlRig", [&]() { return PerformerViewportClient->IsControlRigVisible(SideViewMode); }, [&]() { PerformerViewportClient->ToggleControlRigVisibility(SideViewMode); });
		Item("Undistort", [&]() { return PerformerViewportClient->IsShowingUndistorted(SideViewMode); }, [&]() { PerformerViewportClient->ToggleDistortion(SideViewMode); });
		Item("Curves", [&]() { return PerformerViewportClient->IsShowingCurves(SideViewMode); }, [&]() { PerformerViewportClient->ToggleShowCurves(SideViewMode); });
		Item("ControlVertices", [&]() { return PerformerViewportClient->IsShowingControlVertices(SideViewMode); }, [&]() { PerformerViewportClient->ToggleShowControlVertices(SideViewMode); });
	}

	TMap<EABImageViewMode, FString> ViewModeStrings{ { EABImageViewMode::A, TEXT("A") },
													 { EABImageViewMode::B, TEXT("B") },
													 { EABImageViewMode::ABSplit, TEXT("ABSplit") },
													 { EABImageViewMode::ABSide, TEXT("ABSide") } };

	if (bInStore)
	{
		ViewSetup->Add(TEXT("ABViewMode"), ViewModeStrings[PerformerViewportClient->GetABViewMode()]);
	}
	else
	{
		FString* Value = ViewSetup->Find(TEXT("ABViewMode"));
		if (Value)
		{
			const EABImageViewMode* ViewMode = ViewModeStrings.FindKey(*Value);
			if (ViewMode)
			{
				PerformerViewportClient->SetABViewMode(*ViewMode);
			}
		}
	}

	TArray<FName> TabNames{ ImageReviewTabId, ControlRigTabId, DetailsTabID, TimelineTabId, ViewportTabID };

	for (const FName& TabName : TabNames)
	{
		TSharedPtr<SDockTab> Tab = TabManager->FindExistingLiveTab(TabName);

		if (bInStore)
		{
			ViewSetup->Add(TabName.ToString(), Tab.IsValid() ? TEXT("true") : TEXT("false"));
		}
		else
		{
			FString* Value = ViewSetup->Find(TabName.ToString());
			if (Value)
			{
				if (*Value == TEXT("true") && !Tab.IsValid())
				{
					TabManager->TryInvokeTab(TabName);
				}
				else if (*Value == TEXT("false") && Tab.IsValid())
				{
					Tab->RequestCloseTab();
				}
			}
		}
	}

	if (bInStore)
	{
		Settings->SaveConfig();
	}
}

FText FMetaHumanPerformanceEditorToolkit::GetStartProcessingShotButtonTooltipText() const
{
	const FMetaHumanPerformanceCommands& Commands = FMetaHumanPerformanceCommands::Get();
	FText Tooltip = Commands.StartProcessingShot->GetDescription();
	if (Performance && !Performance->IsProcessing())
	{
		if (CanProcess())
		{
			return Tooltip;
		}
		else
		{
			return FText::Format(LOCTEXT("StartProcessingShotDisabledButtonTooltip", "{0}\n{1}"), Tooltip, Performance->GetCannotProcessTooltipText());
		}
	}
	else
	{
		return FText::Format(LOCTEXT("StartProcessingShotDisabledAsProcessingButtonTooltip", "{0}\nTo enable this option, first cancel the processing of the current shot"), Tooltip);
	}
}

FText FMetaHumanPerformanceEditorToolkit::GetCancelProcessingShotButtonTooltipText() const
{
	const FMetaHumanPerformanceCommands& Commands = FMetaHumanPerformanceCommands::Get();
	FText Tooltip = Commands.CancelProcessingShot->GetDescription();
	if (Performance && Performance->IsProcessing())
	{
		if (CanCancel())
		{
			return Tooltip;
		}
		else
		{
			return FText::Format(LOCTEXT("CancelProcessingShotDisabledButtonTooltip", "{0}\nThis option is temporarily disabled."), Tooltip);
		}
	}
	else
	{
		return FText::Format(LOCTEXT("CancelProcessingShotDisabledNotProcessingButtonTooltip", "{0}\nThis option is enabled only when shot processing has already started"), Tooltip);
	}
}

void FMetaHumanPerformanceEditorToolkit::HandleShowFramesAsTheyAreProcessed()
{
	if (bShowFramesAsTheyAreProcessed)
	{
		bShowFramesAsTheyAreProcessed = false;
	}
	else
	{
		bShowFramesAsTheyAreProcessed = Performance->bShowFramesAsTheyAreProcessed;
	}
}

void FMetaHumanPerformanceEditorToolkit::SendTelemetryForPerformanceExportRequest(bool InIsAnimationSequence, bool InIsWholeSequence )
{
	/**
	  * @EventName <Editor.MetaHumanPlugin.ExportAnimation>
	  * @Trigger <the user exports an animation from MetaHuman Performance toolkit>
	  * @Type <Client>
	  * @EventParam <SequenceType> <"level","animation">
	  * @EventParam <PerformanceID> <SHA1 hashed GUID of Performance asset formed as PrimaryAssetType/PrimaryAssetName>
	  * @EventParam <ExportType> <"whole","range">
	  * @EventParam <DataInputType> <"Depth Footage", "Speech Audio", "Monocular Footage">
	  * @EventParam <NeutralPoseCalibrationEnabled> <bool>
	  * @Comments <->
	  * @Owner <jon.cook>
	  */

	TArray< FAnalyticsEventAttribute > EventAttributes;

	//Sequence Type (level or animation) - if it's level, InIsWholeSequence will be true
	FString LevelOrAnimation = InIsAnimationSequence ? "animation" : "level";
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SequenceType"), LevelOrAnimation));

	//PerformanceID
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PerformanceID"), Performance->GetHashedPerformanceAssetID()));

	FString WholeSequenceOrRange = InIsWholeSequence ? "whole" : "range";
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ExportType"), WholeSequenceOrRange));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DataInputType"),  UEnum::GetDisplayValueAsText(Performance->InputType).ToString()));

	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("NeutralPoseCalibrationEnabled"), Performance->bNeutralPoseCalibrationEnabled));

	FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.MetaHumanPlugin.ExportAnimation"), EventAttributes);

}

void FMetaHumanPerformanceEditorToolkit::GetExcludedFrameInfo(FFrameRate& OutSourceRate, FFrameRangeMap& OutExcludedFramesMap, int32& OutMediaStartFrame, TRange<FFrameNumber>& OutProcessingLimit) const
{
	const FFrameRate ProcessingFrameRate = Performance->GetFrameRate();
	OutSourceRate = ProcessingFrameRate.IsValid() ? ProcessingFrameRate : TimelineSequencer->GetRootDisplayRate();

	OutExcludedFramesMap.Add(EFrameRangeType::UserExcluded, Performance->UserExcludedFrames);
	OutExcludedFramesMap.Add(EFrameRangeType::ProcessingExcluded, Performance->ProcessingExcludedFrames);
	if (Performance->InputType != EDataInputType::Audio)
	{
		OutExcludedFramesMap.Add(EFrameRangeType::CaptureExcluded, Performance->FootageCaptureData->CaptureExcludedFrames);
	}

	OutMediaStartFrame = Performance->GetMediaStartFrame().Value;

	OutProcessingLimit = Performance->GetProcessingLimitFrameRange();
}

ERichCurveInterpMode FMetaHumanPerformanceEditorToolkit::GetInterpolationMode(int32 InFrameNumber) const
{
	return Performance->GetExcludedFrame(InFrameNumber + 1) == EFrameRangeType::None ? ERichCurveInterpMode::RCIM_Constant : ERichCurveInterpMode::RCIM_Linear;
}

#undef LOCTEXT_NAMESPACE

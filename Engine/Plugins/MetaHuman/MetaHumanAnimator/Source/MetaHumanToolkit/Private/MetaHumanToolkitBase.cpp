// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanToolkitBase.h"

#include "MetaHumanToolkitCommands.h"
#include "MetaHumanToolkitStyle.h"
#include "MetaHumanEditorViewportClient.h"
#include "SMetaHumanEditorViewport.h"
#include "EditorViewportTabContent.h"

#include "MetaHumanSequence.h"
#include "MetaHumanSequencerPlaybackContext.h"
#include "MetaHumanMovieSceneMediaSection.h"
#include "MetaHumanMovieSceneMediaTrack.h"
#include "MetaHumanAudioTrack.h"
#include "MetaHumanAudioSection.h"
#include "ImageSequenceUtils.h"
#include "MetaHumanDepthMeshComponent.h"

#include "ImgMediaSource.h"
#include "ISequencerModule.h"
#include "SequencerSettings.h"
#include "MediaTexture.h"
#include "AdvancedPreviewScene.h"
#include "AdvancedPreviewSceneModule.h"
#include "Fonts/FontMeasure.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "LevelEditor.h"

#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "MediaPlayer.h"
#include "Widgets/Input/SComboBox.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "MetaHumanToolkitBase"

class STimeDisplayCombo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STimeDisplayCombo) :
		_TimelineSequencer()
	{
	}
	SLATE_ATTRIBUTE(TWeakPtr<ISequencer>, TimelineSequencer)
	SLATE_END_ARGS()

	typedef TSharedPtr<FString> FComboItemType;

	void Construct(const FArguments& InArgs)
	{
		if (InArgs._TimelineSequencer.IsSet())
		{
			TimelineSequencer = InArgs._TimelineSequencer.Get();
		}

		Options.Add(MakeShareable(new FString("Frames"))); // Do not change order unless you also change OnSelectionChanged
		Options.Add(MakeShareable(new FString("Seconds")));
		Options.Add(MakeShareable(new FString("Timecode (NDF)")));
		Options.Add(MakeShareable(new FString("Timecode (DF)")));

		const FSlateFontInfo Font(FCoreStyle::GetDefaultFont(), 10, "Regular");

		for (TSharedPtr<FString> Option : Options)
		{
			const float Width = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(*Option.Get(), Font, 1.0).X + 10;
			if (Width > MinWidth)
			{
				MinWidth = Width;
			}
		}

		CurrentItem = Options[0];

		ChildSlot
		[
			SNew(SComboBox<FComboItemType>)
			.OptionsSource(&Options)
			.OnSelectionChanged(this, &STimeDisplayCombo::OnSelectionChanged)
			.OnGenerateWidget(this, &STimeDisplayCombo::MakeWidgetForOption)
			.InitiallySelectedItem(CurrentItem)
			[
				SNew(STextBlock)
				.Text(this, &STimeDisplayCombo::GetCurrentItemLabel)
				.MinDesiredWidth(MinWidth)
			]
		];
	}

	TSharedRef<SWidget> MakeWidgetForOption(FComboItemType InOption)
	{
		return SNew(STextBlock).Text(FText::FromString(*InOption));
	}

	void OnSelectionChanged(FComboItemType InNewValue, ESelectInfo::Type)
	{
		CurrentItem = InNewValue;

		if (TimelineSequencer.IsValid() && CurrentItem.IsValid())
		{
			int32 Index = Options.Find(CurrentItem);

			if (Index == 0)
			{
				TimelineSequencer.Pin()->GetSequencerSettings()->SetTimeDisplayFormat(EFrameNumberDisplayFormats::Frames);
			}
			else if (Index == 1)
			{
				TimelineSequencer.Pin()->GetSequencerSettings()->SetTimeDisplayFormat(EFrameNumberDisplayFormats::Seconds);
			}
			else if (Index == 2)
			{
				TimelineSequencer.Pin()->GetSequencerSettings()->SetTimeDisplayFormat(EFrameNumberDisplayFormats::NonDropFrameTimecode);
			}
			else if (Index == 3)
			{
				TimelineSequencer.Pin()->GetSequencerSettings()->SetTimeDisplayFormat(EFrameNumberDisplayFormats::DropFrameTimecode);
			}
		}
	}

	FText GetCurrentItemLabel() const
	{
		if (CurrentItem.IsValid())
		{
			return FText::FromString(*CurrentItem);
		}

		return LOCTEXT("InvalidComboEntryText", "<<Invalid option>>");
	}

	FComboItemType CurrentItem;
	TArray<FComboItemType> Options;
	TWeakPtr<ISequencer> TimelineSequencer;
	float MinWidth = -1;
};



const FName FMetaHumanToolkitBase::TimelineTabId(TEXT("Timeline"));
const FName FMetaHumanToolkitBase::PreviewSettingsTabId(TEXT("PreviewSettings"));

FMetaHumanToolkitBase::FMetaHumanToolkitBase(UAssetEditor* InOwningAssetEditor)
	: FBaseAssetToolkit{ InOwningAssetEditor }
{
	CreateSequencerTimeline();
	CreatePreviewScene();
}

FMetaHumanToolkitBase::~FMetaHumanToolkitBase()
{
	// Unregister Map Change Events
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditor->OnMapChanged().RemoveAll(this);
	}
}

void FMetaHumanToolkitBase::AddReferencedObjects(FReferenceCollector& InCollector)
{
	if (Sequence)
	{
		InCollector.AddReferencedObject(Sequence);
	}
}

FString FMetaHumanToolkitBase::GetReferencerName() const
{
	return TEXT("FMetaHumanToolkitBase");
}

bool FMetaHumanToolkitBase::IsPrimaryEditor() const
{
	return true;
}

void FMetaHumanToolkitBase::CreateWidgets()
{
	FBaseAssetToolkit::CreateWidgets();

	// Replace the DetailsView widget with a custom one that has a notify hook set to this class
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = this;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
}

void FMetaHumanToolkitBase::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	//the following part is copied from FBaseAssetToolkit::RegisterTabSpawners(InTabManager)
	//because we want to name our viewport differently

	//begin FBaseAssetToolkit::RegisterTabSpawners
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabID, FOnSpawnTab::CreateSP(this, &FMetaHumanToolkitBase::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "A|B Viewport"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FMetaHumanToolkitStyle::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.Tabs.ABViewport")));

	InTabManager->RegisterTabSpawner(DetailsTabID, FOnSpawnTab::CreateSP(this, &FMetaHumanToolkitBase::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("Details", "Details"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.Tabs.Details")));
	//end

	InTabManager->RegisterTabSpawner(TimelineTabId, FOnSpawnTab::CreateSP(this, &FMetaHumanToolkitBase::SpawnTab_Sequencer))
		.SetDisplayName(LOCTEXT("TimelineTab", "Timeline"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FMetaHumanToolkitStyle::Get().GetStyleSetName(), TEXT("MetaHuman Toolkit.Tabs.Timeline")));

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FMetaHumanToolkitBase::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSettingsTab", "Preview Scene Settings"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FMetaHumanToolkitBase::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::UnregisterTabSpawners(InTabManager);
}

void FMetaHumanToolkitBase::PostInitAssetEditor()
{
	BindCommands();

	// Bind to depth data change delegate so we can update the depth view
	GetMetaHumanEditorViewportClient()->OnUpdateFootageDepthDataDelegate.BindSP(this, &FMetaHumanToolkitBase::HandleFootageDepthDataChanged);
	GetMetaHumanEditorViewportClient()->OnUpdateMeshDepthDataDelegate.BindSP(this, &FMetaHumanToolkitBase::HandleMeshDepthDataChanged);
	GetMetaHumanEditorViewportClient()->OnUpdateDepthMapVisibilityDelegate.BindSP(this, &FMetaHumanToolkitBase::HandleDepthMapVisibilityChanged);

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.OnMapChanged().AddSP(this, &FMetaHumanToolkitBase::HandleMapChanged);

	// Force the viewport tab to exist to prevent crashes when using the viewport client
	TabManager->TryInvokeTab(ViewportTabID);
}

void FMetaHumanToolkitBase::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged)
{
	GetMetaHumanEditorViewportClient()->UpdateABVisibility();
}

void FMetaHumanToolkitBase::PostUndo(bool bInSuccess)
{
	if (bInSuccess)
	{
		const FTransaction* Transaction = GEditor->Trans->GetTransaction(GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount());
		HandleUndoOrRedoTransaction(Transaction);
	}
}

void FMetaHumanToolkitBase::PostRedo(bool bInSuccess)
{
	if (bInSuccess)
	{
		const FTransaction* Transaction = GEditor->Trans->GetTransaction(GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1);
		HandleUndoOrRedoTransaction(Transaction);
	}
}

TSharedPtr<FEditorViewportClient> FMetaHumanToolkitBase::CreateEditorViewportClient() const
{
	check(PreviewScene.IsValid());
	return MakeShared<FMetaHumanEditorViewportClient>(PreviewScene.Get());
}

AssetEditorViewportFactoryFunction FMetaHumanToolkitBase::GetViewportDelegate()
{
	AssetEditorViewportFactoryFunction TempViewportDelegate = [this](const FAssetEditorViewportConstructionArgs& InArgs) -> TSharedRef<SAssetEditorViewport>
	{
		return SNew(SMetaHumanEditorViewport, InArgs)
			.ViewportClient(GetMetaHumanEditorViewportClient())
			.ABCommandList(ABCommandList)
			.OnGetABViewMenuContents(this, &FMetaHumanToolkitBase::HandleGetViewABMenuContents)
			[
				GetViewportExtraContentWidget()
			];
	};

	return TempViewportDelegate;
}

TSharedRef<SDockTab> FMetaHumanToolkitBase::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	TSharedRef< SDockTab > DockableTab =
		SNew(SDockTab)
		.Label(LOCTEXT("ABViewportTabTitle", "A|B Viewport"))
		.ToolTipText(LOCTEXT("ABViewportTabTooltip", "AB Viewport\nInspect 2D and 3D components of the scene by switching between Single, Wipe and Dual View Mix Mode.\nIn Single View Mix mode, use A|B button to toggle between A and B view.\nIn Wipe mode, drag the splitting line to adjust the wiper position and orientation, and use the lever gizmo\nto control the transparency of A over B view.\nUse A or B button and/or View Mode buttons in the viewport toolbar corners to adjust the lighting and\nvisualization settings for each view.\nNOTE: Tracking curves can be viewed and edited in Single View Mix mode only."));

	const FString LayoutId = FString("BaseAssetViewport");
	ViewportTabContent->Initialize(ViewportDelegate, DockableTab, LayoutId);
	return DockableTab;
}

TSharedRef<SDockTab> FMetaHumanToolkitBase::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	TSharedPtr<SDockTab> DetailsTab =
		SNew(SDockTab)
		.Label(LOCTEXT("BaseDetailsTabTitle", "Details"))
		.ToolTipText(LOCTEXT("BaseDetailsTabTooltip", "Details\nInspect and edit properties of the selected item"))
		[
			DetailsView.ToSharedRef()
		] ;

	return DetailsTab.ToSharedRef();
}

TSharedRef<SDockTab> FMetaHumanToolkitBase::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>(TEXT("AdvancedPreviewScene"));

	TSharedRef<SWidget> PreviewSceneSettingsWidget = SNullWidget::NullWidget;
	if (PreviewScene.IsValid())
	{
		PreviewSceneSettingsWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(PreviewScene.ToSharedRef());
	}

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		.Label(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		[
			SNew(SBox)
			[
				PreviewSceneSettingsWidget
			]
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMetaHumanToolkitBase::SpawnTab_Sequencer(const FSpawnTabArgs& InArgs)
{
	check(InArgs.GetTabId() == TimelineTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("TimelineTabTitle", "Footage Timeline"))
		.ToolTipText(LOCTEXT("TimelineTabTooltip", "Footage Timeline\n\nDrag the gizmo at the top of the vertical line to review frames in the footage\nand use A|B viewport to see how Components in the MetaHuman Identity Tree View behave in relation to them."))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SVerticalBox)
			.IsEnabled_Lambda([this]() { return IsTimelineEnabled(); })
			+ SVerticalBox::Slot()
			.Padding(0, 2)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				+ SHorizontalBox::Slot()
				.Padding(0, 0, 2, 0)
				.AutoWidth()
				[
					SNew(STimeDisplayCombo)
					.TimelineSequencer(TimelineSequencer)
				]
			]
			+ SVerticalBox::Slot()
			[
				TimelineSequencer->GetSequencerWidget()
			]
		];
}

TRange<int32> FMetaHumanToolkitBase::GetSequencerPlaybackRange() const
{
	TRange<int32> PlaybackRange;

	if (Sequence)
	{
		if (UMovieScene* MovieScene = Sequence->GetMovieScene())
		{
			const FFrameRate TickRate = MovieScene->GetTickResolution();

			// TODO: Using the display rate might not be ideal here and we might need to query the actual image sequence frame rate to do the transformation properly
			const FFrameRate SourceRate = MovieScene->GetDisplayRate();
			TRange<FFrameNumber> RangeAsTime = MovieScene->GetPlaybackRange();

			const FFrameTime TransformedLower = FFrameRate::TransformTime(FFrameTime{ RangeAsTime.GetLowerBoundValue().Value }, TickRate, SourceRate);
			const FFrameTime TransformedUpper = FFrameRate::TransformTime(FFrameTime{ RangeAsTime.GetUpperBoundValue().Value }, TickRate, SourceRate);
			PlaybackRange = TRange<int32>{ TransformedLower.FrameNumber.Value, TransformedUpper.FrameNumber.Value };
		}
	}

	return PlaybackRange;
}

FFrameNumber FMetaHumanToolkitBase::GetCurrentFrameNumber() const
{
	if (UMovieScene* MovieScene = Sequence->GetMovieScene())
	{
		// TODO: Same as above, using display rate might not ideal because the user can change it at any point
		const FFrameRate FrameRate = MovieScene->GetDisplayRate();

		// This will be the current frame number being displayed by sequencer
		const FFrameTime CurrentFrameTime = TimelineSequencer->GetGlobalTime().ConvertTo(FrameRate);

		return CurrentFrameTime.GetFrame();
	}

	return FFrameNumber{};
}

void FMetaHumanToolkitBase::SetMediaTrack(EMediaTrackType InTrackType, TSubclassOf<UMetaHumanMovieSceneMediaTrack> InTrackClass, UImgMediaSource* InImageSequence, FTimecode InTimeCode, FFrameNumber InStartFrameOffset)
{
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	check(MovieScene);

	UMediaTexture* MediaTexture = nullptr;
	UMetaHumanMovieSceneMediaTrack* MediaTrack = nullptr;

	switch (InTrackType)
	{
		case EMediaTrackType::Colour:
		{
			if (ColourMediaTrack == nullptr)
			{
				ColourMediaTrack = CastChecked<UMetaHumanMovieSceneMediaTrack>(Sequence->GetMovieScene()->AddTrack(InTrackClass));
				ColourMediaTrack->ClearFlags(RF_Transactional);
				ColourMediaTrack->SetDisplayName(LOCTEXT("VideoSequenceTrack", "Video"));
			}

			MediaTexture = NewObject<UMediaTexture>(GetTransientPackage());
			MediaTrack = ColourMediaTrack;

			ColourMediaTexture = MediaTexture;
			break;
		}

		case EMediaTrackType::Depth:
		{
			if (DepthMediaTrack == nullptr)
			{
				DepthMediaTrack = CastChecked<UMetaHumanMovieSceneMediaTrack>(Sequence->GetMovieScene()->AddTrack(InTrackClass));
				DepthMediaTrack->ClearFlags(RF_Transactional);
				DepthMediaTrack->SetDisplayName(LOCTEXT("DepthSequenceTrack", "Depth"));
			}

			MediaTexture = NewObject<UMediaTexture>(GetTransientPackage());
			MediaTrack = DepthMediaTrack;

			DepthMediaTexture = MediaTexture;
			break;
		}

		default:
			check(false);
			// TODO: Proper log
			break;
	}

	check(MediaTexture);
	check(MediaTrack);

	// New style output prevents texture from being set as external
	MediaTexture->NewStyleOutput = true;
	MediaTexture->UpdateResource();

	// Add a new Section with the new ImgSequence
	UMetaHumanMovieSceneMediaSection* MediaSection = CastChecked<UMetaHumanMovieSceneMediaSection>(MediaTrack->AddNewMediaSource(*InImageSequence, 0));
	MediaSection->MediaTexture = MediaTexture;
	MediaSection->TimecodeSource = InTimeCode;

	MediaSection->OnKeyAddedEventDelegate().AddSP(this, &FMetaHumanToolkitBase::HandleSequencerKeyAdded);
	MediaSection->OnKeyDeletedEventDelegate().AddSP(this, &FMetaHumanToolkitBase::HandleSequencerKeyRemoved);

	int32 NumFrames = 0;
	FIntVector2 ImageDimensions;
	bool bImageOK = FImageSequenceUtils::GetImageSequenceInfoFromAsset(InImageSequence, ImageDimensions, NumFrames);
	verify(bImageOK);

	// Set the range of the MediaSection based on the number of images in the ImgSequence
	const FFrameRate TickRate = MovieScene->GetTickResolution();
	const FFrameRate SourceRate = InImageSequence->FrameRateOverride.IsValid() ? InImageSequence->FrameRateOverride : MovieScene->GetDisplayRate();

	FFrameTime TransformedStartFrame = FFrameRate::TransformTime(FFrameTime{ 0 }, SourceRate, TickRate);
	FFrameTime TransformedEndFrame = FFrameRate::TransformTime(FFrameTime{ NumFrames }, SourceRate, TickRate);

	TransformedStartFrame += InStartFrameOffset;
	TransformedEndFrame += InStartFrameOffset;

	const TRange<FFrameNumber> PlaybackRange{ TransformedStartFrame.GetFrame(), TransformedEndFrame.GetFrame() };
	MediaSection->SetRange(PlaybackRange);

	RatchetMovieSceneDisplayRate(SourceRate);
}

void FMetaHumanToolkitBase::HandleDepthMapVisibilityChanged(bool bInDepthMapVisibility)
{
	// automatically change whether the depth map track is muted in sequencer according to the visibility of the depthmap
	if (DepthMediaTrack != nullptr && DepthMediaTexture != nullptr)
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		check(MovieScene);
		const TArray<FString> CurMuteNodes = MovieScene->GetMuteNodes();

		bool bCurVisibility = CurMuteNodes.Find(DepthMediaTrack.GetName()) == INDEX_NONE;
		if (bInDepthMapVisibility != bCurVisibility)
		{
			MovieScene->Modify();
			TArray<FString>& MuteNodes = MovieScene->GetMuteNodes();
			if (bInDepthMapVisibility)
			{
				MuteNodes.Remove(DepthMediaTrack.GetName());

				// this is a HACK to ensure that the image media cache for the depth map is updated when we turn the depth map back on
				// otherwise it will not be updated if we are currently outside the cache
				TimelineSequencer->SetLocalTime(TimelineSequencer->GetLastEvaluatedLocalTime().RoundToFrame());
			}
			else
			{
				MuteNodes.AddUnique(DepthMediaTrack.GetName());
			}

			TimelineSequencer->RefreshTree();
		}
	}
}

void FMetaHumanToolkitBase::SetMediaTrack(TSubclassOf<class UMovieSceneAudioTrack> InTrackClass, class USoundBase* InAudio, FTimecode InTimecode, FFrameNumber InStartFrameOffset)
{
	if (InAudio != nullptr)
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		check(MovieScene);

		if (AudioMediaTrack == nullptr)
		{
			UMovieSceneTrack* Track = Sequence->GetMovieScene()->AddTrack(InTrackClass);
			AudioMediaTrack = CastChecked<UMovieSceneAudioTrack>(Track);
			AudioMediaTrack->SetDisplayName(LOCTEXT("AudioSequenceTrack", "Audio"));
		}

		UMovieSceneAudioSection* AudioSection = CastChecked<UMovieSceneAudioSection>(AudioMediaTrack->AddNewSound(InAudio, InStartFrameOffset));
		AudioSection->TimecodeSource = InTimecode;
		AudioSection->Modify();

		// Audio tracks currently don't have a proper display rate associated with them, so we default to 30 fps
		const FFrameRate AssumedAudioDisplayRate(30'000, 1'000);
		RatchetMovieSceneDisplayRate(AssumedAudioDisplayRate);
	}
}

void FMetaHumanToolkitBase::ClearMediaTracks()
{
	for (UMetaHumanMovieSceneMediaTrack* MediaTrack : { ColourMediaTrack, DepthMediaTrack })
	{
		if (MediaTrack != nullptr)
		{
			for (UMovieSceneSection* Section : MediaTrack->GetAllSections())
			{
				if (UMetaHumanMovieSceneMediaSection* MetaHumanSection = Cast<UMetaHumanMovieSceneMediaSection>(Section))
				{
					MetaHumanSection->OnKeyAddedEventDelegate().RemoveAll(this);
					MetaHumanSection->OnKeyDeletedEventDelegate().RemoveAll(this);
				}
			}
		}
	}

	// Remove all tracks from the movie scene
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	TArray<UMovieSceneTrack*> MasterTracks = MovieScene->GetTracks();
	for (UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		MovieScene->RemoveTrack(*MasterTrack);
	}

	ColourMediaTrack = nullptr;
	ColourMediaTexture = nullptr;
	DepthMediaTrack = nullptr;
	DepthMediaTexture = nullptr;

	if (AudioMediaTrack != nullptr)
	{
		Sequence->GetMovieScene()->RemoveTrack(*AudioMediaTrack);
		AudioMediaTrack = nullptr;
	}

	ResetMovieSceneDisplayRate();
}

bool FMetaHumanToolkitBase::ChannelContainsKey(UMetaHumanMovieSceneMediaTrack* InMediaTrack, const FFrameNumber& InFrameTime) const
{
	if (InMediaTrack != nullptr)
	{
		TArray<FFrameNumber> OurKeyTimes;
		TArray<FKeyHandle> OurKeyHandles;
		TRange<FFrameNumber> CurrentFrameRange;

		CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>(InFrameTime));
		CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>(InFrameTime));

		UMovieSceneSection* Section = InMediaTrack->GetAllSections().Last();
		TArrayView<FMetaHumanMovieSceneChannel*> MediaTrackChannel = Section->GetChannelProxy().GetChannels<FMetaHumanMovieSceneChannel>();
		TMovieSceneChannelData<bool> ChannelData = MediaTrackChannel.Last()->GetData();
		ChannelData.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);

		return !OurKeyTimes.IsEmpty();
	}

	return false;
}

void FMetaHumanToolkitBase::HandleSequencerGlobalTimeChanged()
{
	if (ViewportClient)
	{
		GetMetaHumanEditorViewportClient()->UpdateSceneCaptureComponents();
	}
}

TSharedRef<SWidget> FMetaHumanToolkitBase::GetViewportExtraContentWidget()
{
	return SNullWidget::NullWidget;
}

void FMetaHumanToolkitBase::CreateDepthMeshComponent(UCameraCalibration* InCameraCalibration)
{
	DestroyDepthMeshComponent();

	if (InCameraCalibration != nullptr)
	{
		checkf(PreviewActor, TEXT("Preview Actor should have been created by CreatePreviewScene"));

		DepthMeshComponent = NewObject<UMetaHumanDepthMeshComponent>(PreviewActor);
		PreviewActor->AddInstanceComponent(DepthMeshComponent);
		DepthMeshComponent->AttachToComponent(PreviewActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DepthMeshComponent->RegisterComponent();

		DepthMeshComponent->SetCameraCalibration(InCameraCalibration);

		DepthMeshComponent->GetMaterial(0)->GetMaterial()->OnMaterialCompilationFinished().AddSP(this, &FMetaHumanToolkitBase::HandleDepthMeshMaterialCompiled);

		GetMetaHumanEditorViewportClient()->SetDepthMeshComponent(DepthMeshComponent);
	}
}

void FMetaHumanToolkitBase::HandleDepthMeshMaterialCompiled(UMaterialInterface*)
{
	GetMetaHumanEditorViewportClient()->UpdateSceneCaptureComponents();
}

void FMetaHumanToolkitBase::HandleMapChanged(UWorld* InNewWorld, EMapChangeType InMapChangeType)
{
	if ((InMapChangeType == EMapChangeType::LoadMap || InMapChangeType == EMapChangeType::NewMap || InMapChangeType == EMapChangeType::TearDownWorld))
	{
		TimelineSequencer->GetSpawnRegister().CleanUp(*TimelineSequencer);
		CloseWindow(EAssetEditorCloseReason::EditorRefreshRequested);
	}
}

void FMetaHumanToolkitBase::SetDepthMeshTexture(UTexture* InDepthTexture)
{
	if (DepthMeshComponent != nullptr)
	{
		DepthMeshComponent->SetDepthTexture(InDepthTexture);
	}
}

void FMetaHumanToolkitBase::DestroyDepthMeshComponent()
{
	if (DepthMeshComponent != nullptr)
	{
		DepthMeshComponent->DestroyComponent();
		DepthMeshComponent = nullptr;
	}
}

void FMetaHumanToolkitBase::CreatePreviewScene()
{
	constexpr float InitialFloorOffset = 250.0f;
	PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues(), InitialFloorOffset);
	check(PreviewScene);

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;
	PreviewActor = PreviewScene->GetWorld()->SpawnActor<AActor>(SpawnInfo);
	check(PreviewActor);

	// Create a root scene component for the preview actor
	// Automatic attachment means this will be the new root component
	const bool bManualAttachment = false;
	const bool bDeferredFinish = false;
	UActorComponent* RootComponent = PreviewActor->AddComponentByClass(USceneComponent::StaticClass(), bManualAttachment, FTransform{}, bDeferredFinish);
	check(RootComponent);
}

void FMetaHumanToolkitBase::CreateSequencerTimeline()
{
	Sequence = NewObject<UMetaHumanSceneSequence>(GetTransientPackage());
	Sequence->MovieScene = NewObject<UMovieScene>(Sequence, NAME_None, RF_Transactional);

	// Setting the tick rate to 24000 to accommodate for audio/video timecode difference of 10+ hours
	Sequence->MovieScene->SetTickResolutionDirectly(FFrameRate(24000, 1));
	ResetMovieSceneDisplayRate();

	PlaybackContext = MakeShared<FMetaHumanSequencerPlaybackContext>();

	FSequencerInitParams SequencerInitParams;
	SequencerInitParams.ViewParams.ScrubberStyle = ESequencerScrubberStyle::FrameBlock;
	SequencerInitParams.ViewParams.bShowPlaybackRangeInTimeSlider = true;

	SequencerInitParams.RootSequence = Sequence;
	SequencerInitParams.bEditWithinLevelEditor = false;
	SequencerInitParams.ToolkitHost = nullptr;
	SequencerInitParams.PlaybackContext.Bind(PlaybackContext.ToSharedRef(), &FMetaHumanSequencerPlaybackContext::GetPlaybackContext);

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
	TimelineSequencer = SequencerModule.CreateSequencer(SequencerInitParams);

	// Set default settings for the sequencer editor
	USequencerSettings* SequencerSettings = TimelineSequencer->GetSequencerSettings();
	SequencerSettings->SetTimeDisplayFormat(EFrameNumberDisplayFormats::Frames);
	SequencerSettings->SetKeepPlayRangeInSectionBounds(false);
	SequencerSettings->SetIsSnapEnabled(true);
	SequencerSettings->SetAutoScrollEnabled(true);
	SequencerSettings->SetShowRangeSlider(true);
	SequencerSettings->SetShowInfoButton(false);

	SequencerSettings->SetShowTickLines(false);
	SequencerSettings->SetShowSequencerToolbar(false);

	TimelineSequencer->OnMovieSceneDataChanged().AddRaw(this, &FMetaHumanToolkitBase::HandleSequencerMovieSceneDataChanged);
	TimelineSequencer->OnGlobalTimeChanged().AddRaw(this, &FMetaHumanToolkitBase::HandleSequencerGlobalTimeChanged);
}

TSharedRef<FMetaHumanEditorViewportClient> FMetaHumanToolkitBase::GetMetaHumanEditorViewportClient() const
{
	return StaticCastSharedPtr<FMetaHumanEditorViewportClient>(ViewportClient).ToSharedRef();
}

void FMetaHumanToolkitBase::RatchetMovieSceneDisplayRate(const FFrameRate InFrameRate)
{
	if (IsValid(Sequence))
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();

		if (IsValid(MovieScene))
		{
			const FFrameRate CurrentDisplayRate = MovieScene->GetDisplayRate();

			if (InFrameRate.AsDecimal() > CurrentDisplayRate.AsDecimal())
			{
				MovieScene->SetDisplayRate(InFrameRate);
			}
		}
	}
}

void FMetaHumanToolkitBase::ResetMovieSceneDisplayRate()
{
	if (IsValid(Sequence))
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();

		if (IsValid(MovieScene))
		{
			const FFrameRate InitialDisplayRate(1'000, 1'000);
			MovieScene->SetDisplayRate(InitialDisplayRate);
		}
	}
}

#undef LOCTEXT_NAMESPACE
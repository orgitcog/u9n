// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubtitlesTrackEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Input/Reply.h"
#include "LevelSequence.h"
#include "MovieSceneSubtitlesTrack.h"
#include "MVVM/Views/ViewUtilities.h"
#include "SequencerSettings.h"
#include "Subtitles/SubtitlesAndClosedCaptionsDelegates.h"
#include "SubtitleSequencerSection.h"


#define LOCTEXT_NAMESPACE "FSubtitlesTrackEditor"

FSubtitlesTrackEditor::FSubtitlesTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

TSharedRef<ISequencerTrackEditor> FSubtitlesTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FSubtitlesTrackEditor(OwningSequencer));
}

FText FSubtitlesTrackEditor::GetDisplayName() const
{
	return LOCTEXT("SubtitilesTrackEditor_DisplayName", "Subtitles");
}

void FSubtitlesTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddTrack", "Subtitles Track"),
		LOCTEXT("AddTooltip", "Adds a new subtitles track that can display subtitles and closed captions."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Audio"), // #SUBTITLES_PRD_TODO - Add a subtitles track icon
		FUIAction(
			FExecuteAction::CreateRaw(this, &FSubtitlesTrackEditor::HandleAddMenuEntryExecute)
		)
	);
}

void FSubtitlesTrackEditor::HandleAddMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddSubtitlesTrack_Transaction", "Add Subtitles Track"));
	FocusedMovieScene->Modify();

	UMovieSceneSubtitlesTrack* NewTrack = FocusedMovieScene->AddTrack<UMovieSceneSubtitlesTrack>();
	check(NewTrack);

	NewTrack->SetDisplayName(LOCTEXT("SubtitlesTrackName", "Subtitles"));

	if (GetSequencer().IsValid())
	{
		GetSequencer()->OnAddTrack(NewTrack, FGuid());
	}
}

TSharedPtr<SWidget> FSubtitlesTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	TDelegate OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FSubtitlesTrackEditor::OnAssetSelected, Track);
	TDelegate OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw(this, &FSubtitlesTrackEditor::OnAssetEnterPressed, Track);
	return UE::Sequencer::MakeAddButton(
		LOCTEXT("SubtitleText", "Subtitle")
		, FOnGetContent::CreateSP(this, &FSubtitlesTrackEditor::BuildSubMenu, MoveTemp(OnAssetSelected), MoveTemp(OnAssetEnterPressed))
		, Params.ViewModel);
}

TSharedRef<SWidget> FSubtitlesTrackEditor::BuildSubMenu(FOnAssetSelected OnAssetSelected, FOnAssetEnterPressed OnAssetEnterPressed)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	UMovieSceneSequence* Sequence = SequencerPtr.IsValid() ? SequencerPtr->GetFocusedMovieSceneSequence() : nullptr;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FTopLevelAssetPath> ClassNames;
	ClassNames.Add(USubtitleAssetUserData::StaticClass()->GetClassPathName());
	TSet<FTopLevelAssetPath> DerivedClassNames;
	AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FTopLevelAssetPath>(), DerivedClassNames);

	FMenuBuilder MenuBuilder(true, nullptr);

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = OnAssetSelected;
		AssetPickerConfig.OnAssetEnterPressed = OnAssetEnterPressed;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bAddFilterUI = true;
		AssetPickerConfig.bShowTypeInColumnView = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		for (FTopLevelAssetPath ClassName : DerivedClassNames)
		{
			AssetPickerConfig.Filter.ClassPaths.Add(ClassName);
		}
		AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");
		AssetPickerConfig.AdditionalReferencingAssets.Add(FAssetData(Sequence));
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	const float WidthOverride = SequencerPtr.IsValid() ? SequencerPtr->GetSequencerSettings()->GetAssetBrowserWidth() : 500.f;
	const float HeightOverride = SequencerPtr.IsValid() ? SequencerPtr->GetSequencerSettings()->GetAssetBrowserHeight() : 400.f;

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(WidthOverride)
		.HeightOverride(HeightOverride)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);

	return MenuBuilder.MakeWidget();
}

void FSubtitlesTrackEditor::OnAssetSelected(const FAssetData& AssetData, UMovieSceneTrack* Track)
{
	FSlateApplication::Get().DismissAllMenus();

	const UObject* SelectedObject = AssetData.GetAsset();
	if (!SelectedObject)
	{
		return;
	}

	const USubtitleAssetUserData* NewAsset = CastChecked<const USubtitleAssetUserData>(AssetData.GetAsset());
	if (!NewAsset)
	{
		return;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddSubtitle_Transaction", "Add Subtitle"));

	UMovieSceneSubtitlesTrack* SubtitlesTrack = Cast<UMovieSceneSubtitlesTrack>(Track);
	check(SubtitlesTrack);
	SubtitlesTrack->Modify();

	if (TSharedPtr<ISequencer> SequencerPin = GetSequencer())
	{
		UMovieSceneSection* NewSection = SubtitlesTrack->AddNewSubtitle(*NewAsset, SequencerPin->GetLocalTime().Time.FrameNumber);

		SequencerPin->EmptySelection();
		SequencerPin->SelectSection(NewSection);
		SequencerPin->ThrobSectionSelection();

		SequencerPin->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

void FSubtitlesTrackEditor::OnAssetEnterPressed(const TArray<FAssetData>& AssetData, UMovieSceneTrack* Track)
{
	if (!AssetData.IsEmpty())
	{
		OnAssetSelected(AssetData[0].GetAsset(), Track);
	}
}

TSharedRef<ISequencerSection> FSubtitlesTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShareable(new FSubtitleSequencerSection(SectionObject));
}

bool FSubtitlesTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneSubtitlesTrack::StaticClass();
}

bool FSubtitlesTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	ETrackSupport TrackSupported = ETrackSupport::NotSupported;
	if (InSequence && InSequence->IsA(ULevelSequence::StaticClass()))
	{
		TrackSupported = ETrackSupport::Supported;
	}
	return TrackSupported == ETrackSupport::Supported;
}

bool FSubtitlesTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams)
{
	return false;
}

FReply FSubtitlesTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams)
{
	return FReply::Unhandled();
}
#undef LOCTEXT_NAMESPACE

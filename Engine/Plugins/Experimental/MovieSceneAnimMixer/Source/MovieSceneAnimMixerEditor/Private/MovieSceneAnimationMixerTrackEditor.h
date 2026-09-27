// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "MovieSceneTrackEditor.h"
#include "TrackEditors/CommonAnimationTrackEditor.h"

namespace UE::Sequencer
{


class FAnimationMixerTrackEditor
	: public FCommonAnimationTrackEditor
{
public:

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	FAnimationMixerTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	virtual FText GetDisplayName() const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual TSharedPtr<SWidget> BuildOutlinerColumnWidget(const FBuildColumnWidgetParams& Params, const FName& ColumnName) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual TSubclassOf<UMovieSceneCommonAnimationTrack> GetTrackClass() const override;

private:
	using FCommonAnimationTrackEditor::BuildAddAnimationSubMenu;

	TSharedRef<SWidget> BuildAddSectionSubMenu(TWeakViewModelPtr<IOutlinerExtension> WeakViewModel, TWeakViewModelPtr<FSequencerEditorViewModel> WeakEditor);
	void HandleAddAnimationTrackMenuEntryExecute(TArray<FGuid> ObjectBindings);
	void PopulateAddAnimationMenu(FMenuBuilder& MenuBuilder, TWeakViewModelPtr<IOutlinerExtension> WeakViewModel, TWeakViewModelPtr<FSequencerEditorViewModel> WeakEditor);

	bool FilterAnimSequences(const FAssetData& AssetData, USkeleton* Skeleton);
	void OnAnimationAssetSelected(const FAssetData& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);
	void OnAnimationAssetEnterPressed(const TArray<FAssetData>& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);
};


} // namespace UE::Sequencer
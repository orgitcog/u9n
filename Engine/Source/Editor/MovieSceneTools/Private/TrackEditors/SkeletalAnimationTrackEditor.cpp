// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/SkeletalAnimationTrackEditor.h"

#include "AnimSequencerInstanceProxy.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"

#define LOCTEXT_NAMESPACE "FSkeletalAnimationTrackEditor"


namespace UE::Sequencer::SkeletalAnimationEditorConstants
{
	static TAutoConsoleVariable<bool> CVarEvaluateSkeletalMeshOnPropertyChange(
		TEXT("Sequencer.EvaluateSkeletalMeshOnPropertyChange"),
		true,
		TEXT("Enable/disable sending a track value changed when properties change so that the skeletal mesh can be re-evaluated in Sequencer"));
} // namespace UE::Sequencer::SkeletalAnimationEditorConstants


FSkeletalAnimationTrackEditor::FSkeletalAnimationTrackEditor( TSharedRef<ISequencer> InSequencer )
	: UE::Sequencer::FCommonAnimationTrackEditor(InSequencer)
{
}

TSharedRef<ISequencerTrackEditor> FSkeletalAnimationTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShared<FSkeletalAnimationTrackEditor>(InSequencer);
}

void FSkeletalAnimationTrackEditor::OnInitialize()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FSkeletalAnimationTrackEditor::OnPostPropertyChanged);
	FCommonAnimationTrackEditor::OnInitialize();
}

void FSkeletalAnimationTrackEditor::OnRelease()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FCommonAnimationTrackEditor::OnRelease();
}

bool FSkeletalAnimationTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	ETrackSupport TrackSupported = InSequence ? InSequence->IsTrackSupported(UMovieSceneSkeletalAnimationTrack::StaticClass()) : ETrackSupport::NotSupported;
	return TrackSupported == ETrackSupport::Supported;
}

bool FSkeletalAnimationTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneSkeletalAnimationTrack::StaticClass();
}

void FSkeletalAnimationTrackEditor::OnPostPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (UE::Sequencer::SkeletalAnimationEditorConstants::CVarEvaluateSkeletalMeshOnPropertyChange->GetBool())
	{
		const FName PropertyName = (InPropertyChangedEvent.Property != nullptr) ? InPropertyChangedEvent.Property->GetFName() : NAME_None;
		if (PropertyName == USceneComponent::GetRelativeLocationPropertyName() ||
			PropertyName == USceneComponent::GetRelativeRotationPropertyName() ||
			PropertyName == USceneComponent::GetRelativeScale3DPropertyName())
		{
			return;
		}

		// If the object changed has any animation track, notify sequencer to update because animations tick on their own and sequencer needs to evaluate again
		const bool bCreateIfMissing = false;
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(InObject, bCreateIfMissing);
		FGuid ObjectHandle = HandleResult.Handle;
		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneSkeletalAnimationTrack::StaticClass(), NAME_None, bCreateIfMissing);
			if (TrackResult.Track)
			{
				GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
			}
		}
	}
}

TSubclassOf<UMovieSceneCommonAnimationTrack> FSkeletalAnimationTrackEditor::GetTrackClass() const
{
	return UMovieSceneSkeletalAnimationTrack::StaticClass();
}

void FSkeletalAnimationTrackEditor::BuildTrackContextMenu_Internal(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track, const bool bAddSeparatorAtEnd)
{
	//there's a bug with a section being open already, so we end it.
	UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack = Cast<UMovieSceneSkeletalAnimationTrack>(Track);

	/** Put this back when and if it works
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("MotionBlendingOptions", "Motion Blending Options"));
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "AutoMatchClipsRootMotions", "Auto Match Clips Root Motions"),
			NSLOCTEXT("Sequencer", "AutoMatchClipsRootMotionsTooltip", "Preceeding clips will auto match to the preceding clips root bones position. You can override this behavior per clip in it's section options."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]()->void {
					SkeletalAnimationTrack->ToggleAutoMatchClipsRootMotions(); 
					SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);

					}),
				FCanExecuteAction::CreateLambda([=]()->bool { return SequencerPtr && SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([=]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->bAutoMatchClipsRootMotions; })),
			NAME_None, 
			EUserInterfaceActionType::ToggleButton
		);
		MenuBuilder.EndSection();
	}
	*/

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("SkelAnimRootMOtion", "Root Motion"));
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "BlendFirstChildOfRoot", "Blend First Child Of Root"),
			NSLOCTEXT("Sequencer", "BlendFirstChildOfRootTooltip", "If True, do not blend and match the root bones but instead the first child bone of the root. Toggle this on when the matched sequences in the track have no motion on the root."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, SkeletalAnimationTrack]() -> void
				{
					SkeletalAnimationTrack->bBlendFirstChildOfRoot = SkeletalAnimationTrack->bBlendFirstChildOfRoot ? false : true;
					SkeletalAnimationTrack->SetRootMotionsDirty();

					if (const TSharedPtr<ISequencer> SequencerPtr = GetSequencer())
					{
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
					}
				}),
				FCanExecuteAction::CreateLambda([this, SkeletalAnimationTrack]() -> bool
				{
					return SkeletalAnimationTrack != nullptr;
				}),
				FIsActionChecked::CreateLambda([this, SkeletalAnimationTrack]() -> bool
				{
					return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->bBlendFirstChildOfRoot;
				})),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "ShowRootMotionTrails", "Show Root Motion Trail"),
			NSLOCTEXT("Sequencer", "ShowRootMotionTrailsTooltip", "Show the Root Motion Trail for all Animation Clips."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, SkeletalAnimationTrack]() -> void
				{
					SkeletalAnimationTrack->ToggleShowRootMotionTrail();

					if (const TSharedPtr<ISequencer> SequencerPtr = GetSequencer())
					{
						SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
					}
				}),
				FCanExecuteAction::CreateLambda([this, SkeletalAnimationTrack]()->bool
				{
					if (const TSharedPtr<ISequencer> SequencerPtr = GetSequencer())
					{
						return SkeletalAnimationTrack != nullptr;
					}
					return false;
				}),
				FIsActionChecked::CreateLambda([this, SkeletalAnimationTrack]()->bool
				{
					return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->bShowRootMotionTrail;
				})),
			NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "SwapRootBoneNone", "Swap Root Bone None"),
			NSLOCTEXT("Sequencer", "SwapRootBoneNoneTooltip", "Do not swap root bone for all sections."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->SetSwapRootBone(ESwapRootBone::SwapRootBone_None);
					}),
				FCanExecuteAction::CreateLambda([SkeletalAnimationTrack]()->bool { return  SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->SwapRootBone == ESwapRootBone::SwapRootBone_None; })),
				NAME_None,
				EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "SwapRootBoneActor", "Swap Root Bone Actor"),
			NSLOCTEXT("Sequencer", "SwapRootBoneActorTooltip", "Swap root bone on root actor component for all sections."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->SetSwapRootBone(ESwapRootBone::SwapRootBone_Actor);
					}),
				FCanExecuteAction::CreateLambda([SkeletalAnimationTrack]()->bool { return  SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->SwapRootBone == ESwapRootBone::SwapRootBone_Actor; })),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "SwapRootBoneComponent", "Swap Root Bone Component"),
			NSLOCTEXT("Sequencer", "SwapRootBoneComponentTooltip", "Swap root bone on current component for all sections."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([SkeletalAnimationTrack]()->void {
					SkeletalAnimationTrack->SetSwapRootBone(ESwapRootBone::SwapRootBone_Component);
					}),
				FCanExecuteAction::CreateLambda([SkeletalAnimationTrack]()->bool { return  SkeletalAnimationTrack != nullptr; }),
				FIsActionChecked::CreateLambda([SkeletalAnimationTrack]()->bool { return SkeletalAnimationTrack != nullptr && SkeletalAnimationTrack->SwapRootBone == ESwapRootBone::SwapRootBone_Component; })),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
	}
	MenuBuilder.EndSection();

	if (bAddSeparatorAtEnd)
	{
		MenuBuilder.AddSeparator();
	}
}

TSharedRef<ISequencerSection> FSkeletalAnimationTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	return MakeShared<FSkeletalAnimationSection>(SectionObject, GetSequencer());
}

FSkeletalAnimationSection::FSkeletalAnimationSection( UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: UE::Sequencer::FCommonAnimationSection( InSection, InSequencer)
{
}

void FSkeletalAnimationTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	BuildTrackContextMenu_Internal(MenuBuilder, Track, true);
}

void FSkeletalAnimationTrackEditor::BuildTrackSidebarMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	BuildTrackContextMenu_Internal(MenuBuilder, Track, false);
}


#undef LOCTEXT_NAMESPACE

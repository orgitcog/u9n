// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneAnimationMixerTrackEditor.h"

#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ClassIconFinder.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "ISequencer.h"
#include "MovieSceneAnimationMixerTrack.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/ViewModels/OutlinerColumns/OutlinerColumnTypes.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/Views/SOutlinerItemViewBase.h"
#include "MVVM/Views/ViewUtilities.h"
#include "ScopedTransaction.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "SequencerUtilities.h"

#define LOCTEXT_NAMESPACE "MovieSceneAnimationMixerTrackEditor"


namespace UE::Sequencer
{

class FAnimMixerAnimationSection
	: public FCommonAnimationSection
{
public:

	FAnimMixerAnimationSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
		: FCommonAnimationSection(InSection, InSequencer)
	{}

	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override
	{
		FCommonAnimationSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

		UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(WeakSection.Get());
		if (AnimSection)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("RootTransform_Label", "Root Transform"),
				LOCTEXT("RootTransform_Tooltip", "Options for root transform behavior from this anim clip"),
				FNewMenuDelegate::CreateSP(this, &FAnimMixerAnimationSection::PopulateRootTransformMenu)
			);
		}
	}

	UMovieSceneAnimationBaseTransformDecoration* FindRootDecoration() const
	{
		UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(WeakSection.Get());
		return AnimSection ? AnimSection->FindDecoration<UMovieSceneAnimationBaseTransformDecoration>() : nullptr;
	}

	UMovieSceneAnimationBaseTransformDecoration* FindOrCreateRootDecoration() const
	{
		UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(WeakSection.Get());
		if (AnimSection)
		{
			UMovieSceneAnimationBaseTransformDecoration* Decoration = AnimSection->FindDecoration<UMovieSceneAnimationBaseTransformDecoration>();
			if (!Decoration)
			{
				AnimSection->Modify();
				Decoration = AnimSection->GetOrCreateDecoration<UMovieSceneAnimationBaseTransformDecoration>();
			}
			return Decoration;
		}
		return nullptr;
	}

	void PopulateRootTransformMenu(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("RootTransformModeSection_Label", "Root Transform Mode"));
		{
			FCanExecuteAction AlwaysExecute = FCanExecuteAction::CreateLambda([] { return true; });

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RootTransformMode_None", "From Animation"),
				LOCTEXT("RootTransformMode_None_Tip", "Use the root motion transform directly from the animation asset"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAnimMixerAnimationSection::SetRootTransformNone),
					AlwaysExecute,
					FIsActionChecked::CreateSP(this, &FAnimMixerAnimationSection::IsTransformMode, EMovieSceneRootMotionTransformMode::Asset)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RootTransformMode_Offset", "Offset From Animation"),
				LOCTEXT("RootTransformMode_Offset_Toolitp", "Offset the root motion transform using keyframed values"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAnimMixerAnimationSection::SetRootTransformOffset),
					AlwaysExecute,
					FIsActionChecked::CreateSP(this, &FAnimMixerAnimationSection::IsTransformMode, EMovieSceneRootMotionTransformMode::Offset)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RootTransformMode_Override", "Manual Override"),
				LOCTEXT("RootTransformMode_Override_Toolitp", "Completely override the root motion transform using keyframed values"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAnimMixerAnimationSection::SetRootTransformOverride),
					AlwaysExecute,
					FIsActionChecked::CreateSP(this, &FAnimMixerAnimationSection::IsTransformMode, EMovieSceneRootMotionTransformMode::Override)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}
		MenuBuilder.EndSection();

		UMovieSceneAnimationBaseTransformDecoration* Decoration = FindRootDecoration();
		if (Decoration && Decoration->GetRootTransformMode() == EMovieSceneRootMotionTransformMode::Offset)
		{
			MenuBuilder.BeginSection(NAME_None, LOCTEXT("RootTransformOffset_Label", "Offset"));
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("RecenterRootTransform", "Re-center Root Transform"),
					LOCTEXT("RecenterRootTransform_Tooltip", "Center the root transform for this animation around its current position"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAnimMixerAnimationSection::RecenterRootTransform)
					)
				);
			}
		}
	}

	bool IsTransformMode(EMovieSceneRootMotionTransformMode Mode) const
	{
		if (UMovieSceneAnimationBaseTransformDecoration* Decoration = FindRootDecoration())
		{
			return Decoration->GetRootTransformMode() == Mode;
		}
		return Mode == EMovieSceneRootMotionTransformMode::Asset;
	}

	void SetRootTransformNone()
	{
		FScopedTransaction Transaction(LOCTEXT("SetRootTransformNone", "Use Root Transform from Asset"));

		UMovieSceneSkeletalAnimationSection*         AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(WeakSection.Get());
		UMovieSceneAnimationBaseTransformDecoration* Decoration  = FindOrCreateRootDecoration();
		if (Decoration && AnimSection)
		{
			Decoration->Modify();
			Decoration->TransformMode.SetDefault((uint8)EMovieSceneRootMotionTransformMode::Asset);
			AnimSection->InvalidateChannelProxy();
		}
	}
	void SetRootTransformOffset()
	{
		FScopedTransaction Transaction(LOCTEXT("SetRootTransformOffset", "Offset Root Transform from Asset"));

		UMovieSceneSkeletalAnimationSection*         AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(WeakSection.Get());
		UMovieSceneAnimationBaseTransformDecoration* Decoration  = FindOrCreateRootDecoration();
		if (Decoration && AnimSection)
		{
			Decoration->Modify();
			Decoration->TransformMode.SetDefault((uint8)EMovieSceneRootMotionTransformMode::Offset);
			AnimSection->InvalidateChannelProxy();
		}
	}
	void SetRootTransformOverride()
	{
		FScopedTransaction Transaction(LOCTEXT("SetRootTransformOverride", "Override Root Transform"));

		UMovieSceneSkeletalAnimationSection*         AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(WeakSection.Get());
		UMovieSceneAnimationBaseTransformDecoration* Decoration  = FindOrCreateRootDecoration();
		if (Decoration && AnimSection)
		{
			Decoration->Modify();
			Decoration->TransformMode.SetDefault((uint8)EMovieSceneRootMotionTransformMode::Override);
			AnimSection->InvalidateChannelProxy();
		}
	}
	void RecenterRootTransform()
	{
		FScopedTransaction Transaction(LOCTEXT("RecenterRootTransform", "Re-center Root Transform"));

		TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();

		UMovieSceneSkeletalAnimationSection*         AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(WeakSection.Get());
		UMovieSceneAnimationBaseTransformDecoration* Decoration  = FindRootDecoration();
		UAnimSequenceBase*                           Animation   = AnimSection ? AnimSection->GetAnimation() : nullptr;

		if (SequencerPtr && Decoration && Animation)
		{
			Decoration->Modify();

			FQualifiedFrameTime Time = SequencerPtr->GetLocalTime();
			double AnimTime = AnimSection->MapTimeToAnimation(Time.Time, Time.Rate);

			FAnimExtractContext CurrentExtractContext(AnimTime);
			FTransform CurrentTransform  = Animation->ExtractRootTrackTransform(CurrentExtractContext, nullptr);

			Decoration->RootOriginLocation = CurrentTransform.GetTranslation();
		}
	}
};

TSharedRef<ISequencerTrackEditor> FAnimationMixerTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FAnimationMixerTrackEditor(InSequencer));
}

FAnimationMixerTrackEditor::FAnimationMixerTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FCommonAnimationTrackEditor(InSequencer)
{
}

FText FAnimationMixerTrackEditor::GetDisplayName() const
{
	return LOCTEXT("AnimationMixerTrackEditor_DisplayName", "Animation Mixer");
}

void FAnimationMixerTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass != nullptr && (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass())))
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddAnimationTrack", "Animation Mixer"),
			LOCTEXT("AddAnimationTrackTooltip", "Adds a new animation track for playing back Anim Sequences and other sources of animation."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Sequencer.Tracks.Animation"),
			FUIAction( 
				FExecuteAction::CreateSP(this, &FAnimationMixerTrackEditor::HandleAddAnimationTrackMenuEntryExecute, ObjectBindings)
			)
		);
	}
}

TSharedPtr<SWidget> FAnimationMixerTrackEditor::BuildOutlinerColumnWidget(const FBuildColumnWidgetParams& Params, const FName& ColumnName)
{
	using namespace UE::Sequencer;

	TViewModelPtr<FSequencerEditorViewModel> Editor       = Params.Editor->CastThisShared<FSequencerEditorViewModel>();
	TViewModelPtr<IOutlinerExtension>        OutlinerItem = Params.ViewModel.ImplicitCast();
	if (!Editor || !OutlinerItem)
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == FCommonOutlinerNames::Add)
	{
		return MakeAddButton(
			LOCTEXT("AddSection", "Section"),
			FOnGetContent::CreateSP(
				this,
				&FAnimationMixerTrackEditor::BuildAddSectionSubMenu,
				TWeakViewModelPtr<IOutlinerExtension>(OutlinerItem),
				TWeakViewModelPtr<FSequencerEditorViewModel>(Editor)
			),
			Params.ViewModel);
	}

	return FMovieSceneTrackEditor::BuildOutlinerColumnWidget(Params, ColumnName);
}

bool FAnimationMixerTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return true;
}

bool FAnimationMixerTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneAnimationMixerTrack::StaticClass());
}

const FSlateBrush* FAnimationMixerTrackEditor::GetIconBrush() const
{
	return FAppStyle::GetBrush("Sequencer.Tracks.Animation");
}

TSubclassOf<UMovieSceneCommonAnimationTrack> FAnimationMixerTrackEditor::GetTrackClass() const
{
	return UMovieSceneAnimationMixerTrack::StaticClass();
}

TSharedRef<ISequencerSection> FAnimationMixerTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	if (SectionObject.IsA<UMovieSceneSkeletalAnimationSection>())
	{
		return MakeShared<FAnimMixerAnimationSection>(SectionObject, GetSequencer());
	}

	return FMovieSceneTrackEditor::MakeSectionInterface(SectionObject, Track, ObjectBinding);
}

TSharedRef<SWidget> FAnimationMixerTrackEditor::BuildAddSectionSubMenu(TWeakViewModelPtr<IOutlinerExtension> WeakViewModel, TWeakViewModelPtr<FSequencerEditorViewModel> WeakEditor)
{
	TViewModelPtr<ITrackExtension> Track = WeakViewModel.ImplicitPin();
	if (!Track)
	{
		return SNullWidget::NullWidget;
	}

	auto CreateNewSection = [WeakViewModel, WeakEditor](FTopLevelAssetPath ClassPath)
	{
		UClass* SectionClass = FSoftClassPath(ClassPath.ToString()).TryLoadClass<UMovieSceneSection>();
		if (!SectionClass)
		{
			return;
		}

		check(SectionClass && SectionClass->IsChildOf(UMovieSceneSection::StaticClass()) && SectionClass->ImplementsInterface(IMovieSceneAnimationSectionInterface::UClassType::StaticClass()));

		TViewModelPtr<ITrackExtension>           Track     = WeakViewModel.ImplicitPin();
		TViewModelPtr<FSequencerEditorViewModel> Editor    = WeakEditor.Pin();
		TSharedPtr<ISequencer>                   Sequencer = Editor ? Editor->GetSequencer() : nullptr;

		UMovieSceneTrack* TrackObject = Track ? Track->GetTrack() : nullptr;
		if (Sequencer && TrackObject)
		{
			FScopedTransaction Transaction(FText::Format(LOCTEXT("AddSectionTransaction", "Add New {0} Section"), SectionClass->GetDisplayNameText()));

			TrackObject->Modify();

			UMovieSceneSection* NewSection = NewObject<UMovieSceneSection>(TrackObject, SectionClass, NAME_None, RF_Transactional);

			int32 MaxRowIndex = -1;
			for (UMovieSceneSection* Section : TrackObject->GetAllSections())
			{
				if (Section->GetClass() == SectionClass)
				{
					if (!Section->GetRange().Overlaps(NewSection->GetRange()))
					{
						if (MaxRowIndex > Section->GetRowIndex() || MaxRowIndex == -1)
						{
							MaxRowIndex = Section->GetRowIndex();
						}
					}
					else if (MaxRowIndex == -1 || MaxRowIndex > Section->GetRowIndex()+1)
					{
						MaxRowIndex = Section->GetRowIndex() + 1;
					}
				}
			}

			NewSection->SetRowIndex(MaxRowIndex);

			TrackObject->AddSection(*NewSection);

			TrackObject->FixRowIndices();
			TrackObject->UpdateEasing();

			Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		}
	};

	TViewModelSubListIterator<FSectionModel> Sections = Track->GetSectionModels().IterateSubList<FSectionModel>();

	FMenuBuilder MenuBuilder(true, nullptr);

	if (Sections && Cast<UMovieSceneSkeletalAnimationSection>(Sections->GetSection()))
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("TimeWarp_Label", "Time Warp"));
		{
			FSequencerUtilities::MakeTimeWarpMenuEntry(MenuBuilder, Track);
		}
		MenuBuilder.EndSection();
	}

	// Find all classes that match our interface
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TSet<FTopLevelAssetPath> AllClasses;
	{
		FTopLevelAssetPath TargetClassPath(UMovieSceneSection::StaticClass());

		AssetRegistryModule.Get().GetDerivedClassNames({ TargetClassPath }, TSet<FTopLevelAssetPath>(), AllClasses);
	}

	for (auto It = AllClasses.CreateIterator(); It; ++It)
	{
		const UClass* Class = FSoftClassPath(It->ToString()).TryLoadClass<UMovieSceneSection>();

		if (!Class || Class->HasMetaData("Hidden") || !Class->ImplementsInterface(IMovieSceneAnimationSectionInterface::UClassType::StaticClass()))
		{
			It.RemoveCurrent();
		}
	}

	if (AllClasses.Num() != 0)
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("AnimationCategoryLabel", "Animation:"));

		for (const FTopLevelAssetPath& ClassPath : AllClasses)
		{
			const UClass* Class = FSoftClassPath(ClassPath.ToString()).ResolveClass();
			// Should have been loaded by the validation loop above
			check(Class);

			MenuBuilder.AddMenuEntry(
				Class->GetDisplayNameText(),
				Class->GetToolTipText(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda(CreateNewSection, ClassPath))
			);
		}

		FViewModelPtr Model = WeakViewModel.Pin();
		TViewModelPtr<IObjectBindingExtension> ObjectBinding = Model ? Model->FindAncestorOfType<IObjectBindingExtension>() : nullptr;
		if (Track && ObjectBinding)
		{
			USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBinding->GetObjectGuid(), GetSequencer());
			if (Skeleton)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("AddAnimationSubMenu", "Animation"),
					LOCTEXT("AddAnimationSubMenu_Tooltip", "Adds a new animation section for an animation asset at the current time"),
					FNewMenuDelegate::CreateSP(this, &FAnimationMixerTrackEditor::AddAnimationSubMenu, TArray<FGuid>({ ObjectBinding->GetObjectGuid() }), Skeleton, Track->GetTrack())
				);
			}
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void FAnimationMixerTrackEditor::HandleAddAnimationTrackMenuEntryExecute(TArray<FGuid> ObjectBindings)
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	if (MovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddAnimationTrack_Transaction", "Add Animation Track"));

	MovieScene->Modify();

	for (const FGuid& Guid : ObjectBindings)
	{
		FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(Guid, UMovieSceneAnimationMixerTrack::StaticClass());
		if (TrackResult.bWasCreated)
		{
			UMovieSceneAnimationMixerTrack* AnimTrack = CastChecked<UMovieSceneAnimationMixerTrack>(TrackResult.Track);
			AnimTrack->SetDisplayName(AnimTrack->GetDefaultDisplayName());
		}
	}
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE



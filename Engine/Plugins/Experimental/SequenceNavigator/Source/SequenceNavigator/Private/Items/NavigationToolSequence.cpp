// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/NavigationToolSequence.h"
#include "GameFramework/Actor.h"
#include "INavigationTool.h"
#include "Items/NavigationToolActor.h"
#include "Items/NavigationToolItemUtils.h"
#include "Items/NavigationToolMarker.h"
#include "Items/NavigationToolSubTrack.h"
#include "Items/NavigationToolTrack.h"
#include "LevelSequenceActor.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieScene.h"
#include "MVVM/SectionModelStorageExtension.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "NavigationToolScopedSelection.h"
#include "ScopedTransaction.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "SequencerChannelInterface.h"
#include "SequencerSettings.h"
#include "Styling/SlateIconFinder.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Utils/NavigationToolMiscUtils.h"
#include "Utils/NavigationToolMovieSceneUtils.h"

#define LOCTEXT_NAMESPACE "NavigationToolSequence"

using namespace UE::Sequencer;

namespace UE::SequenceNavigator
{

FNavigationToolSequence::FNavigationToolSequence(INavigationTool& InTool
	, const FNavigationToolItemPtr& InParentItem
	, UMovieSceneSequence* const InSequence
	, UMovieSceneSubSection* const InSubSection
	, const int32 InSubSectionIndex)
	: Super(InTool, InParentItem)
	, IRevisionControlExtension(InSequence)
	, WeakSubSection(InSubSection)
	, SubSectionIndex(InSubSectionIndex)
	, WeakSequence(InSequence)
{
}

bool FNavigationToolSequence::IsItemValid() const
{
	return WeakSequence.IsValid();
}

UObject* FNavigationToolSequence::GetItemObject() const
{
	return GetSequence();
}

bool FNavigationToolSequence::IsAllowedInTool() const
{
	return WeakSequence.IsValid();
}

void FNavigationToolSequence::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive)
{
	Super::FindChildren(OutChildren, bInRecursive);

	UMovieSceneSequence* const Sequence = GetSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	const TSharedRef<FNavigationToolSequence> SharedThisRef = SharedThis(this);
	const TSharedPtr<FNavigationToolProvider> Provider = GetProvider();
	const TArray<UMovieSceneTrack*> MovieSceneTracks = MovieScene->GetTracks();

	for (UMovieSceneTrack* const Track : MovieSceneTracks)
	{
		FNavigationToolItemPtr NewItem;

		if (UMovieSceneSubTrack* const SubTrack = Cast<UMovieSceneSubTrack>(Track))
		{
			NewItem = Tool.FindOrAdd<FNavigationToolSubTrack>(GetProvider()
				, SharedThisRef, SubTrack, Sequence, WeakSubSection, SubSectionIndex);
		}
		else if (Track)
		{
			NewItem = Tool.FindOrAdd<FNavigationToolTrack>(GetProvider()
				, SharedThisRef, Track, Sequence, WeakSubSection, SubSectionIndex);
		}

		if (NewItem.IsValid())
		{
			OutChildren.Add(NewItem);
			if (bInRecursive)
			{
				NewItem->FindChildren(OutChildren, bInRecursive);
			}
		}
	}

	// Only show actor bindings for a Sequence
	const TArray<FMovieSceneBinding> Bindings = GetSortedBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		const FGuid& BindingObjectGuid = Binding.GetObjectGuid();
		const UClass* const BoundObjectClass = MovieSceneHelpers::GetBoundObjectClass(Sequence, BindingObjectGuid);
		if (BoundObjectClass && BoundObjectClass->IsChildOf<AActor>())
		{
			const FNavigationToolItemPtr NewItem = Tool.FindOrAdd<FNavigationToolActor>(GetProvider()
				, SharedThisRef, SharedThisRef, Binding);
			OutChildren.Add(NewItem);
			if (bInRecursive)
			{
				NewItem->FindChildren(OutChildren, bInRecursive);
			}
		}
	}

	// Add sequence marker items
	const TArray<FMovieSceneMarkedFrame>& MarkedFrames = MovieScene->GetMarkedFrames();
	for (int32 Index = 0; Index < MarkedFrames.Num(); ++Index)
	{
		const FNavigationToolItemPtr NewItem = Tool.FindOrAdd<FNavigationToolMarker>(GetProvider()
			, SharedThisRef, SharedThisRef, Index);
		OutChildren.Add(NewItem);
		if (bInRecursive)
		{
			NewItem->FindChildren(OutChildren, bInRecursive);
		}
	}
}

void FNavigationToolSequence::GetItemProxies(TArray<TSharedPtr<FNavigationToolItemProxy>>& OutItemProxies)
{
	Super::GetItemProxies(OutItemProxies);
}

bool FNavigationToolSequence::AddChild(const FNavigationToolAddItemParams& InAddItemParams)
{
	// @TODO: handle sequence being moved to this item
	return Super::AddChild(InAddItemParams);
}

bool FNavigationToolSequence::RemoveChild(const FNavigationToolRemoveItemParams& InRemoveItemParams)
{
	/// @TODO: handle sequence being moved to this item
	return Super::RemoveChild(InRemoveItemParams);
}

ENavigationToolItemViewMode FNavigationToolSequence::GetSupportedViewModes(const INavigationToolView& InToolView) const
{
	// Sequences should only be visualized in Navigation Tool View and not appear in the Item Column List
	// Support any other type of View Mode
	return ENavigationToolItemViewMode::ItemTree | ~ENavigationToolItemViewMode::HorizontalItemList;
}

FText FNavigationToolSequence::GetDisplayName() const
{
	FText NewDisplayName = FText::GetEmpty();

	if (const UMovieSceneCinematicShotSection* const ShotSection = Cast<UMovieSceneCinematicShotSection>(WeakSubSection.Get()))
	{
		NewDisplayName = FText::FromString(ShotSection->GetShotDisplayName());
	}
	else
	{
		if (const UMovieSceneSequence* const Sequence = GetSequence())
		{
			NewDisplayName = Sequence->GetDisplayName();
		}
	}

	if (NewDisplayName.IsEmpty())
	{
		return NewDisplayName;
	}

	// Apply additional options to display name
	const TSharedRef<FNavigationToolSequence> ThisSequenceRef = SharedThis(const_cast<FNavigationToolSequence*>(this));
	const UNavigationToolSettings* const ToolSettings = GetDefault<UNavigationToolSettings>();

	if (ToolSettings->ShouldUseShortNames())
	{
		RemoveSequenceDisplayNameParentPrefix(NewDisplayName, ThisSequenceRef);
	}

	if (UMovieSceneSequence* const Sequence = ThisSequenceRef->GetSequence())
	{
		AppendSequenceDisplayNameDirtyStatus(NewDisplayName, *Sequence);
	}

	return NewDisplayName;
}

FText FNavigationToolSequence::GetClassName() const
{
	if (const UMovieSceneSequence* const Sequence = GetSequence())
	{
		return FText::FromString(Sequence->GetClass()->GetName());
	}
	return FText::FromString(UMovieSceneSequence::StaticClass()->GetName());
}

FSlateIcon FNavigationToolSequence::GetIcon() const
{
	return FSlateIconFinder::FindIconForClass(ALevelSequenceActor::StaticClass());
}

FText FNavigationToolSequence::GetIconTooltipText() const
{
	return ALevelSequenceActor::StaticClass()->GetDisplayNameText();
}

bool FNavigationToolSequence::IsSelected(const FNavigationToolScopedSelection& InSelection) const
{
	return WeakSubSection.IsValid() && InSelection.IsSelected(WeakSubSection.Get());
}

void FNavigationToolSequence::Select(FNavigationToolScopedSelection& InSelection) const
{
	if (WeakSubSection.IsValid())
	{
		InSelection.Select(WeakSubSection.Get());
	}
}

void FNavigationToolSequence::OnSelect()
{
	Super::OnSelect();
}

void FNavigationToolSequence::OnDoubleClick()
{
	UMovieSceneSequence* const Sequence = GetSequence();
	if (!Sequence)
	{
		return;
	}

	const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();

	if (ModifierKeys.IsAltDown())
	{
		// Instead of focusing the sequence in the Sequencer, focus the parent sequence and select sequence
		if (const FNavigationToolSequence* const ParentSequenceItem = FindAncestorOfType<FNavigationToolSequence>())
		{
			if (UMovieSceneSequence* const ParentSequence = ParentSequenceItem->GetSequence())
			{
				FocusSequence(Tool, *ParentSequence, *this);
			}
		}
	}

	FocusSequence(Tool, *Sequence);
}

void FNavigationToolSequence::OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bInRecursive)
{
	// Get the Object even if it's Pending Kill (most likely it is)
	const UMovieSceneSequence* const ObjectPendingKill = WeakSequence.Get(true);
	if (ObjectPendingKill && InReplacementMap.Contains(ObjectPendingKill))
	{
		WeakSequence = Cast<UMovieSceneSequence>(InReplacementMap[ObjectPendingKill]);
	}

	// This handles calling OnObjectsReplaced for every child item
	Super::OnObjectsReplaced(InReplacementMap, bInRecursive);
}

FNavigationToolItemId FNavigationToolSequence::CalculateItemId() const
{
	return FNavigationToolItemId(GetParent(), GetSequence(), WeakSubSection.Get(), SubSectionIndex);
}

bool FNavigationToolSequence::CanRename() const
{
	// Disable for now
	return false;
	/*const UMovieSceneSequence* const Sequence = GetSequence();
	if (!Sequence)
	{
		return false;
	}

	const UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return false;
	}

	return !MovieScene->IsReadOnly();*/
}

bool FNavigationToolSequence::Rename(const FString& InName)
{
	UMovieSceneSequence* const Sequence = GetSequence();
	if (!Sequence)
	{
		return false;
	}

	bool bRenamed = false;

	if (!InName.Equals(Sequence->GetName(), ESearchCase::CaseSensitive))
	{
		//const FScopedTransaction Transaction(LOCTEXT("SequenceRename", "Rename Sequence"));

		if (UMovieSceneCinematicShotSection* const ShotSection = Cast<UMovieSceneCinematicShotSection>(WeakSubSection.Get()))
		{
			ShotSection->Modify();
			ShotSection->SetShotDisplayName(InName);

			Tool.NotifyToolItemRenamed(SharedThis(this));

			bRenamed = true;
		}
		else
		{
			Sequence->Modify();

			if (Sequence->Rename(*InName))
			{
				Tool.NotifyToolItemRenamed(SharedThis(this));

				bRenamed = true;
			}
		}
	}

	return bRenamed;
}

EItemSequenceInactiveState FNavigationToolSequence::GetInactiveState() const
{
	if (WeakSubSection.IsValid())
	{
		return WeakSubSection->IsActive() ? EItemSequenceInactiveState::None : EItemSequenceInactiveState::Inactive;
	}

	const ENavigationToolCompareState State = CompareChildrenItemState<ISequenceInactivableExtension>(this,
		[](const ISequenceInactivableExtension* const InItem)
			{
				return InItem->GetInactiveState() == EItemSequenceInactiveState::Inactive;
			},
		[](const ISequenceInactivableExtension* const InItem)
			{
				return InItem->GetInactiveState() == EItemSequenceInactiveState::None;
			});

	return static_cast<EItemSequenceInactiveState>(State);
}

void FNavigationToolSequence::SetIsInactive(const bool bInIsInactive)
{
	const bool bNewActiveState = !bInIsInactive;

	if (WeakSubSection.IsValid())
	{
		if (!WeakSubSection->IsReadOnly() && WeakSubSection->IsActive() != bNewActiveState)
		{
			WeakSubSection->Modify();
			WeakSubSection->SetIsActive(bNewActiveState);
		}
	}

	for (ISequenceInactivableExtension* const InactivableItem : GetChildrenOfType<ISequenceInactivableExtension>())
	{
		InactivableItem->SetIsInactive(bInIsInactive);
	}
}

EItemMarkerVisibility FNavigationToolSequence::GetMarkerVisibility() const
{
	return IsGloballyMarkedFramesForSequence(GetSequence())
		? EItemMarkerVisibility::Visible : EItemMarkerVisibility::None;
}

void FNavigationToolSequence::SetMarkerVisibility(const bool bInVisible)
{
	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	const bool bVisible = (GetMarkerVisibility() == EItemMarkerVisibility::Visible);
	if (bVisible == bInVisible)
	{
		return;
	}

	UMovieSceneSequence* const Sequence = GetSequence();
	if (Sequence == Sequencer->GetRootMovieSceneSequence())
	{
		Sequencer->GetSequencerSettings()->SetShowMarkedFrames(bInVisible);
	}

	ShowGloballyMarkedFramesForSequence(*Sequencer, Sequence, bInVisible);

	for (IMarkerVisibilityExtension* const MarkerVisibilityItem : GetChildrenOfType<IMarkerVisibilityExtension>())
	{
		MarkerVisibilityItem->SetMarkerVisibility(bInVisible);
	}
}

EItemSequenceLockState FNavigationToolSequence::GetLockState() const
{
	if (WeakSubSection.IsValid())
	{
		return WeakSubSection->IsLocked() ? EItemSequenceLockState::Locked : EItemSequenceLockState::None;
	}

	const ENavigationToolCompareState State = CompareChildrenItemState<ISequenceLockableExtension>(this,
		[](const ISequenceLockableExtension* const InItem)
			{
				return InItem->GetLockState() == EItemSequenceLockState::Locked;
			},
		[](const ISequenceLockableExtension* const InItem)
			{
				return InItem->GetLockState() != EItemSequenceLockState::Locked;
			});

	return static_cast<EItemSequenceLockState>(State);
}

void FNavigationToolSequence::SetIsLocked(const bool bInIsLocked)
{
	if (WeakSubSection.IsValid())
	{
		if (WeakSubSection->IsLocked() != bInIsLocked)
		{
			WeakSubSection->Modify();
			WeakSubSection->SetIsLocked(bInIsLocked);
		}

		return;
	}

	for (ISequenceLockableExtension* const LockableItem : GetChildrenOfType<ISequenceLockableExtension>())
	{
		LockableItem->SetIsLocked(bInIsLocked);
	}
}

TOptional<FColor> FNavigationToolSequence::GetColor() const
{
	if (!WeakSubSection.IsValid())
	{
		return FColor::Transparent;
	}

	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return FColor::Transparent;
	}

	const FColor TrackColor = WeakSubSection->GetColorTint();
	return (TrackColor == FColor()) ? TOptional<FColor>() : WeakSubSection->GetColorTint();
}

void FNavigationToolSequence::SetColor(const TOptional<FColor>& InColor)
{
	if (!WeakSubSection.IsValid())
	{
		return;
	}

	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	WeakSubSection->Modify();
	WeakSubSection->SetColorTint(InColor.Get(FColor()));
}

EItemRevisionControlState FNavigationToolSequence::GetRevisionControlState() const
{
	UMovieSceneSequence* const Sequence = GetSequence();
	if (!Sequence)
	{
		return EItemRevisionControlState::None;
	}

	const FSourceControlStatePtr RevisionControlState = FindSourceControlState(Sequence->GetPackage());
	if (!RevisionControlState.IsValid())
	{
		return EItemRevisionControlState::None;
	}

	return EItemRevisionControlState::SourceControlled;
}

const FSlateBrush* FNavigationToolSequence::GetRevisionControlStatusIcon() const
{
	if (const UMovieSceneSequence* const Sequence = GetSequence())
	{
		return FindSourceControlStatusBrush(Sequence->GetPackage());
	}
	return nullptr;
}

FText FNavigationToolSequence::GetRevisionControlStatusText() const
{
	if (const UMovieSceneSequence* const Sequence = GetSequence())
	{
		return FindSourceControlStatusText(Sequence->GetPackage());
	}
	return FText::GetEmpty();
}

EItemContainsPlayhead FNavigationToolSequence::ContainsPlayhead() const
{
	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return EItemContainsPlayhead::None;
	}

	const UMovieSceneSequence* const Sequence = WeakSequence.Get();
	if (!IsValid(Sequence))
	{
		return EItemContainsPlayhead::None;
	}

	UMovieSceneSequence* const FocusedSequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!IsValid(FocusedSequence))
	{
		return EItemContainsPlayhead::None;
	}

	const FQualifiedFrameTime PlayheadTime = Sequencer->GetLocalTime();

	if (Sequence == FocusedSequence)
	{
		UMovieScene* const SequenceMovieScene = Sequence->GetMovieScene();
		if (IsValid(SequenceMovieScene))
		{
			return SequenceMovieScene->GetPlaybackRange().Contains(PlayheadTime.Time.FrameNumber)
				? EItemContainsPlayhead::ContainsPlayhead : EItemContainsPlayhead::None;
		}
	}
	else
	{
		UMovieSceneSubSection* const SubSection = GetSubSection();
		if (IsValid(SubSection))
		{
			return SubSection->GetTrueRange().Contains(PlayheadTime.Time.FrameNumber)
				? EItemContainsPlayhead::ContainsPlayhead : EItemContainsPlayhead::None;
		}
	}

	return EItemContainsPlayhead::None;
}

FFrameNumber FNavigationToolSequence::GetInTime() const
{
	const UMovieSceneSequence* const Sequence = WeakSequence.Get();
	if (!Sequence)
	{
		return 0;
	}

	const UMovieSceneSubSection* const SubSection = WeakSubSection.Get();
	if (!SubSection)
	{
		return 0;
	}

	const FFrameTime LowerBoundValue = SubSection->SectionRange.Value.GetLowerBoundValue();
	return LowerBoundValue.FrameNumber;
}

void FNavigationToolSequence::SetInTime(const FFrameNumber& InTime)
{
	const UMovieSceneSequence* const Sequence = WeakSequence.Get();
	if (!Sequence)
	{
		return;
	}

	UMovieSceneSubSection* const SubSection = WeakSubSection.Get();
	if (!SubSection)
	{
		return;
	}

	if (SubSection->SectionRange.Value.GetLowerBoundValue() == InTime)
	{
		return;
	}

	SubSection->Modify();
	SubSection->SectionRange.Value.SetLowerBoundValue(InTime);
}

FFrameNumber FNavigationToolSequence::GetOutTime() const
{
	const UMovieSceneSequence* const Sequence = WeakSequence.Get();
	if (!Sequence)
	{
		return 0;
	}

	const UMovieSceneSubSection* const SubSection = WeakSubSection.Get();
	if (!SubSection)
	{
		return 0;
	}

	const FFrameTime UpperBoundValue = SubSection->SectionRange.Value.GetUpperBoundValue();
	return UpperBoundValue.FrameNumber;
}

void FNavigationToolSequence::SetOutTime(const FFrameNumber& InTime)
{
	const UMovieSceneSequence* const Sequence = WeakSequence.Get();
	if (!Sequence)
	{
		return;
	}

	UMovieSceneSubSection* const SubSection = WeakSubSection.Get();
	if (!SubSection)
	{
		return;
	}

	if (SubSection->SectionRange.Value.GetUpperBoundValue() == InTime)
	{
		return;
	}

	SubSection->Modify();
	SubSection->SectionRange.Value.SetUpperBoundValue(InTime);
}

UMovieSceneSequence* FNavigationToolSequence::GetSequence() const
{
	return WeakSequence.Get();
}

UMovieSceneSubSection* FNavigationToolSequence::GetSubSection() const
{
	return WeakSubSection.IsValid() ? WeakSubSection.Get() : nullptr;
}

int32 FNavigationToolSequence::GetSubSectionIndex() const
{
	return SubSectionIndex;
}

UMovieScene* FNavigationToolSequence::GetSequenceMovieScene() const
{
	if (const UMovieSceneSequence* const Sequence = GetSequence())
	{
		return Sequence->GetMovieScene();
	}
	return nullptr;
}

TArray<FMovieSceneBinding> FNavigationToolSequence::GetSortedBindings() const
{
	UMovieScene* const MovieScene = GetSequenceMovieScene();
	if (!MovieScene)
	{
		return {};
	}

	TArray<FMovieSceneBinding> Bindings = MovieScene->GetBindings();

	Bindings.Sort([](const FMovieSceneBinding& InA, const FMovieSceneBinding& InB)
		{
			const int32 SortingOrderA = InA.GetSortingOrder();
			const int32 SortingOrderB = InB.GetSortingOrder();
			if (SortingOrderA == SortingOrderB)
			{
				return InA.GetName() < InB.GetName();
			}
			return SortingOrderA < SortingOrderB;
		});

	return MoveTemp(Bindings);
}

TViewModelPtr<FSectionModel> FNavigationToolSequence::GetViewModel() const
{
	if (!WeakSubSection.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<FSequencerEditorViewModel> ViewModel = Sequencer->GetViewModel();
	if (!ViewModel.IsValid())
	{
		return nullptr;
	}

	const FViewModelPtr RootViewModel = ViewModel->GetRootModel();
	if (!RootViewModel.IsValid())
	{
		return nullptr;
	}

	const FSectionModelStorageExtension* const StorageExtension = RootViewModel->CastDynamic<FSectionModelStorageExtension>();
	if (!StorageExtension)
	{
		return nullptr;
	}

	return StorageExtension->FindModelForSection(WeakSubSection.Get());
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationToolAvaSequence.h"
#include "AvaEditorCoreStyle.h"
#include "AvaSequence.h"
#include "AvaSequencerUtils.h"
#include "IAvaSequencer.h"
#include "INavigationTool.h"
#include "ISequencer.h"
#include "Items/NavigationToolItemParameters.h"
#include "Items/NavigationToolSequence.h"
#include "ScopedTransaction.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "NavigationToolAvaSequence"

namespace UE::SequenceNavigator
{

using namespace UE::Sequencer;

FNavigationToolAvaSequence::FNavigationToolAvaSequence(INavigationTool& InTool
	, const FNavigationToolItemPtr& InParentItem
	, UAvaSequence* const InAvaSequence)
	: FNavigationToolSequence(InTool
	, InParentItem
	, InAvaSequence
	, nullptr
	, 0)
{
}

bool FNavigationToolAvaSequence::AddChild(const FNavigationToolAddItemParams& InAddItemParams)
{
	if (!CanAddChild(InAddItemParams.Item))
	{
		return false;
	}

	const FNavigationToolAvaSequence* const AvaSequenceItemToAdd = InAddItemParams.Item->CastTo<FNavigationToolAvaSequence>();
	if (!AvaSequenceItemToAdd)
	{
		return false;
	}

	AddChildChecked(InAddItemParams);

	UAvaSequence* const ParentAvaSequence = GetAvaSequence();
	UAvaSequence* const AvaSequenceToAdd = AvaSequenceItemToAdd->GetAvaSequence();

	const TArray<TWeakObjectPtr<UAvaSequence>> TargetChildren = ParentAvaSequence->GetChildren();
	if (TargetChildren.Contains(AvaSequenceToAdd))
	{
		return false;
	}

	// Remove the sequence from any parent it's a child of
	if (UAvaSequence* const AvaSequenceToMoveParent = AvaSequenceToAdd->GetParent())
	{
		AvaSequenceToMoveParent->RemoveChild(AvaSequenceToAdd);
	}

	// Process moving sequence if it's parent is the target
	if (ParentAvaSequence == AvaSequenceToAdd->GetParent())
	{
		ParentAvaSequence->RemoveChild(AvaSequenceToAdd);

		if (UAvaSequence* const TargetParent = ParentAvaSequence->GetParent())
		{
			TargetParent->AddChild(AvaSequenceToAdd);
		}

		return true;
	}

	// Check if the Target Item is a Child of Sequence Item (to prevent cycle)
	UAvaSequence* CurrentItem = ParentAvaSequence;
	while (CurrentItem)
	{
		if (CurrentItem->GetParent() == AvaSequenceToAdd)
		{
			AvaSequenceToAdd->RemoveChild(CurrentItem);
			CurrentItem->SetParent(AvaSequenceToAdd->GetParent());

			break;
		}

		CurrentItem = CurrentItem->GetParent();
	}

	ParentAvaSequence->AddChild(AvaSequenceToAdd);

	return true;
}

bool FNavigationToolAvaSequence::RemoveChild(const FNavigationToolRemoveItemParams& InRemoveItemParams)
{
	// Check that we're removing a child item that is directly under us, and not a grandchild
	if (!InRemoveItemParams.Item.IsValid() || !Children.Contains(InRemoveItemParams.Item))
	{
		return false;
	}

	if (const FNavigationToolAvaSequence* const AvaSequenceItemToRemove = InRemoveItemParams.Item->CastTo<FNavigationToolAvaSequence>())
	{
		if (UAvaSequence* const AvaSequenceToRemove = AvaSequenceItemToRemove->GetAvaSequence())
		{
			// Remove the sequence from any parent it's a child of
			if (UAvaSequence* const AvaSequenceToRemoveParent = AvaSequenceToRemove->GetParent())
			{
				AvaSequenceToRemoveParent->RemoveChild(AvaSequenceToRemove);
			}
		}
	}

	return RemoveChildChecked(InRemoveItemParams.Item);
}

void FNavigationToolAvaSequence::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, bool bRecursive)
{
	FNavigationToolSequence::FindChildren(OutChildren, bRecursive);

	UAvaSequence* const AvaSequence = GetAvaSequence();
	if (!AvaSequence)
	{
		return;
	}

	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	const TSharedPtr<FNavigationToolProvider> Provider = GetProvider();
	if (!Provider.IsValid())
	{
		return;
	}

	TArray<TWeakObjectPtr<UAvaSequence>> ChildSequences = AvaSequence->GetChildren();
	ChildSequences.RemoveAll([](const TWeakObjectPtr<UAvaSequence>& InChild)
		{
			return !InChild.IsValid();
		});
	ChildSequences.Sort([](const TWeakObjectPtr<UAvaSequence>& InA, const TWeakObjectPtr<UAvaSequence>& InB)
		{
			return InA->GetDisplayName().CompareTo(InB->GetDisplayName()) > 0;
		});

	const TSharedRef<FNavigationToolAvaSequence> SharedThisRef = SharedThis(this);

	for (const TWeakObjectPtr<UAvaSequence>& SequenceWeak : ChildSequences)
	{
		const FNavigationToolItemPtr NewItem = Tool.FindOrAdd<FNavigationToolAvaSequence>(Provider
			, SharedThisRef, SequenceWeak.Get());
		OutChildren.Add(NewItem);
		if (bRecursive)
		{
			NewItem->FindChildren(OutChildren, bRecursive);
		}
	}
}

void FNavigationToolAvaSequence::GetItemProxies(TArray<TSharedPtr<FNavigationToolItemProxy>>& OutItemProxies)
{
	FNavigationToolSequence::GetItemProxies(OutItemProxies);
}

bool FNavigationToolAvaSequence::CanRename() const
{
	return GetAvaSequence() != nullptr;
}

bool FNavigationToolAvaSequence::Rename(const FString& InName)
{
	UAvaSequence* const UnderlyingSequence = GetAvaSequence();
	if (!UnderlyingSequence)
	{
		return false;
	}

	bool bRenamed = false;

	if (!InName.Equals(UnderlyingSequence->GetLabel().ToString(), ESearchCase::CaseSensitive))
	{
		const FScopedTransaction Transaction(LOCTEXT("AvaSequenceRename", "Rename Sequence"));

		UnderlyingSequence->Modify();
		UnderlyingSequence->SetLabel(FName(InName));

		Tool.NotifyToolItemRenamed(SharedThis(this));

		bRenamed = true;
	}

	return bRenamed;
}

bool FNavigationToolAvaSequence::CanDelete() const
{
	return GetAvaSequence() != nullptr;
}

bool FNavigationToolAvaSequence::Delete()
{
	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return false;
	}

	UAvaSequence* const AvaSequence = GetAvaSequence();
	if (!AvaSequence)
	{
		return false;
	}

	const TSharedPtr<IAvaSequencer> AvaSequencer = FAvaSequencerUtils::GetAvaSequencer(Sequencer.ToSharedRef());
	if (!AvaSequencer.IsValid())
	{
		return false;
	}

	AvaSequencer->DeleteSequences({ AvaSequence });

	return true;
}

FText FNavigationToolAvaSequence::GetDisplayName() const
{
	if (const UAvaSequence* const AvaSequence = GetAvaSequence())
	{
		return AvaSequence->GetDisplayName();
	}
	return FText::GetEmpty();
}

FSlateIcon FNavigationToolAvaSequence::GetIcon() const
{
	return FSlateIcon(FAvaEditorCoreStyle::Get().GetStyleSetName(), TEXT("Icons.MotionDesign"));
}

FSlateColor FNavigationToolAvaSequence::GetIconColor() const
{
	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return FStyleColors::Foreground;
	}

	const UMovieSceneSequence* const RootSequence = Sequencer->GetRootMovieSceneSequence();
	if (!RootSequence)
	{
		return FStyleColors::Foreground;
	}

	if (GetAvaSequence() == RootSequence)
	{
		return FStyleColors::AccentGreen;
	}

	return FStyleColors::Foreground;
}

void FNavigationToolAvaSequence::OnSelect()
{
	Super::OnSelect();
}

void FNavigationToolAvaSequence::OnDoubleClick()
{
	UAvaSequence* const AvaSequence = GetAvaSequence();
	if (!AvaSequence)
	{
		return;
	}

	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	const TSharedPtr<IAvaSequencer> AvaSequencer = FAvaSequencerUtils::GetAvaSequencer(Sequencer.ToSharedRef());
	if (!AvaSequencer.IsValid())
	{
		return;
	}

	AvaSequencer->SetViewedSequence(AvaSequence);
}

UAvaSequence* FNavigationToolAvaSequence::GetAvaSequence() const
{
	return Cast<UAvaSequence>(GetSequence());
}

FNavigationToolItemId FNavigationToolAvaSequence::CalculateItemId() const
{
	return FNavigationToolItemId(GetParent(), GetAvaSequence());
}

TOptional<FColor> FNavigationToolAvaSequence::GetColor() const
{
	return Tool.FindItemColor(SharedThis(const_cast<FNavigationToolAvaSequence*>(this)));
}

void FNavigationToolAvaSequence::SetColor(const TOptional<FColor>& InColor)
{
	Tool.SetItemColor(SharedThis(this), InColor.Get(FColor()));
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE

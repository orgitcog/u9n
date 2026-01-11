// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationToolAvaSequenceDropHandler.h"
#include "AvaSequence.h"
#include "IAvaSequenceProvider.h"
#include "IAvaSequencer.h"
#include "IAvaSequencerProvider.h"
#include "INavigationTool.h"
#include "ItemActions/NavigationToolAddItem.h"
#include "ItemActions/NavigationToolRemoveItem.h"
#include "Items/NavigationToolAvaSequence.h"
#include "Misc/Optional.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NavigationToolAvaSequenceDropHandler"

namespace UE::SequenceNavigator
{

FNavigationToolAvaSequenceDropHandler::FNavigationToolAvaSequenceDropHandler(const TWeakPtr<IAvaSequencer>& InWeakAvaSequencer)
	: WeakAvaSequencer(InWeakAvaSequencer)
{
	
}

bool FNavigationToolAvaSequenceDropHandler::IsDraggedItemSupported(const FNavigationToolItemPtr& InDraggedItem) const
{
	return InDraggedItem->IsA<FNavigationToolAvaSequence>();
}

TOptional<EItemDropZone> FNavigationToolAvaSequenceDropHandler::CanDrop(const EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) const
{
	const bool bTargetIsRoot = (InTargetItem->GetItemId() == FNavigationToolItemId::RootId);

	UAvaSequence* TargetAvaSequence = nullptr;

	if (!bTargetIsRoot)
	{
		if (FNavigationToolAvaSequence* const TargetAvaSequenceItem = InTargetItem->CastTo<FNavigationToolAvaSequence>())
		{
			TargetAvaSequence = TargetAvaSequenceItem->GetAvaSequence();
		}
		if (!TargetAvaSequence)
		{
			return TOptional<EItemDropZone>();
		}
	}

	for (const FNavigationToolItemPtr& Item : Items)
	{
		// Only allow moving sequences to other Motion Design sequences
		const FNavigationToolAvaSequence* const AvaSequenceItemToMove = Item->CastTo<FNavigationToolAvaSequence>();
		if (!AvaSequenceItemToMove)
		{
			return TOptional<EItemDropZone>();
		}

		// Only allow moving valid sequences and sequences that aren't the target
		UAvaSequence* const AvaSequenceToMove = AvaSequenceItemToMove->GetAvaSequence();
		if (!AvaSequenceToMove || AvaSequenceToMove == TargetAvaSequence)
		{
			return TOptional<EItemDropZone>();
		}

		if (!bTargetIsRoot && TargetAvaSequence)
		{
			if (TargetAvaSequence->GetChildren().Contains(AvaSequenceToMove))
			{
				return TOptional<EItemDropZone>();
			}
		}
	}

	switch (ActionType)
	{
		case ENavigationToolDragDropActionType::Move:
			// Make sure the destination is not one of the items we're moving
			if (!Items.Contains(InTargetItem))
			{
				return InDropZone;
			}
			break;

		case ENavigationToolDragDropActionType::Copy:
			return InDropZone;
	}

	return TOptional<EItemDropZone>();
}

bool FNavigationToolAvaSequenceDropHandler::Drop(const EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem)
{
	switch (ActionType)
	{
	case ENavigationToolDragDropActionType::Move:
		MoveItems(InDropZone, InTargetItem);
		break;

	case ENavigationToolDragDropActionType::Copy:
		DuplicateItems(Items, InTargetItem, InDropZone);
		break;

	default:
		return false;
	}

	return true;
}

void FNavigationToolAvaSequenceDropHandler::MoveItems(const EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem)
{
	TArray<TSharedPtr<INavigationToolAction>> ItemActions;

	FNavigationToolAddItemParams AddParams;
	AddParams.RelativeItem = InTargetItem;
	AddParams.RelativeDropZone = InDropZone;
	AddParams.Flags = ENavigationToolAddItemFlags::Select | ENavigationToolAddItemFlags::Transact;
	AddParams.SelectionFlags = ENavigationToolItemSelectionFlags::AppendToCurrentSelection;

	const TSet<FNavigationToolItemPtr> DraggedItemSet(Items);

	// Remove all Items whose Parent is in the Item Set 
	Items.RemoveAll([&DraggedItemSet](const FNavigationToolItemPtr& InItem)
		{
			return !InItem.IsValid()
				|| !InItem->GetParent().IsValid()
				|| DraggedItemSet.Contains(InItem->GetParent());
		});

	// Reverse order for onto since Item->AddChild(...) adds it to Index 0, so last item would be at the top, which is reversed
	if (InDropZone == EItemDropZone::OntoItem)
	{
		Algo::Reverse(Items);
	}

	for (const FNavigationToolItemPtr& Item : Items)
	{
		// Only allow moving sequences to other Motion Design sequences
		if (FNavigationToolAvaSequence* const AvaSequenceItemToMove = Item->CastTo<FNavigationToolAvaSequence>())
		{
			AddParams.Item = SharedThis(AvaSequenceItemToMove);
			ItemActions.Add(MakeShared<FNavigationToolAddItem>(AddParams));
		}
	}

	InTargetItem->GetOwnerTool().EnqueueItemActions(MoveTemp(ItemActions));
}

void FNavigationToolAvaSequenceDropHandler::DuplicateItems(const TArray<FNavigationToolItemPtr>& InItems
	, const FNavigationToolItemPtr& InRelativeItem
	, const TOptional<EItemDropZone>& InRelativeDropZone)
{
	const TSharedPtr<IAvaSequencer> AvaSequencer = WeakAvaSequencer.Pin();
	if (!AvaSequencer.IsValid())
	{
		return;
	}

	const IAvaSequencerProvider& SequencerProvider = AvaSequencer->GetProvider();

	IAvaSequenceProvider* const SequenceProvider = SequencerProvider.GetSequenceProvider();
	if (!SequenceProvider)
	{
		return;
	}

	UObject* const Outer = SequenceProvider->ToUObject();
	if (!Outer)
	{
		return;
	}

	// Gather list of sequences to duplicate
	TSet<UAvaSequence*> SequencesToDuplicate;

	for (const FNavigationToolItemPtr& Item : InItems)
	{
		const FNavigationToolAvaSequence* const AvaSequenceItem = Item->CastTo<FNavigationToolAvaSequence>();
		if (!AvaSequenceItem)
		{
			continue;
		}

		UAvaSequence* const TemplateSequence = AvaSequenceItem->GetAvaSequence();
		if (!TemplateSequence)
		{
			continue;
		}

		SequencesToDuplicate.Add(TemplateSequence);
	}

	if (SequencesToDuplicate.IsEmpty())
	{
		return;
	}

	// Duplicate sequence objects
	const FScopedTransaction Transaction(LOCTEXT("DuplicateSequencesTransaction", "Duplicate Sequence(s)"));

	Outer->Modify();

	for (const UAvaSequence* const TemplateSequence : SequencesToDuplicate)
	{
		if (UAvaSequence* const DupedSequence = DuplicateObject<UAvaSequence>(TemplateSequence, Outer))
		{
			SequenceProvider->AddSequence(DupedSequence);
		}
	}
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE

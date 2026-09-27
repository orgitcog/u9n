// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/NavigationToolItemUtils.h"
#include "Containers/Array.h"
#include "Items/INavigationToolItem.h"
#include "Items/NavigationToolSequence.h"
#include "Items/NavigationToolTreeRoot.h"
#include "MovieSceneSequence.h"
#include "NavigationTool.h"
#include "NavigationToolView.h"
#include "ScopedTransaction.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "TrackEditors/SubTrackEditorBase.h"
#include "Utils/NavigationToolMiscUtils.h"

#define LOCTEXT_NAMESPACE "NavigationToolItemUtils"

namespace UE::SequenceNavigator
{

bool CompareToolItemOrder(const FNavigationToolItemPtr& InA, const FNavigationToolItemPtr& InB)
{
	if (!InA.IsValid())
	{
		return false;
	}

	if (!InB.IsValid())
	{
		return false;
	}

	if (const FNavigationToolItemPtr LowestCommonAncestor = FNavigationTool::FindLowestCommonAncestor({ InA, InB }))
	{
		const TArray<FNavigationToolItemPtr> PathToA = LowestCommonAncestor->FindPath({ InA });
		const TArray<FNavigationToolItemPtr> PathToB = LowestCommonAncestor->FindPath({ InB });

		int32 Index = 0;
	
		int32 PathAIndex = -1;
		int32 PathBIndex = -1;

		while (PathAIndex == PathBIndex)
		{
			if (!PathToA.IsValidIndex(Index))
			{
				return true;
			}
			if (!PathToB.IsValidIndex(Index))
			{
				return false;
			}

			PathAIndex = LowestCommonAncestor->GetChildIndex(PathToA[Index]);
			PathBIndex = LowestCommonAncestor->GetChildIndex(PathToB[Index]);
			++Index;
		}

		return PathAIndex < PathBIndex;
	}

	return false;
}

void SplitSortableAndUnsortableItems(const TArray<FNavigationToolItemPtr>& InItems
	, TArray<FNavigationToolItemPtr>& OutSortable
	, TArray<FNavigationToolItemPtr>& OutUnsortable)
{
	// Allocate both for worst case scenarios
	OutSortable.Reserve(InItems.Num());
	OutUnsortable.Reserve(InItems.Num());

	for (const FNavigationToolItemPtr& Item : InItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		if (Item->ShouldSort())
		{
			OutSortable.Add(Item);
		}
		else
		{
			OutUnsortable.Add(Item);
		}
	}
}

UMovieSceneSubSection* GetSequenceItemSubSection(const TSharedPtr<INavigationToolItem>& InItem)
{
	if (const FNavigationToolSequence* const SequenceItem = InItem->CastTo<FNavigationToolSequence>())
	{
		return SequenceItem->GetSubSection();
	}
	return nullptr;
}

UMovieSceneMetaData* GetSequenceItemMetaData(const TSharedPtr<INavigationToolItem>& InItem)
{
	if (const FNavigationToolSequence* const SequenceItem = InItem->CastTo<FNavigationToolSequence>())
	{
		return FSubTrackEditorUtil::FindOrAddMetaData(SequenceItem->GetSequence());
	}
	return nullptr;
}

void RemoveSequenceDisplayNameParentPrefix(FText& InOutDisplayName
	, const TSharedRef<FNavigationToolSequence>& InSequenceItem)
{
	UMovieSceneSequence* const Sequence = InSequenceItem->GetSequence();
	if (!Sequence)
	{
		return;
	}

	const FNavigationToolItemPtr Parent = InSequenceItem->GetParent();
	if (!Parent.IsValid() || Parent->IsA<FNavigationToolTreeRoot>())
	{
		return;
	}

	// Go up the hierarchy to find the parent sequence
	const FNavigationToolSequence* const ParentSequenceItem = InSequenceItem->FindAncestorOfType<FNavigationToolSequence>();
	if (!ParentSequenceItem)
	{
		return;
	}

	UMovieSceneSequence* const ParentSequence = ParentSequenceItem->GetSequence();
	if (!ParentSequence)
	{
		return;
	}

	const UMovieSceneCinematicShotSection* const ShotSection = Cast<UMovieSceneCinematicShotSection>(InSequenceItem->GetSubSection());

	FText NewSequenceName = ShotSection ? FText::FromString(ShotSection->GetShotDisplayName()) : Sequence->GetDisplayName();
	if (NewSequenceName.IsEmpty())
	{
		return;
	}

	// Remove the parent prefix from child name
	static FString SeparatorString = TEXT("_");
	const FString ParentSequenceNameStr = ParentSequence->GetDisplayName().ToString();

	FString Prefix;
	FString BaseName;
	if (ParentSequenceNameStr.Split(SeparatorString, &Prefix, &BaseName))
	{
		Prefix += SeparatorString;

		FString NewSequenceNameStr = NewSequenceName.ToString();

		if (NewSequenceNameStr.StartsWith(Prefix)
			&& NewSequenceNameStr.Len() > Prefix.Len())
		{
			NewSequenceNameStr.RemoveFromStart(Prefix);
			NewSequenceName = FText::FromString(NewSequenceNameStr);
		}
	}

	InOutDisplayName = NewSequenceName;
}

void AppendSequenceDisplayNameDirtyStatus(FText& InOutDisplayName
	, const UMovieSceneSequence& InSequence)
{
	UPackage* const Package = InSequence.GetPackage();
	if (Package && Package->IsDirty())
	{
		InOutDisplayName = FText::Format(LOCTEXT("DirtySymbol", "{0}*"), InOutDisplayName);
	}
}

FSlateColor GetItemBindingColor(const ISequencer& InSequencer
	, UMovieSceneSequence& InSequence
	, const FGuid& InObjectGuid
	, const FSlateColor& DefaultColor)
{
	const TArrayView<TWeakObjectPtr<>> BoundObjects = ResolveBoundObjects(InSequencer, &InSequence, InObjectGuid);
	if (BoundObjects.Num() > 0)
	{
		int32 NumValidObjects = 0;

		for (const TWeakObjectPtr<>& BoundObject : BoundObjects)
		{
			if (BoundObject.IsValid())
			{
				++NumValidObjects;
			}
		}

		if (NumValidObjects == BoundObjects.Num())
		{
			return DefaultColor;
		}

		if (NumValidObjects > 0)
		{
			return FStyleColors::Warning;
		}
	}

	return FStyleColors::Error;
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE

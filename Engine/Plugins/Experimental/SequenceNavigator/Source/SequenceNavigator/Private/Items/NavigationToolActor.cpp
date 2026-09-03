// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/NavigationToolActor.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/Actor.h"
#include "ISequencer.h"
#include "Items/NavigationToolComponent.h"
#include "Items/NavigationToolSequence.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSequence.h"
#include "NavigationTool.h"
#include "NavigationToolSettings.h"
#include "ScopedTransaction.h"
#include "Utils/NavigationToolMiscUtils.h"

#define LOCTEXT_NAMESPACE "NavigationToolActor"

namespace UE::SequenceNavigator
{

FNavigationToolActor::FNavigationToolActor(INavigationTool& InTool
	, const FNavigationToolItemPtr& InParentItem
	, const TSharedPtr<FNavigationToolSequence>& InParentSequenceItem
	, const FMovieSceneBinding& InBinding)
	: Super(InTool, InParentItem, InParentSequenceItem, InBinding)
{
}

void FNavigationToolActor::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive)
{
	Super::FindChildren(OutChildren, bInRecursive);

	const AActor* const ThisActor = GetActor();
	if (!ThisActor)
	{
		return;
	}

	const TSharedPtr<FNavigationToolSequence> ParentSequenceItem = WeakParentSequenceItem.Pin();
	if (!ParentSequenceItem.IsValid())
	{
		return;
	}

	UMovieSceneSequence* const Sequence = ParentSequenceItem->GetSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* const MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	const TSharedRef<FNavigationToolActor> SharedThisRef = SharedThis(this);

	const TArray<FMovieSceneBinding> Bindings = ParentSequenceItem->GetSortedBindings();
	for (const FMovieSceneBinding& CurrentBinding : Bindings)
	{
		const FGuid& CurrentBindingGuid = CurrentBinding.GetObjectGuid();
		const UClass* const CurrentObjectClass = MovieSceneHelpers::GetBoundObjectClass(Sequence, CurrentBindingGuid);
		if (CurrentObjectClass && CurrentObjectClass->IsChildOf<UActorComponent>())
		{
			const TArrayView<TWeakObjectPtr<>> WeakBoundObjects = ResolveBoundObjects(*Sequencer, Sequence, CurrentBindingGuid);
			const TWeakObjectPtr<> CurrentBoundObject = WeakBoundObjects.IsEmpty() ? nullptr : WeakBoundObjects[0];

			const UActorComponent* const CurrentBoundActorComponent = Cast<UActorComponent>(CurrentBoundObject);
			if (CurrentBoundActorComponent && CurrentBoundActorComponent->GetOwner() == ThisActor)
			{
				const FNavigationToolItemPtr NewItem = Tool.FindOrAdd<FNavigationToolComponent>(GetProvider()
					, SharedThisRef, ParentSequenceItem, CurrentBinding);
				OutChildren.Add(NewItem);
				if (bInRecursive)
				{
					NewItem->FindChildren(OutChildren, bInRecursive);
				}
			}
		}
	}
}

bool FNavigationToolActor::IsAllowedInTool() const
{
	FNavigationTool& ToolPrivate = static_cast<FNavigationTool&>(Tool);

	const AActor* const UnderlyingActor = GetActor();
	if (!UnderlyingActor)
	{
		// Always allow unbound binding items
		return true;
	}

	const bool bActorAllowed = ToolPrivate.IsObjectAllowedInTool(UnderlyingActor);

	return bActorAllowed;
}

ENavigationToolItemViewMode FNavigationToolActor::GetSupportedViewModes(const INavigationToolView& InToolView) const
{
	return ENavigationToolItemViewMode::ItemTree | ENavigationToolItemViewMode::HorizontalItemList;
}

TArray<FName> FNavigationToolActor::GetTags() const
{
	if (const AActor* const UnderlyingActor = GetActor())
	{
		return UnderlyingActor->Tags;
	}
	return Super::GetTags();
}

bool FNavigationToolActor::GetVisibility() const
{
	if (const AActor* const UnderlyingActor = GetActor())
	{
		return !UnderlyingActor->IsTemporarilyHiddenInEditor(true);
	}
	return false;
}

void FNavigationToolActor::OnVisibilityChanged(const bool bInNewVisibility)
{
	if (AActor* const UnderlyingActor = GetActor())
	{
		UnderlyingActor->SetIsTemporarilyHiddenInEditor(!bInNewVisibility);
	}
}

bool FNavigationToolActor::CanRename() const
{
	return Super::CanRename() && GetActor() != nullptr;
}

bool FNavigationToolActor::Rename(const FString& InName)
{
	AActor* const UnderlyingActor = GetActor();
	const bool bIsActorLabelEditable = UnderlyingActor && UnderlyingActor->IsActorLabelEditable();

	bool bRenamed = false;
	
	if (bIsActorLabelEditable && !InName.Equals(UnderlyingActor->GetActorLabel(), ESearchCase::CaseSensitive))
	{
		const FScopedTransaction Transaction(LOCTEXT("NavigationToolRenameActor", "Rename Actor"));

		FActorLabelUtilities::RenameExistingActor(UnderlyingActor, InName);

		Super::Rename(InName);

		bRenamed = true;
	}

	return bRenamed;
}

AActor* FNavigationToolActor::GetActor() const
{
	return WeakBoundObject.IsValid() ? Cast<AActor>(GetCachedBoundObject()) : nullptr;
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE

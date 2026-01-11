// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/NavigationToolComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Items/NavigationToolItemProxy.h"
#include "NavigationTool.h"
#include "ScopedTransaction.h"
#include "Styling/StyleColors.h"
#include "Widgets/Columns/SNavigationToolLabelComponent.h"

#define LOCTEXT_NAMESPACE "NavigationToolComponent"

namespace UE::SequenceNavigator
{

FNavigationToolComponent::FNavigationToolComponent(INavigationTool& InTool
	, const FNavigationToolItemPtr& InParentItem
	, const TSharedPtr<FNavigationToolSequence>& InParentSequenceItem
	, const FMovieSceneBinding& InBinding)
	: Super(InTool, InParentItem, InParentSequenceItem, InBinding)
{
}

void FNavigationToolComponent::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive)
{
	Super::FindChildren(OutChildren, bInRecursive);
}

void FNavigationToolComponent::GetItemProxies(TArray<TSharedPtr<FNavigationToolItemProxy>>& OutItemProxies)
{
	Super::GetItemProxies(OutItemProxies);

	if (UPrimitiveComponent* const PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent()))
	{
		if (const TSharedPtr<FNavigationToolItemProxy> MaterialItemProxy = Tool.GetOrCreateItemProxy<FNavigationToolItemProxy>(SharedThis(this)))
		{
			OutItemProxies.Add(MaterialItemProxy);
		}
	}
}

bool FNavigationToolComponent::IsAllowedInTool() const
{
	FNavigationTool& ToolPrivate = static_cast<FNavigationTool&>(Tool);

	const UActorComponent* const UnderlyingComponent = GetComponent();
	if (!UnderlyingComponent)
	{
		// Always allow unbound binding items
		return true;
	}

	const bool bOwnerAllowed = ToolPrivate.IsObjectAllowedInTool(UnderlyingComponent->GetOwner());
	const bool bComponentAllowed = ToolPrivate.IsObjectAllowedInTool(UnderlyingComponent);

	return bOwnerAllowed && bComponentAllowed;
}

ENavigationToolItemViewMode FNavigationToolComponent::GetSupportedViewModes(const INavigationToolView& InToolView) const
{
	return ENavigationToolItemViewMode::ItemTree | ENavigationToolItemViewMode::HorizontalItemList;
}

TSharedRef<SWidget> FNavigationToolComponent::GenerateLabelWidget(const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	return SNew(SNavigationToolLabelComponent, SharedThis(this), InRow);
}

FLinearColor FNavigationToolComponent::GetItemTintColor() const
{
	return FStyleColors::White25.GetSpecifiedColor();
}

TArray<FName> FNavigationToolComponent::GetTags() const
{
	if (const UActorComponent* const UnderlyingComponent = GetComponent())
	{
		return UnderlyingComponent->ComponentTags;
	}
	return Super::GetTags();
}

bool FNavigationToolComponent::GetVisibility() const
{
	if (const USceneComponent* const UnderlyingComponent = Cast<USceneComponent>(GetComponent()))
	{
		return UnderlyingComponent->IsVisibleInEditor();
	}
	return false;
}

void FNavigationToolComponent::OnVisibilityChanged(const bool bInNewVisibility)
{
	if (USceneComponent* const UnderlyingComponent = Cast<USceneComponent>(GetComponent()))
	{
		UnderlyingComponent->SetVisibility(bInNewVisibility);
	}
}

bool FNavigationToolComponent::CanRename() const
{
	return Super::CanRename() && GetComponent() != nullptr;
}

bool FNavigationToolComponent::Rename(const FString& InName)
{
	UActorComponent* const UnderlyingComponent = GetComponent();
	if (!UnderlyingComponent)
	{
		return false;
	}

	bool bRenamed = false;

	if (!InName.Equals(UnderlyingComponent->GetName(), ESearchCase::CaseSensitive))
	{
		const FScopedTransaction Transaction(LOCTEXT("NavigationToolRenameComponent", "Rename Component"));

		UnderlyingComponent->Modify();
		UnderlyingComponent->Rename(*InName);

		Super::Rename(InName);

		bRenamed = true;
	}

	return bRenamed;
}

UActorComponent* FNavigationToolComponent::GetComponent() const
{
	return WeakBoundObject.IsValid() ? Cast<UActorComponent>(GetCachedBoundObject()) : nullptr;
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE

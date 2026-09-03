// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationToolTreeRoot.h"
#include "NavigationTool.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/STableRow.h"

namespace UE::SequenceNavigator
{

FNavigationToolTreeRoot::FNavigationToolTreeRoot(INavigationTool& InTool)
	: FNavigationToolItem(InTool, nullptr)
{
}

void FNavigationToolTreeRoot::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive)
{
	Super::FindChildren(OutChildren, bInRecursive);

	const TSharedRef<FNavigationToolTreeRoot> SharedThisRef = SharedThis(this);

	// Get items to add from providers
	GetOwnerTool().ForEachProvider([this, &OutChildren, bInRecursive, &SharedThisRef]
		(const TSharedRef<FNavigationToolProvider>& InToolProvider)
		{
			TArray<FNavigationToolItemPtr> ThisProviderItems;
			InToolProvider->OnExtendItemChildren(Tool, SharedThisRef, ThisProviderItems, bInRecursive);

			OutChildren.Append(ThisProviderItems);

			return true;
		});
}

bool FNavigationToolTreeRoot::CanAddChild(const FNavigationToolItemPtr& InChild) const
{
	return Super::CanAddChild(InChild) && InChild->CanBeTopLevel();
}

bool FNavigationToolTreeRoot::IsAllowedInTool() const
{
	checkNoEntry();
	return false;
}

FText FNavigationToolTreeRoot::GetDisplayName() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

FText FNavigationToolTreeRoot::GetClassName() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

FText FNavigationToolTreeRoot::GetIconTooltipText() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

FSlateIcon FNavigationToolTreeRoot::GetIcon() const
{
	checkNoEntry();
	return FSlateIcon();
}

TSharedRef<SWidget> FNavigationToolTreeRoot::GenerateLabelWidget(const TSharedRef<SNavigationToolTreeRow>& InRow)
{
	checkNoEntry();
	return SNullWidget::NullWidget;
}

TOptional<EItemDropZone> FNavigationToolTreeRoot::CanAcceptDrop(const FDragDropEvent& InDragDropEvent, const EItemDropZone InDropZone)
{
	return Super::CanAcceptDrop(InDragDropEvent, InDropZone);
}

FReply FNavigationToolTreeRoot::AcceptDrop(const FDragDropEvent& InDragDropEvent, const EItemDropZone InDropZone)
{
	// Force tree root to always be OntoItem so we always create new items as children of root
	constexpr EItemDropZone ForcedDropZone = EItemDropZone::OntoItem;
	return Super::AcceptDrop(InDragDropEvent, ForcedDropZone);
}

FNavigationToolItemId FNavigationToolTreeRoot::CalculateItemId() const
{
	return FNavigationToolItemId::RootId;
}

} // namespace UE::SequenceNavigator

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/NavigationToolFilterData.h"
#include "Items/INavigationToolItem.h"

namespace UE::SequenceNavigator
{

using namespace UE::Sequencer;

FNavigationToolFilterData::FNavigationToolFilterData(const FString& InRawFilterText)
	: RawFilterText(InRawFilterText)
{
}

bool FNavigationToolFilterData::operator==(const FNavigationToolFilterData& InRhs) const
{
	return ContainsFilterInNodes(InRhs) && TotalNodeCount == InRhs.GetTotalNodeCount();
}

bool FNavigationToolFilterData::operator!=(const FNavigationToolFilterData& InRhs) const
{
	return !(*this == InRhs);
}

void FNavigationToolFilterData::Reset()
{
	RawFilterText.Reset();
	TotalNodeCount = 0;
	FilterInNodes.Reset();
}

FString FNavigationToolFilterData::GetRawFilterText() const
{
	return RawFilterText;
}

uint32 FNavigationToolFilterData::GetDisplayNodeCount() const
{
	return FilterInNodes.Num();
}

uint32 FNavigationToolFilterData::GetTotalNodeCount() const
{
	return TotalNodeCount;
}

uint32 FNavigationToolFilterData::GetFilterInCount() const
{
	return FilterInNodes.Num();
}

uint32 FNavigationToolFilterData::GetFilterOutCount() const
{
	return GetTotalNodeCount() - GetFilterInCount();
}

void FNavigationToolFilterData::IncrementTotalNodeCount()
{
	++TotalNodeCount;
}

void FNavigationToolFilterData::FilterInNode(const FNavigationToolItemPtr& InNode)
{
	FilterInNodes.Add(InNode);
}

void FNavigationToolFilterData::FilterOutNode(const FNavigationToolItemPtr& InNode)
{
	const FSetElementId ElementId = FilterInNodes.FindId(InNode);
	if (ElementId.IsValidId())
	{
		FilterInNodes.Remove(ElementId);
	}
}

void FNavigationToolFilterData::FilterInParentChildNodes(const FNavigationToolItemPtr& InNode
	, const bool bInIncludeSelf, const bool bInIncludeParents, const bool bInIncludeChildren)
{
	if (!InNode.IsValid())
	{
		return;
	}

	if (bInIncludeParents)
	{
		FNavigationToolItemPtr CurrentParent = InNode->GetParent();
		while (CurrentParent.IsValid())
		{
			FilterInNode(CurrentParent);

			CurrentParent = CurrentParent->GetParent();
			if (CurrentParent.IsValid() && CurrentParent->GetItemId() == FNavigationToolItemId::RootId)
			{
				CurrentParent = nullptr;
			}
		}
	}

	if (bInIncludeSelf)
	{
		FilterInNode(InNode);
	}

	if (bInIncludeChildren)
	{
		auto AddAllChildren = [this](const FNavigationToolItemPtr& InNode)
		{
			for (const FNavigationToolItemPtr& Child : InNode->GetChildren())
			{
				FilterInNode(Child);
			}
		};

		for (const FNavigationToolItemPtr& Child : InNode->GetChildren())
		{
			AddAllChildren(Child);
		}
	}
}

void FNavigationToolFilterData::FilterInNodeWithAncestors(const FNavigationToolItemPtr& InNode)
{
	FilterInParentChildNodes(InNode, true, true, false);
}

bool FNavigationToolFilterData::ContainsFilterInNodes(const FNavigationToolFilterData& InOtherData) const
{
	return FilterInNodes.Includes(InOtherData.FilterInNodes);
}

bool FNavigationToolFilterData::IsFilteredIn(const FNavigationToolItemPtr& InNode) const
{
	return FilterInNodes.Contains(InNode);
}

bool FNavigationToolFilterData::IsFilteredOut(const FNavigationToolItemPtr& InNode) const
{
	return !FilterInNodes.Contains(InNode);
}

} // namespace UE::SequenceNavigator

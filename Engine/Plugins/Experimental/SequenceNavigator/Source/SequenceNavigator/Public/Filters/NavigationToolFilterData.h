// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "UObject/WeakObjectPtr.h"

namespace UE::SequenceNavigator
{

using FNavigationToolFilterType = TSharedPtr<INavigationToolItem>;

/** Represents a cache between nodes for a filter operation. */
struct SEQUENCENAVIGATOR_API FNavigationToolFilterData
{
	FNavigationToolFilterData(const FString& InRawFilterText);

	bool operator==(const FNavigationToolFilterData& InRhs) const;
	bool operator!=(const FNavigationToolFilterData& InRhs) const;

	void Reset();

	FString GetRawFilterText() const;

	uint32 GetDisplayNodeCount() const;
	uint32 GetTotalNodeCount() const;

	uint32 GetFilterInCount() const;
	uint32 GetFilterOutCount() const;

	void IncrementTotalNodeCount();

	void FilterInNode(const FNavigationToolItemPtr& InNode);
	void FilterOutNode(const FNavigationToolItemPtr& InNode);

	void FilterInParentChildNodes(const FNavigationToolItemPtr& InNode
		, const bool bInIncludeSelf, const bool bInIncludeParents, const bool bInIncludeChildren = false);

	void FilterInNodeWithAncestors(const FNavigationToolItemPtr& InNode);

	bool ContainsFilterInNodes(const FNavigationToolFilterData& InOtherData) const;

	bool IsFilteredIn(const FNavigationToolItemPtr& InNode) const;
	bool IsFilteredOut(const FNavigationToolItemPtr& InNode) const;

protected:
	FString RawFilterText;

	uint32 TotalNodeCount = 0;

	/** Nodes to be displayed in the UI */
	TSet<FNavigationToolItemWeakPtr> FilterInNodes;
};

} // namespace UE::SequenceNavigator

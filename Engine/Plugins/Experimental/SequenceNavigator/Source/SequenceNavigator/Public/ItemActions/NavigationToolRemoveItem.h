// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemAction.h"
#include "Items/NavigationToolItemParameters.h"

namespace UE::SequenceNavigator
{

class FNavigationTool;

/**
 * Item Action responsible for removing/unregistering items from the tree
 */
class SEQUENCENAVIGATOR_API FNavigationToolRemoveItem : public INavigationToolAction
{
public:
	UE_NAVIGATIONTOOL_INHERITS(FNavigationToolRemoveItem, INavigationToolAction);

	FNavigationToolRemoveItem(const FNavigationToolRemoveItemParams& InRemoveItemParams);

	//~ Begin INavigationToolAction
	virtual void Execute(FNavigationTool& InTool) override;
	virtual void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bInRecursive) override;
	//~ End INavigationToolAction

protected:
	FNavigationToolRemoveItemParams RemoveParams;
};

} // namespace UE::SequenceNavigator

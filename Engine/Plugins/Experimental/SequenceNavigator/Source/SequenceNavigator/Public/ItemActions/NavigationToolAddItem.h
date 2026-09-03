// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Items/NavigationToolItemParameters.h"
#include "NavigationToolItemAction.h"

namespace UE::SequenceNavigator
{

class FNavigationTool;

/**
 * Item action responsible for adding an item to the tree under a given optional parent.
 * If Parent is null, it is added as a Top Level Item.
 */
class SEQUENCENAVIGATOR_API FNavigationToolAddItem : public INavigationToolAction
{
public:
	UE_NAVIGATIONTOOL_INHERITS(FNavigationToolAddItem, INavigationToolAction);

	FNavigationToolAddItem(const FNavigationToolAddItemParams& InAddItemParams);

	//~ Begin INavigationToolAction
	virtual bool ShouldTransact() const override;
	virtual void Execute(FNavigationTool& InTool) override;
	virtual void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bRecursive) override;
	//~ End INavigationToolAction

protected:
	FNavigationToolAddItemParams AddParams;
};

} // namespace UE::SequenceNavigator

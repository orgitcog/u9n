// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemType.h"

class UObject;

namespace UE::SequenceNavigator
{

class FNavigationTool;

/**
 * Interface class for an Action in the Navigation Tool (e.g. Add/Delete/Move Tree Item)
 */ 
class INavigationToolAction : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(INavigationToolAction, INavigationToolItemTypeCastable);

	/** Determines whether the given action modifies its objects and should transact */
	virtual bool ShouldTransact() const { return false; }

	/** The Action to execute on the given Navigation Tool */
	virtual void Execute(FNavigationTool& InTool) = 0;

	/** Replace any Objects that might be held in this Action that has been killed and replaced by a new object (e.g. BP Components) */
	virtual void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, const bool bInRecursive) = 0;
};

} // namespace UE::SequenceNavigator

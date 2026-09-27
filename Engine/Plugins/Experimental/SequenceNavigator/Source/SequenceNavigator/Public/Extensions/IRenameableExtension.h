// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

class IRenameableExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(IRenameableExtension, INavigationToolItemTypeCastable)

	/** @return True if this item can be renamed */
	SEQUENCENAVIGATOR_API virtual bool CanRename() const = 0;

	/** The implementation to rename the item */
	SEQUENCENAVIGATOR_API virtual bool Rename(const FString& InName) = 0;
};

} // namespace UE::SequenceNavigator

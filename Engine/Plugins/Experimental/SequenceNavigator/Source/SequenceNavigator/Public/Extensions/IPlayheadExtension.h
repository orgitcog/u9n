// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

enum class EItemContainsPlayhead
{
	None                      = 0,
	ContainsPlayhead          = 1 << 0,
	PartiallyContainsPlayhead = 1 << 1,
};
ENUM_CLASS_FLAGS(EItemContainsPlayhead)

class IPlayheadExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(IPlayheadExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual EItemContainsPlayhead ContainsPlayhead() const = 0;
};

} // namespace UE::SequenceNavigator

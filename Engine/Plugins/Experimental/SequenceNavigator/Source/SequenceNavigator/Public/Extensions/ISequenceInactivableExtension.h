// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

enum class EItemSequenceInactiveState
{
	None              = 0,
	Inactive          = 1 << 0,
	PartiallyInactive = 1 << 1,
};
ENUM_CLASS_FLAGS(EItemSequenceInactiveState)

class ISequenceInactivableExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(ISequenceInactivableExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual EItemSequenceInactiveState GetInactiveState() const = 0;

	SEQUENCENAVIGATOR_API virtual void SetIsInactive(const bool bInIsMuted) = 0;
};

} // namespace UE::SequenceNavigator

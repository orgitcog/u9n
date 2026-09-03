// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

enum class EItemSequenceLockState
{
	None            = 0,
	Locked          = 1 << 0,
	PartiallyLocked = 1 << 1,
};
ENUM_CLASS_FLAGS(EItemSequenceLockState)

class ISequenceLockableExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(ISequenceLockableExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual EItemSequenceLockState GetLockState() const = 0;

	SEQUENCENAVIGATOR_API virtual void SetIsLocked(const bool bInIsLocked) = 0;
};

} // namespace UE::SequenceNavigator

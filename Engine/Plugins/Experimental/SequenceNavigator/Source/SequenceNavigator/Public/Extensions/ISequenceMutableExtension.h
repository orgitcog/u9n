// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

enum class EItemSequenceMuteState
{
	None           = 0,
	Muted          = 1 << 0,
	PartiallyMuted = 1 << 1,
};
ENUM_CLASS_FLAGS(EItemSequenceMuteState)

class ISequenceMutableExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(ISequenceMutableExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual EItemSequenceMuteState GetMuteState() const = 0;

	SEQUENCENAVIGATOR_API virtual void SetIsMuted(const bool bInIsMuted) = 0;
};

} // namespace UE::SequenceNavigator

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameNumber.h"
#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

class IInTimeExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(IInTimeExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual FFrameNumber GetInTime() const = 0;

	SEQUENCENAVIGATOR_API virtual void SetInTime(const FFrameNumber& InTime) = 0;
};

} // namespace UE::SequenceNavigator

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameNumber.h"
#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

class IOutTimeExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(IOutTimeExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual FFrameNumber GetOutTime() const = 0;

	SEQUENCENAVIGATOR_API virtual void SetOutTime(const FFrameNumber& InTime) = 0;
};

} // namespace UE::SequenceNavigator

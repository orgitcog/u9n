// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

class IColorExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(IColorExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual TOptional<FColor> GetColor() const = 0;

	SEQUENCENAVIGATOR_API virtual void SetColor(const TOptional<FColor>& InColor) = 0;
};

} // namespace UE::SequenceNavigator

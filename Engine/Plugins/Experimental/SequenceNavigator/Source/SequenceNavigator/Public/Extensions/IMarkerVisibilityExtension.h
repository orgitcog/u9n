// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolItemType.h"

namespace UE::SequenceNavigator
{

enum class EItemMarkerVisibility
{
	None             = 0,
	Visible          = 1 << 0,
	PartiallyVisible = 1 << 1,
};
ENUM_CLASS_FLAGS(EItemMarkerVisibility)

class IMarkerVisibilityExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(IMarkerVisibilityExtension, INavigationToolItemTypeCastable)

	SEQUENCENAVIGATOR_API virtual EItemMarkerVisibility GetMarkerVisibility() const = 0;

	SEQUENCENAVIGATOR_API virtual void SetMarkerVisibility(const bool bInVisible) = 0;
};

} // namespace UE::SequenceNavigator

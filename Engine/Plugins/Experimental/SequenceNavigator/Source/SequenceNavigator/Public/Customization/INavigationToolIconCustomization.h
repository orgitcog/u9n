// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

struct FSlateIcon;

namespace UE::SequenceNavigator
{

class FNavigationToolItem;

class INavigationToolIconCustomization
{
public:
	virtual ~INavigationToolIconCustomization() = default;

	virtual FName GetToolItemIdentifier() const = 0;

	virtual bool HasOverrideIcon(TSharedPtr<const FNavigationToolItem> InToolItem) const = 0;

	virtual FSlateIcon GetOverrideIcon(TSharedPtr<const FNavigationToolItem> InToolItem) const = 0;
};

} // namespace UE::SequenceNavigator

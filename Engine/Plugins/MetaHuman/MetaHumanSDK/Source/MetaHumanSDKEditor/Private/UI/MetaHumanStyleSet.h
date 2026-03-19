// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Styling/SlateStyle.h"

namespace UE::MetaHuman
{
/**
 * Class to manage styles used by the MetaHuman SDK
 */
class FMetaHumanStyleSet : public FSlateStyleSet
{
public:
	// Management
	static FMetaHumanStyleSet& Get();

private:
	FMetaHumanStyleSet();
};
} // namespace UE::MetaHuman

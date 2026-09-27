// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

/**
 * Slate style set for the MetaHuman Character
 */
class FMetaHumanCharacterStyle : public FSlateStyleSet
{
public:
	static const FMetaHumanCharacterStyle& Get();

	static void Register();
	static void Unregister();

private:
	FMetaHumanCharacterStyle();
};
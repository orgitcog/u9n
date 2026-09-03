// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class METAHUMANTOOLKIT_API FMetaHumanToolkitStyle
	: public FSlateStyleSet
{
public:
	static FMetaHumanToolkitStyle& Get();

	static void Register();
	static void Unregister();

private:
	FMetaHumanToolkitStyle();
};
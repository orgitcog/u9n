// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class METAHUMANIDENTITY_API FMetaHumanIdentityStyle
	: public FSlateStyleSet
{
public:
	static FMetaHumanIdentityStyle& Get();

	static void Register();
	static void Unregister();

private:
	FMetaHumanIdentityStyle();
};
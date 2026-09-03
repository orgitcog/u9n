// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** A Style set for MetaHuman Config asset */
class FMetaHumanConfigStyle : public FSlateStyleSet
{
public:

	virtual const FName& GetStyleSetName() const override;
	static const FMetaHumanConfigStyle& Get();

	static void ReloadTextures();

	static void Register();
	static void Unregister();

private:

	FMetaHumanConfigStyle();

	static FName StyleName;
};

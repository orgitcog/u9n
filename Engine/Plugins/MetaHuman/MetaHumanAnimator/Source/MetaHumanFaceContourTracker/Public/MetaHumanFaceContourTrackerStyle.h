// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** A Style set for MetaHuman Face Contour Tracker asset */
class FMetaHumanFaceContourTrackerStyle : public FSlateStyleSet
{
public:

	virtual const FName& GetStyleSetName() const override;
	static const FMetaHumanFaceContourTrackerStyle& Get();

	static void ReloadTextures();

	static void Register();
	static void Unregister();

private:

	FMetaHumanFaceContourTrackerStyle();

	static FName StyleName;
};
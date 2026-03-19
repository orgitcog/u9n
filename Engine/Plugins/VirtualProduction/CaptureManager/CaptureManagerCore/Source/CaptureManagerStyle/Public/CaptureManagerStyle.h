// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** A style set for Capture Manager */
class CAPTUREMANAGERSTYLE_API FCaptureManagerStyle : public FSlateStyleSet
{
public:

	virtual const FName& GetStyleSetName() const override;
	static const FCaptureManagerStyle& Get();

	static void ReloadTextures();

private:

	FCaptureManagerStyle();

	static FName StyleName;
};
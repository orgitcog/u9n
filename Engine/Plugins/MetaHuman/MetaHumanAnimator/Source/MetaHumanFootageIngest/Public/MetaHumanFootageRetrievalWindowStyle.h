// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** A Style set for Footage Retrieval window */
class FMetaHumanFootageRetrievalWindowStyle: public FSlateStyleSet
{
public:

	virtual const FName& GetStyleSetName() const override;
	static const FMetaHumanFootageRetrievalWindowStyle& Get();

	static void ReloadTextures();

	static void Register();
	static void Unregister();

private:

	FMetaHumanFootageRetrievalWindowStyle();

	static FName StyleName;
};
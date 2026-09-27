// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"


class FGeometryCacheLevelSequenceBakerStyle
	: public FSlateStyleSet
{
public:
	static FGeometryCacheLevelSequenceBakerStyle& Get();

protected:
	friend class FGeometryCacheLevelSequenceBakerModule;

	static void Register();
	static void Unregister();

private:
	FGeometryCacheLevelSequenceBakerStyle();
};
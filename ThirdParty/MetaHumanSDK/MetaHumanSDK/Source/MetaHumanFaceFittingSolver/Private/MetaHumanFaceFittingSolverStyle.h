// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** A Style set for MetaHuman Face Fitting Solver asset */
class FMetaHumanFaceFittingSolverStyle : public FSlateStyleSet
{
public:

	virtual const FName& GetStyleSetName() const override;
	static const FMetaHumanFaceFittingSolverStyle& Get();

	static void ReloadTextures();

	static void Register();
	static void Unregister();

private:

	FMetaHumanFaceFittingSolverStyle();

	static FName StyleName;
};

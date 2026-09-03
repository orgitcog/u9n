// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** A Style set for MetaHuman Face Animation Solver asset */
class FMetaHumanFaceAnimationSolverStyle : public FSlateStyleSet
{
public:

	virtual const FName& GetStyleSetName() const override;
	static const FMetaHumanFaceAnimationSolverStyle& Get();

	static void ReloadTextures();

	static void Register();
	static void Unregister();

private:

	FMetaHumanFaceAnimationSolverStyle();

	static FName StyleName;
};

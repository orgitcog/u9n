// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"


class FSkeletalMeshMorphTargetEditingToolsStyle
	: public FSlateStyleSet
{
public:
	static FSkeletalMeshMorphTargetEditingToolsStyle& Get();

protected:
	friend class FSkeletalMeshMorphTargetEditingToolsModule;

	static void Register();
	static void Unregister();

private:
	FSkeletalMeshMorphTargetEditingToolsStyle();
};
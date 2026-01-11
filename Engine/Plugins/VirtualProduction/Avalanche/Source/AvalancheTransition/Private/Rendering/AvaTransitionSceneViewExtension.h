// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"

class FAvaTransitionSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FAvaTransitionSceneViewExtension(const FAutoRegister& InAutoRegister)
		: FSceneViewExtensionBase(InAutoRegister)
	{
	}

	//~ Begin FSceneViewExtensionBase
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	//~ End FSceneViewExtensionBase
};

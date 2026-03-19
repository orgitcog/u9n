// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"

class FMobileFSRViewExtension final : public FSceneViewExtensionBase
{
public:
	FMobileFSRViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

	MOBILEFSR_API void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
};
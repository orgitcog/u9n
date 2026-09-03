// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

enum EPixelFormat : uint8;

namespace LumenShortRangeAO
{
	bool ShouldApplyDuringIntegration();
	bool UseBentNormal();
	EPixelFormat GetTextureFormat();
	uint32 GetDownsampleFactor();
	bool UseTemporal();
	float GetTemporalNeighborhoodClampScale();
};
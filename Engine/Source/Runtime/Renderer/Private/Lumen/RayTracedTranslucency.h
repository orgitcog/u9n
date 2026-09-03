// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Lumen/LumenReflections.h"

namespace RayTracedTranslucency
{
	bool IsEnabled(const FViewInfo& View);
	bool UseForceOpaque();
	bool UseRayTracedRefraction(const TArray<FViewInfo>& Views);
	bool AllowTranslucentReflectionInReflections();
	float GetPathThroughputThreshold();
	uint32 GetDownsampleFactor(const TArray<FViewInfo>& Views);
	uint32 GetMaxPrimaryHitEvents(const FViewInfo& View);
	uint32 GetMaxSecondaryHitEvents(const FViewInfo& View);
}

extern void TraceTranslucency(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FSceneTextures& SceneTextures,
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenReflectionTileParameters& ReflectionTileParameters,
	EDiffuseIndirectMethod DiffuseIndirectMethod,
	ERDGPassFlags ComputePassFlags,
	bool bUseRayTracedRefraction = false);

extern void RenderHardwareRayTracingTranslucency(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures,
	const FSceneTextureParameters& SceneTextureParameters,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenCardTracingParameters& TracingParameters,
	const FLumenReflectionTracingParameters& ReflectionTracingParameters,
	const FLumenReflectionTileParameters& ReflectionTileParameters,
	float MaxTraceDistance,
	EDiffuseIndirectMethod DiffuseIndirectMethod,
	ERDGPassFlags ComputePassFlags,
	bool bUseRayTracedRefraction = false);

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DebugRenderSceneProxy.h"

#define UE_API SMARTOBJECTSMODULE_API

/**
 * Simple DebugRenderSceneProxy that gets relevant when associated component is shown and view flag is active (if specified on construction)
 */
#if UE_ENABLE_DEBUG_DRAWING
class FSmartObjectDebugSceneProxy final : public FDebugRenderSceneProxy
{
public:
	UE_API explicit FSmartObjectDebugSceneProxy(const UPrimitiveComponent& InComponent, const EDrawType InDrawType = EDrawType::WireMesh, const TCHAR* InViewFlagName = nullptr);

	UE_API virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	UE_API virtual uint32 GetMemoryFootprint() const override;

private:
	uint32 ViewFlagIndex = 0;
};
#endif // UE_ENABLE_DEBUG_DRAWING

#undef UE_API

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "SmartObjectDebugRenderingComponent.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

class FDebugRenderSceneProxy;

/**
 * Simple UDebugDrawComponent to inherit from to use a FSmartObjectDebugSceneProxy.
 * Derived classes can set ViewFlagName at construction to control relevancy.
 */
UCLASS(MinimalAPI, ClassGroup = Debug, NotBlueprintable, NotPlaceable)
class USmartObjectDebugRenderingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()
public:
	UE_API explicit USmartObjectDebugRenderingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
#if UE_ENABLE_DEBUG_DRAWING
	UE_API virtual void OnRegister() override;
	UE_API virtual void OnUnregister() override;

	UE_API virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual void DebugDraw(FDebugRenderSceneProxy* DebugProxy) {}
	virtual void DebugDrawCanvas(UCanvas* Canvas, APlayerController*) {}

	FDelegateHandle CanvasDebugDrawDelegateHandle;
	FString ViewFlagName;
#endif // UE_ENABLE_DEBUG_DRAWING
};

#undef UE_API

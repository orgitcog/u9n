// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectDebugRenderingComponent.h"
#include "SmartObjectSubsystemRenderingActor.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

/** Rendering component for SmartObjectRendering actor. */
UCLASS(MinimalAPI, ClassGroup = Debug, NotBlueprintable, NotPlaceable)
class USmartObjectSubsystemRenderingComponent : public USmartObjectDebugRenderingComponent
{
	GENERATED_BODY()

public:
	explicit USmartObjectSubsystemRenderingComponent(const FObjectInitializer& ObjectInitializer)
		: USmartObjectDebugRenderingComponent(ObjectInitializer)
	{
#if UE_ENABLE_DEBUG_DRAWING
		ViewFlagName = TEXT("SmartObjects");
#endif
	}

protected:
	UE_API virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	UE_API virtual void DebugDraw(FDebugRenderSceneProxy* DebugProxy) override;
	UE_API virtual void DebugDrawCanvas(UCanvas* Canvas, APlayerController*) override;
#endif
};

UCLASS(MinimalAPI, Transient, NotBlueprintable, NotPlaceable)
class ASmartObjectSubsystemRenderingActor : public AActor
{
	GENERATED_BODY()

public:
	UE_API ASmartObjectSubsystemRenderingActor();

#if WITH_EDITOR
	virtual bool ShouldExport() override { return false; }
	virtual bool CanDeleteSelectedActor(FText& OutReason) const override { return false; }
#endif

private:
	UPROPERTY()
	TObjectPtr<USmartObjectSubsystemRenderingComponent> RenderingComponent;
};

#undef UE_API

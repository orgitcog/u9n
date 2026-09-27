// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"

#include "PathedPhysicsDebugDrawComponent.generated.h"

class UMeshComponent;

//@todo DanH: Ideally clicking anything drawn by this would select the mover comp in the editor. I think I need a whole FComponentVisualizer set up for that though?
UCLASS()
class UPathedPhysicsDebugDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UPathedPhysicsDebugDrawComponent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	TArray<FDebugRenderSceneProxy::FDebugLine> DebugLines;
	TArray<FDebugRenderSceneProxy::FDashedLine> DebugDashedLines;
	TArray<FDebugRenderSceneProxy::FArrowLine> DebugArrowLines;
	TArray<FDebugRenderSceneProxy::FWireStar> DebugStars;
	TArray<FDebugRenderSceneProxy::FSphere> DebugSpheres;
	
protected:
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;

	void UpdatePreviewMeshComp(bool bForce = false);
	void DestroyProgressPreviewMeshComp();
	UMeshComponent* GetOwnerMeshRoot() const;

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
#endif

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> ProgressPreviewMeshComp = nullptr;
};
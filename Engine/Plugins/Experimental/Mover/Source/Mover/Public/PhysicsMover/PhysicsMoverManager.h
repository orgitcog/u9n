// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsInterfaceDeclaresCore.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicsMoverManager.generated.h"

#define UE_API MOVER_API

//////////////////////////////////////////////////////////////////////////

class FChaosScene;
class UMoverNetworkPhysicsLiaisonComponentBase;

UCLASS(MinimalAPI)
class UPhysicsMoverManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UE_API virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	UE_API virtual void Deinitialize() override;
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UE_API void InjectInputs_External(int32 PhysicsStep, int32 NumSteps);
	UE_API void OnPostPhysicsTick(FChaosScene* Scene);

	UE_API void RegisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> InPhysicsMoverComp);
	UE_API void UnregisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> InPhysicsMoverComp);

private:
	TArray<TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase>> PhysicsMoverComponents;
	FDelegateHandle InjectInputsExternalCallbackHandle;
	FDelegateHandle PhysScenePostTickCallbackHandle;
	class FPhysicsMoverManagerAsyncCallback* AsyncCallback = nullptr;
};

#undef UE_API

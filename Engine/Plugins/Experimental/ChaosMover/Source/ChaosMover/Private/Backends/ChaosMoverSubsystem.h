// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Declares.h"
#include "Subsystems/WorldSubsystem.h"

#include "ChaosMoverSubsystem.generated.h"

namespace UE::ChaosMover
{
	class FAsyncCallback;
	class FSimulation;
}

class FChaosScene;
class UChaosMoverBackendComponent;

UCLASS()
class UChaosMoverSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(TWeakObjectPtr<UChaosMoverBackendComponent> Backend);
	void Unregister(TWeakObjectPtr<UChaosMoverBackendComponent> Backend);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	int32 GetNetworkPhysicsTickOffset() const;

	void InjectInputs_External(int32 PhysicsStep, int32 NumSteps);
	void OnPostPhysicsTick(FChaosScene* Scene);

	TArray<TWeakObjectPtr<UChaosMoverBackendComponent>> Backends;

	FDelegateHandle InjectInputsExternalCallbackHandle;
	FDelegateHandle PhysScenePostTickCallbackHandle;
	class UE::ChaosMover::FAsyncCallback* AsyncCallback = nullptr;
};
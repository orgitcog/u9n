// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsMover/PhysicsCharacterMoverComponent.h"

#include "MoverExamplesPhysicsCharacterMoverComponent.generated.h"


UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class MOVEREXAMPLES_API UMoverExamplesPhysicsCharacterMoverComponent : public UPhysicsCharacterMoverComponent
{
	GENERATED_BODY()

protected:

	virtual void OnMoverPreMovement(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
};

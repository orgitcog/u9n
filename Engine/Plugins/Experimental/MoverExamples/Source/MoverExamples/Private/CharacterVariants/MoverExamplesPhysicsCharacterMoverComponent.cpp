// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterVariants/MoverExamplesPhysicsCharacterMoverComponent.h"

#include "CharacterVariants/AbilityInputs.h"

void UMoverExamplesPhysicsCharacterMoverComponent::OnMoverPreMovement(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (FMoverExampleAbilityInputs* AbilityInputs = InputCmd.InputCollection.FindMutableDataByType<FMoverExampleAbilityInputs>())
	{
		if (AbilityInputs->bWantsToBeCrouched)
		{
			Crouch_Internal(SyncState);
		}
		else
		{
			UnCrouch_Internal(SyncState);
		}
	}

	Super::OnMoverPreMovement(TimeStep, InputCmd, SyncState, AuxState);
}
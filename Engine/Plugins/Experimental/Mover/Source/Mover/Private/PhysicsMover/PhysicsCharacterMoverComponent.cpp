// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsCharacterMoverComponent.h"

#include "PhysicsMover/Modes/PhysicsDrivenFallingMode.h"
#include "PhysicsMover/Modes/PhysicsDrivenFlyingMode.h"
#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"
#include "PhysicsMover/MovementModifiers/PhysicsStanceModifier.h"
#include "Backends/MoverNetworkPhysicsLiaison.h"

UPhysicsCharacterMoverComponent::UPhysicsCharacterMoverComponent()
{
	// Override with physics movement modes
	MovementModes.Add(DefaultModeNames::Walking, CreateDefaultSubobject<UPhysicsDrivenWalkingMode>(TEXT("PhysicsDrivenWalkingMode")));
	MovementModes.Add(DefaultModeNames::Falling, CreateDefaultSubobject<UPhysicsDrivenFallingMode>(TEXT("PhysicsDrivenFallingMode")));
	MovementModes.Add(DefaultModeNames::Flying,  CreateDefaultSubobject<UPhysicsDrivenFlyingMode>(TEXT("PhysicsDrivenFlyingMode")));

	BackendClass = UMoverNetworkPhysicsLiaisonComponent::StaticClass();

	bHandleJump = false;
}

void UPhysicsCharacterMoverComponent::BeginPlay()
{
	Super::BeginPlay();

	OnPreMovement.AddDynamic(this, &UPhysicsCharacterMoverComponent::OnMoverPreMovement);
	OnPostSimulationTick.AddDynamic(this, &UPhysicsCharacterMoverComponent::OnMoverPostSimulationTick);

	if (bHandleJump)
	{
		UE_LOG(LogMover, Warning, TEXT("Handle Jump flag is ignored for Physics Character Mover Component; jumps are handled via the Physics Jump Check Transition."));
	}
}

const FMovementModifierBase* UPhysicsCharacterMoverComponent::FindMovementModifier_Internal(const FMoverSyncState& SyncState, const FMovementModifierHandle& ModifierHandle) const
{
	Chaos::EnsureIsInPhysicsThreadContext();
	
	// Check active modifiers for modifier handle
	for (auto ActiveModifierFromSyncStateIt = SyncState.MovementModifiers.GetActiveModifiersIterator(); ActiveModifierFromSyncStateIt; ++ActiveModifierFromSyncStateIt)
	{
		const TSharedPtr<FMovementModifierBase> ActiveModifierFromSyncState = *ActiveModifierFromSyncStateIt;

		if (ModifierHandle == ActiveModifierFromSyncState->GetHandle())
		{
			return ActiveModifierFromSyncState.Get();
		}
	}

	// Check queued modifiers for modifier handle
	for (auto QueuedModifierFromSyncStateIt = SyncState.MovementModifiers.GetQueuedModifiersIterator(); QueuedModifierFromSyncStateIt; ++QueuedModifierFromSyncStateIt)
	{
		const TSharedPtr<FMovementModifierBase> QueuedModifierFromSyncState = *QueuedModifierFromSyncStateIt;

		if (ModifierHandle == QueuedModifierFromSyncState->GetHandle())
		{
			return QueuedModifierFromSyncState.Get();
		}
	}

	return nullptr;
}

const FMovementModifierBase* UPhysicsCharacterMoverComponent::FindMovementModifierByType_Internal(const FMoverSyncState& SyncState, const UScriptStruct* DataStructType) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	// Check active modifiers for modifier handle
	for (auto ActiveModifierFromSyncStateIt = SyncState.MovementModifiers.GetActiveModifiersIterator(); ActiveModifierFromSyncStateIt; ++ActiveModifierFromSyncStateIt)
	{
		const TSharedPtr<FMovementModifierBase> ActiveModifierFromSyncState = *ActiveModifierFromSyncStateIt;

		if (DataStructType == ActiveModifierFromSyncState->GetScriptStruct())
		{
			return ActiveModifierFromSyncState.Get();
		}
	}

	// Check queued modifiers for modifier handle
	for (auto QueuedModifierFromSyncStateIt = SyncState.MovementModifiers.GetQueuedModifiersIterator(); QueuedModifierFromSyncStateIt; ++QueuedModifierFromSyncStateIt)
	{
		const TSharedPtr<FMovementModifierBase> QueuedModifierFromSyncState = *QueuedModifierFromSyncStateIt;

		if (DataStructType == QueuedModifierFromSyncState->GetScriptStruct())
		{
			return QueuedModifierFromSyncState.Get();
		}
	}

	return nullptr;
}

bool UPhysicsCharacterMoverComponent::HasGameplayTag_Internal(const FMoverSyncState& SyncState, FGameplayTag TagToFind, bool bExactMatch) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (bExactMatch)
	{
		if (ExternalGameplayTags.HasTagExact(TagToFind))
		{
			return true;
		}
	}
	else
	{
		if (ExternalGameplayTags.HasTag(TagToFind))
		{
			return true;
		}
	}

	// Search Movement Modes
	if (const UBaseMovementMode* ActiveMovementMode = GetMovementMode())
	{
		if (ActiveMovementMode->HasGameplayTag(TagToFind, bExactMatch))
		{
			return true;
		}
	}

	// Search Movement Modifiers
	for (auto ModifierFromSyncStateIt = SyncState.MovementModifiers.GetActiveModifiersIterator(); ModifierFromSyncStateIt; ++ModifierFromSyncStateIt)
	{
		const TSharedPtr<FMovementModifierBase> ModifierFromSyncState = *ModifierFromSyncStateIt;
		if (ModifierFromSyncState->HasGameplayTag(TagToFind, bExactMatch))
		{
			return true;
		}
	}

	// Search Layered Moves
	for (const TSharedPtr<FLayeredMoveBase>& LayeredMove : SyncState.LayeredMoves.GetActiveMoves())
	{
		if (LayeredMove->HasGameplayTag(TagToFind, bExactMatch))
		{
			return true;
		}
	}

	return false;
}

void UPhysicsCharacterMoverComponent::Crouch_Internal(const FMoverSyncState& SyncState)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (CanCrouch_Internal(SyncState))
	{
		bWantsToCrouch = true;
	}
}

void UPhysicsCharacterMoverComponent::UnCrouch_Internal(const FMoverSyncState& SyncState)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	bWantsToCrouch = false;
}

void UPhysicsCharacterMoverComponent::OnMoverPreMovement(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	const FPhysicsStanceModifier* StanceModifier = static_cast<const FPhysicsStanceModifier*>(FindMovementModifier_Internal(SyncState, StanceModifierHandle));
	// This is a fail safe in case our handle was bad - try finding the modifier by type if we can
	if (!StanceModifier)
	{
		StanceModifier = FindMovementModifierByType_Internal<FPhysicsStanceModifier>(SyncState);
	}

	EStanceMode OldActiveStance = EStanceMode::Invalid;
	if (StanceModifier)
	{
		OldActiveStance = StanceModifier->ActiveStance;
	}

	const bool bIsCrouching = HasGameplayTag_Internal(SyncState, Mover_IsCrouching, true);
	if (bIsCrouching && (!bWantsToCrouch || !CanCrouch_Internal(SyncState)))
	{
		if (StanceModifier && StanceModifier->CanExpand_Internal(this, UpdatedComponent, SyncState))
		{
			CancelModifierFromHandle(StanceModifier->GetHandle());
			StanceModifierHandle.Invalidate();

			StanceModifier = nullptr;
		}
	}
	else if (!bIsCrouching && bWantsToCrouch && CanCrouch_Internal(SyncState))
	{
		TSharedPtr<FPhysicsStanceModifier> NewStanceModifier = MakeShared<FPhysicsStanceModifier>();
		StanceModifierHandle = QueueMovementModifier(NewStanceModifier);

		StanceModifier = NewStanceModifier.Get();
	}

	// Ensure that StanceModifierHandle is consistent with the crouch state 
	if(!bIsCrouching && !bWantsToCrouch && StanceModifierHandle.IsValid())
	{
		StanceModifierHandle.Invalidate();
	}
}

void UPhysicsCharacterMoverComponent::OnMoverPostSimulationTick(const FMoverTimeStep& TimeStep)
{
	const FPhysicsStanceModifier* StanceModifier = static_cast<const FPhysicsStanceModifier*>(FindMovementModifier(StanceModifierHandle));
	if (!StanceModifier)
	{
		StanceModifier = FindMovementModifierByType<FPhysicsStanceModifier>();
	}

	const bool bIsCrouching = HasGameplayTag(Mover_IsCrouching, true);
	bool bStanceChanged = false;

	FPhysicsStanceModifier::OnPostSimulationTick(StanceModifier, this, UpdatedCompAsPrimitive, bIsCrouching, bStancePostProcessed, bStanceChanged);

	if (bStanceChanged && bIsCrouching)
	{
		OnStanceChanged.Broadcast(EStanceMode::Invalid, EStanceMode::Crouch);
	}
	else if (bStanceChanged && !bIsCrouching)
	{
		OnStanceChanged.Broadcast(EStanceMode::Crouch, EStanceMode::Invalid);
	}
}
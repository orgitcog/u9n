// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/Character/ChaosCharacterMoverComponent.h"

#include "ChaosMover/Backends/ChaosMoverBackend.h"
#include "ChaosMover/Character/ChaosCharacterInputs.h"
#include "ChaosMover/Character/Modes/ChaosFallingMode.h"
#include "ChaosMover/Character/Modes/ChaosFlyingMode.h"
#include "ChaosMover/Character/Modes/ChaosWalkingMode.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "DefaultMovementSet/CharacterMoverSimulationTypes.h"

UChaosCharacterMoverComponent::UChaosCharacterMoverComponent()
{
	// Default movement modes
	MovementModes.Add(DefaultModeNames::Walking, CreateDefaultSubobject<UChaosWalkingMode>(TEXT("DefaultChaosWalkingMode")));
	MovementModes.Add(DefaultModeNames::Falling, CreateDefaultSubobject<UChaosFallingMode>(TEXT("DefaultChaosFallingMode")));
	MovementModes.Add(DefaultModeNames::Flying, CreateDefaultSubobject<UChaosFlyingMode>(TEXT("DefaultChaosFlyingMode")));

	StartingMovementMode = DefaultModeNames::Falling;

	bHandleJump = false;
	bHandleStanceChanges = false;

	BackendClass = UChaosMoverBackendComponent::StaticClass();
}

void UChaosCharacterMoverComponent::ProcessSimulationEvent(const FMoverSimulationEventData& EventData)
{
	Super::ProcessSimulationEvent(EventData);

	if (const FLandedEventData* LandedData = EventData.CastTo<FLandedEventData>())
	{
		OnLanded.Broadcast(LandedData->NewModeName, LandedData->HitResult);
	}
}

void UChaosCharacterMoverComponent::SetAdditionalSimulationOutput(const FMoverDataCollection& Data)
{
	Super::SetAdditionalSimulationOutput(Data);

	if (const FFloorResultData* FloorData = Data.FindDataByType<FFloorResultData>())
	{
		bFloorResultSet = true;
		LatestFloorResult = FloorData->FloorResult;
	}
}

bool UChaosCharacterMoverComponent::TryGetFloorCheckHitResult(FHitResult& OutHitResult) const
{
	if (bFloorResultSet)
	{
		OutHitResult = LatestFloorResult.HitResult;
		return true;
	}
	else
	{
		return Super::TryGetFloorCheckHitResult(OutHitResult);
	}
}

void UChaosCharacterMoverComponent::ProduceInput(const int32 DeltaTimeMS, FMoverInputCmdContext* Cmd)
{
	Super::ProduceInput(DeltaTimeMS, Cmd);

	// Add launch input data if launch velocity is set
	if (!LaunchVelocityOrImpulse.IsZero())
	{
		check(Cmd);

		FChaosMoverLaunchInputs& LaunchInputs = Cmd->InputCollection.FindOrAddMutableDataByType<FChaosMoverLaunchInputs>();
		LaunchInputs.LaunchVelocityOrImpulse = LaunchVelocityOrImpulse;
		LaunchInputs.Mode = LaunchMode;

		LaunchVelocityOrImpulse = FVector::ZeroVector;
	}
}

void UChaosCharacterMoverComponent::Launch(const FVector& VelocityOrImpulse, EChaosMoverVelocityEffectMode Mode)
{
	LaunchVelocityOrImpulse = VelocityOrImpulse;
	LaunchMode = Mode;
}
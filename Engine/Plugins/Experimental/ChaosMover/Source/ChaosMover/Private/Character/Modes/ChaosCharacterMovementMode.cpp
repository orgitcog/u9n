// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/Character/Modes/ChaosCharacterMovementMode.h"

#include "Chaos/Character/CharacterGroundConstraintSettings.h"
#include "ChaosMover/Character/Settings/SharedChaosCharacterMovementSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Math/UnitConversion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterMovementMode)

UChaosCharacterMovementMode::UChaosCharacterMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(USharedChaosCharacterMovementSettings::StaticClass());
}

void UChaosCharacterMovementMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	if (TargetHeightOverride.IsSet())
	{
		TargetHeight = TargetHeightOverride.GetValue();
	}
	else if (const ACharacter* CharacterOwner = GetTypedOuter<ACharacter>())
	{
		TargetHeight = -CharacterOwner->GetMesh()->GetRelativeLocation().Z;
	}

	SharedSettings = GetMoverComponent()->FindSharedSettings<USharedChaosCharacterMovementSettings>();
	ensureMsgf(SharedSettings, TEXT("Failed to find instance of USharedChaosCharacterMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void UChaosCharacterMovementMode::OnUnregistered()
{
	SharedSettings = nullptr;

	Super::OnUnregistered();
}

void UChaosCharacterMovementMode::SetTargetHeightOverride(float InTargetHeight)
{
	TargetHeightOverride = InTargetHeight;
	TargetHeight = InTargetHeight;
}

void UChaosCharacterMovementMode::ClearTargetHeightOverride()
{
	TargetHeightOverride.Reset();

	if (const ACharacter* CharacterOwner = GetTypedOuter<ACharacter>())
	{
		TargetHeight = -CharacterOwner->GetMesh()->GetRelativeLocation().Z;
	}
	else
	{
		TargetHeight = GetDefault<UChaosCharacterMovementMode>(GetClass())->TargetHeight;
	}
}

void UChaosCharacterMovementMode::SetQueryRadiusOverride(float InQueryRadius)
{
	QueryRadiusOverride = InQueryRadius;
	QueryRadius = InQueryRadius;
}

void UChaosCharacterMovementMode::ClearQueryRadiusOverride()
{
	QueryRadiusOverride.Reset();

	if (const ACharacter* CharacterOwner = GetTypedOuter<ACharacter>())
	{
		if (UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent())
		{
			QueryRadius = FMath::Max(CapsuleComp->GetScaledCapsuleRadius() - 5.0f, 0.0f);
			return;
		}
	}
	
	QueryRadius = GetDefault<UChaosCharacterMovementMode>(GetClass())->QueryRadius;
}

void UChaosCharacterMovementMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const
{
	ConstraintSettings.RadialForceLimit = FUnitConversion::Convert(RadialForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared);
	ConstraintSettings.TwistTorqueLimit = FUnitConversion::Convert(TwistTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared);
	ConstraintSettings.SwingTorqueLimit = FUnitConversion::Convert(SwingTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared);
	ConstraintSettings.TargetHeight = TargetHeight;
}

float UChaosCharacterMovementMode::GetMaxWalkSlopeCosine() const
{
	if (const USharedChaosCharacterMovementSettings* SharedSettingsPtr = SharedSettings.Get())
	{
		return SharedSettingsPtr->GetMaxWalkableSlopeCosine();
	}

	return 0.707f;
}

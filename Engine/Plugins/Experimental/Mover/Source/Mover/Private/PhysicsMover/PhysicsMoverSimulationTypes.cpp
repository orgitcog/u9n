// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DataValidation.h"

//////////////////////////////////////////////////////////////////////////
// Debug

FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

FAutoConsoleVariableRef CVarPhysicsDrivenMotionEnableMultithreading(TEXT("p.mover.physics.EnableMultithreading"),
	GPhysicsDrivenMotionDebugParams.EnableMultithreading, TEXT("Enable multi-threading of physics driven motion updates."));

FAutoConsoleVariableRef CVarPhysicsDrivenMotionDebugDrawFloorTest(TEXT("p.mover.physics.DebugDrawFloorQueries"),
	GPhysicsDrivenMotionDebugParams.DebugDrawGroundQueries, TEXT("Debug draw floor test queries."));

FAutoConsoleVariableRef CVarPhysicsDrivenMotionTeleportThreshold(TEXT("p.mover.physics.TeleportThreshold"),
	GPhysicsDrivenMotionDebugParams.TeleportThreshold, TEXT("Single frame movement threshold in cm that will trigger a teleport."));

FAutoConsoleVariableRef CVarPhysicsDrivenMotionMinStepUpDistance(TEXT("p.mover.physics.MinStepUpDistance"),
	GPhysicsDrivenMotionDebugParams.MinStepUpDistance, TEXT("Minimum distance that will be considered a step up."));

FAutoConsoleVariableRef CVarPhysicsDrivenMotionMaxCharacterGroundMassRatio(TEXT("p.mover.physics.MaxCharacterGroundMassRatio"),
	GPhysicsDrivenMotionDebugParams.MaxCharacterGroundMassRatio, TEXT("Maximum ratio between character mass and ground mass as seen by the ground constraint."));

//////////////////////////////////////////////////////////////////////////
// PhysicsMovementModeUtils

namespace PhysicsMovementModeUtils
{
	void ValidateBackendClass(UMoverComponent* MoverComponent, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		if (MoverComponent)
		{
			const UClass* BackendClass = MoverComponent->BackendClass;
			if (BackendClass && !BackendClass->IsChildOf<UMoverNetworkPhysicsLiaisonComponent>())
			{
				Context.AddError(NSLOCTEXT("PhysicsMovementModeUtils", "PhysicsMovementModeHasValidPhysicsLiaison", "Physics movement modes need to have a backend class that supports physics (UMoverNetworkPhysicsLiaisonComponent)."));
				Result = EDataValidationResult::Invalid;
			}
		}
	}	
}

//////////////////////////////////////////////////////////////////////////
// FMovementSettingsInputs

FMoverDataStructBase* FMovementSettingsInputs::Clone() const
{
	FMovementSettingsInputs* CopyPtr = new FMovementSettingsInputs(*this);
	return CopyPtr;
}

bool FMovementSettingsInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << MaxSpeed;
	Ar << Acceleration;

	bOutSuccess = true;
	return true;
}


void FMovementSettingsInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("MaxSpeed=%.2f | ", MaxSpeed);
	Out.Appendf("Acceleration=%.2f", Acceleration);
}

bool FMovementSettingsInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FMovementSettingsInputs& TypedAuthority = static_cast<const FMovementSettingsInputs&>(AuthorityState);
	return !FMath::IsNearlyEqual(Acceleration, TypedAuthority.Acceleration) || !FMath::IsNearlyEqual(MaxSpeed, TypedAuthority.MaxSpeed);
}

void FMovementSettingsInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FMovementSettingsInputs& TypedFrom = static_cast<const FMovementSettingsInputs&>(From);
	const FMovementSettingsInputs& TypedTo = static_cast<const FMovementSettingsInputs&>(To);

	MaxSpeed = FMath::Lerp(TypedFrom.MaxSpeed, TypedTo.MaxSpeed, Pct);;
	Acceleration = FMath::Lerp(TypedFrom.Acceleration, TypedTo.Acceleration, Pct);

}

void FMovementSettingsInputs::Merge(const FMoverDataStructBase& From)
{
}

static float MovementSettingsInputsDecayAmountMultiplier = 0.25f;
FAutoConsoleVariableRef CVarMovementSettingsInputsDecayAmountMultiplier(
	TEXT("Mover.Input.MovementSettingsInputsDecayAmountMultiplier"),
	MovementSettingsInputsDecayAmountMultiplier,
	TEXT("Multiplier to use when decaying MovementSettingsInputs."));

void FMovementSettingsInputs::Decay(float DecayAmount)
{
	DecayAmount *= MovementSettingsInputsDecayAmountMultiplier;

	MaxSpeed *= (1.0f - DecayAmount);
	Acceleration *= (1.0f - DecayAmount);
}


//////////////////////////////////////////////////////////////////////////
// FMoverAIInputs

FMoverDataStructBase* FMoverAIInputs::Clone() const
{
	FMoverAIInputs* CopyPtr = new FMoverAIInputs(*this);
	return CopyPtr;
}

bool FMoverAIInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << RVOVelocityDelta;

	bOutSuccess = true;
	return true;
}

void FMoverAIInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("ROVVelDelta: X=%.2f Y=%.2f Z=%.2f\n", RVOVelocityDelta.X, RVOVelocityDelta.Y, RVOVelocityDelta.Z);
}

bool FMoverAIInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FMoverAIInputs& TypedAuthority = static_cast<const FMoverAIInputs&>(AuthorityState);
	return TypedAuthority.RVOVelocityDelta != RVOVelocityDelta;
}

void FMoverAIInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FMoverAIInputs& TypedFrom = static_cast<const FMoverAIInputs&>(From);
	const FMoverAIInputs& TypedTo = static_cast<const FMoverAIInputs&>(To);

	RVOVelocityDelta = FMath::Lerp(TypedFrom.RVOVelocityDelta, TypedTo.RVOVelocityDelta, Pct);
}

void FMoverAIInputs::Merge(const FMoverDataStructBase& From)
{
}

//////////////////////////////////////////////////////////////////////////
// FMoverLaunchInputs

FMoverDataStructBase* FMoverLaunchInputs::Clone() const
{
	FMoverLaunchInputs* CopyPtr = new FMoverLaunchInputs(*this);
	return CopyPtr;
}

bool FMoverLaunchInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << LaunchVelocity;
	Ar << Mode;

	bOutSuccess = true;
	return true;
}

void FMoverLaunchInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("LaunchVelocity: X=%.2f Y=%.2f Z=%.2f\n", LaunchVelocity.X, LaunchVelocity.Y, LaunchVelocity.Z);
	Out.Appendf("Mode: %u\n", Mode);
}

bool FMoverLaunchInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FMoverLaunchInputs& TypedAuthority = static_cast<const FMoverLaunchInputs&>(AuthorityState);
	return Mode != TypedAuthority.Mode || LaunchVelocity != TypedAuthority.LaunchVelocity;
}

void FMoverLaunchInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	if (Pct < .5f)
	{
		*this = static_cast<const FMoverLaunchInputs&>(From);
	}
	else
	{
		*this = static_cast<const FMoverLaunchInputs&>(To);
	}
}

void FMoverLaunchInputs::Merge(const FMoverDataStructBase& From)
{
	
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/GeometryParticlesfwd.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "UObject/Interface.h"
#include "UObject/WeakInterfacePtr.h"
#include "PhysicsMoverSimulationTypes.generated.h"

#define UE_API MOVER_API

//////////////////////////////////////////////////////////////////////////

namespace Chaos
{
	class FCharacterGroundConstraint;
	class FCharacterGroundConstraintHandle;
	class FCollisionContactModifier;
}

class UPrimitiveComponent;

//////////////////////////////////////////////////////////////////////////
// Debug

struct FPhysicsDrivenMotionDebugParams
{
	float TeleportThreshold = 1000.0f;
	float MinStepUpDistance = 5.0f;
	float MaxCharacterGroundMassRatio = 1.5f;
	bool EnableMultithreading = false;
	bool DebugDrawGroundQueries = false;
};

#ifndef PHYSICSDRIVENMOTION_DEBUG_DRAW
#define PHYSICSDRIVENMOTION_DEBUG_DRAW (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)
#endif

//////////////////////////////////////////////////////////////////////////
// Async update

struct FPhysicsMoverSimulationTickParams
{
	float SimTimeSeconds = 0.0f;
	float DeltaTimeSeconds = 0.0f;
};

struct FPhysicsMoverAsyncInput
{
	bool IsValid() const
	{
		return MoverSimulation.IsValid() && MoverIdx.IsValid();
	}

	// Input is modified during ProcessInputs_Internal
	mutable FMoverInputCmdContext InputCmd;
	mutable FMoverSyncState SyncState;

	TWeakObjectPtr<class UMoverNetworkPhysicsLiaisonComponentBase> MoverSimulation;
	Chaos::FUniqueIdx MoverIdx;
};

struct FPhysicsMoverAsyncOutput
{
	FMoverSyncState SyncState;
	FMoverInputCmdContext InputCmd;
	FFloorCheckResult FloorResult;
	bool bIsValid = false;
};

//////////////////////////////////////////////////////////////////////////
// Movement modes

struct FPhysicsMoverSimulationContactModifierParams
{
	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle;
	UPrimitiveComponent* UpdatedPrimitive;
};

/**
 * UPhysicsCharacterMovementModeInterface: Interface for movement modes that are for physics driven motion
 * A physics driven motion mode needs to update the character ground constraint with the
 * parameters associated with that mode
 */
UINTERFACE(MinimalAPI)
class UPhysicsCharacterMovementModeInterface : public UInterface
{
	GENERATED_BODY()
};

class IPhysicsCharacterMovementModeInterface
{
	GENERATED_BODY()

public:
	// Update the constraint settings on the game thread
	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const = 0;

	// Optionally run contact modification on the physics thread
	virtual void OnContactModification_Internal(const FPhysicsMoverSimulationContactModifierParams& Params, Chaos::FCollisionContactModifier& Modifier) const {};

	// Optional TargetHeight getter
	virtual float GetTargetHeight() const { return -1.f; }

	// Optional TargetHeightOverride methods
	virtual void SetTargetHeightOverride(float InTargetHeight) {}
	virtual void ClearTargetHeightOverride() {}
};

class FDataValidationContext;
namespace PhysicsMovementModeUtils
{
	void ValidateBackendClass(UMoverComponent* MoverComponent, FDataValidationContext& Context, EDataValidationResult& Result);
}

//////////////////////////////////////////////////////////////////////////
// FMovementSettingsInputs

// Data block containing movement settings inputs that are networked from client to server.
// This is useful if settings changes need to be predicted on the client and synced on the server.
// Also supports rewind/resimulation of settings changes.
USTRUCT(BlueprintType)
struct FMovementSettingsInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

public:
	// Maximum speed in cm/s
	UPROPERTY(BlueprintReadWrite, Category = Mover)
		float MaxSpeed;

	// Maximum acceleration in cm/s^2
	UPROPERTY(BlueprintReadWrite, Category = Mover)
		float Acceleration;

	FMovementSettingsInputs()
		: MaxSpeed(800.0f)
		, Acceleration(4000.0f)
	{
	}

	virtual ~FMovementSettingsInputs() {}

	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void Merge(const FMoverDataStructBase& From) override;
	UE_API virtual void Decay(float DecayAmount) override;
};

template<>
struct TStructOpsTypeTraits< FMovementSettingsInputs > : public TStructOpsTypeTraitsBase2< FMovementSettingsInputs >
{
	enum
	{
		WithCopy = true
	};
};


//////////////////////////////////////////////////////////////////////////
// FMoverAIInputs

// Data block containing ROV Velocity that is networked from server to clients.
// Also supports rewind/resimulation of the data.
USTRUCT(BlueprintType)
struct FMoverAIInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

public:
	// ROV Velocity calculated on the Server
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FVector RVOVelocityDelta;

	FMoverAIInputs()
		: RVOVelocityDelta(ForceInitToZero)
	{
	}

	virtual ~FMoverAIInputs() {}

	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void Merge(const FMoverDataStructBase& From) override;
};

template<>
struct TStructOpsTypeTraits< FMoverAIInputs > : public TStructOpsTypeTraitsBase2< FMoverAIInputs >
{
	enum
	{
		WithCopy = true
	};
};

UENUM(BlueprintType)
enum class EMoverLaunchVelocityMode : uint8
{
	Additive,
	Override
};

USTRUCT(BlueprintType)
struct FMoverLaunchInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

public:
	// Launch velocity in cm/s
	UPROPERTY(BlueprintReadWrite, Category = Mover, meta = (Units = "cm/s"))
	FVector LaunchVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	EMoverLaunchVelocityMode Mode = EMoverLaunchVelocityMode::Additive;

	UE_API virtual FMoverDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	UE_API virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	UE_API virtual void Merge(const FMoverDataStructBase& From) override;
};

template<>
struct TStructOpsTypeTraits< FMoverLaunchInputs > : public TStructOpsTypeTraitsBase2< FMoverLaunchInputs >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

#undef UE_API

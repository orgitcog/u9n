// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Declares.h"
#include "CollisionQueryParams.h"
#include "MoverSimulationTypes.h"

#include "ChaosMoverSimulationTypes.generated.h"

namespace Chaos
{
	class FPBDJointSettings;
	class FCharacterGroundConstraintSettings;
}

struct FConstraintProfileProperties;
class UChaosMoverSimulation;
class UBaseMovementMode;

namespace UE::ChaosMover
{
	struct FSimulationInputData
	{
		void Reset()
		{
			InputCmd.Reset();
			AuxInputState.AuxStateCollection.Empty();
		}

		mutable FMoverInputCmdContext InputCmd; // Can be overridden by network physics
		FMoverAuxStateContext AuxInputState; // Optional aux input state. Not networked
	};

	using FSimulationOutputData = ::UE::Mover::FSimulationOutputData;

	/** Mode change event structure, used to postpone callbacks to gameplay code when a mode has changed */
	struct FMovementModeChangeEvent
	{
		FName PreviousMovementModeName;
		TWeakObjectPtr<UBaseMovementMode> PreviousMovementMode;
		FName NextMovementModeName;
		TWeakObjectPtr<UBaseMovementMode> NextMovementMode;
	};

	// Util function to be able to get the debug sim data collection from a UChaosMoverSimulation from another plugin,
	// without including ChaosMoverSimulation.h
	CHAOSMOVER_API FMoverDataCollection& GetDebugSimData(UChaosMoverSimulation* Simulation);

} // namespace UE::ChaosMover

// Default chaos mover character simulation state, contains state basic for character simulation
USTRUCT(BlueprintType)
struct CHAOSMOVER_API FChaosMoverCharacterSimState : public FMoverDataStructBase
{
	GENERATED_BODY()

	/** return newly allocated copy of this FMoverDataStructBase. MUST be overridden by derived types. */
	virtual FMoverDataStructBase* Clone() const override
	{
		return new FChaosMoverCharacterSimState(*this);
	}

	/** Gets the type info of this FMoverDataStructBase. MUST be overridden by derived types. */
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=ChaosMover)
	FVector TargetDeltaPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=ChaosMover)
	float TargetDeltaFacing = 0.0f;
};

// Data block containing all default inputs required by the Chaos Mover simulation
USTRUCT(BlueprintType)
struct CHAOSMOVER_API FChaosMoverSimulationDefaultInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	FChaosMoverSimulationDefaultInputs()
	{
		Reset();
	}

	void Reset();

	/** return newly allocated copy of this FMoverDataStructBase. MUST be overridden by derived types. */
	virtual FMoverDataStructBase* Clone() const override
	{
		return new FChaosMoverSimulationDefaultInputs(*this);
	}

	/** Gets the type info of this FMoverDataStructBase. MUST be overridden by derived types. */
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	FCollisionResponseParams CollisionResponseParams;
	FCollisionQueryParams CollisionQueryParams;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=ChaosMover)
	FVector UpDir;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=ChaosMover)
	FVector Gravity;
	Chaos::FPhysicsObjectHandle PhysicsObject;
	AActor* OwningActor;
	UWorld* World;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=ChaosMover)
	float PhysicsObjectGravity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=ChaosMover)
	float PawnCollisionHalfHeight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=ChaosMover)
	float PawnCollisionRadius;
	ECollisionChannel CollisionChannel;
};

UINTERFACE(BlueprintType, MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UChaosCharacterMovementModeInterface : public UInterface
{
	GENERATED_BODY()
};

class IChaosCharacterMovementModeInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category=ChaosMover)
	virtual float GetTargetHeight() const = 0;

	UFUNCTION(BlueprintCallable, Category = ChaosMover)
	virtual float GetGroundQueryRadius() const = 0;

	UFUNCTION(BlueprintCallable, Category = ChaosMover)
	virtual float GetMaxWalkSlopeCosine() const = 0;

	UFUNCTION(BlueprintCallable, Category = ChaosMover)
	virtual bool ShouldCharacterRemainUpright() const = 0;
};

// Interface for mover modes moving on ground like characters, using a character ground constraint
UINTERFACE(MinimalAPI)
class UChaosCharacterConstraintMovementModeInterface : public UInterface
{
	GENERATED_BODY()
};

class IChaosCharacterConstraintMovementModeInterface
{
	GENERATED_BODY()

public:
	virtual bool ShouldEnableConstraint() const = 0;
	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const = 0;
};

// Interface for mover modes moving on a path, with a constraint or kinematically
UINTERFACE(MinimalAPI)
class UChaosPathedMovementModeInterface : public UInterface
{
	GENERATED_BODY()
};

class IChaosPathedMovementModeInterface
{
	GENERATED_BODY()

public:

	// Whether the simulation should use a constraint to move the actor along the path, or moving the actor kinematically
	virtual bool ShouldUseConstraint() const = 0;

	// Constraint settings of the joint constraint, only valid when ShouldUseConstraint() is true
	virtual const Chaos::FPBDJointSettings& GetConstraintSettings() const = 0;
};

UENUM(BlueprintType)
enum class EChaosMoverVelocityEffectMode : uint8
{
	/** Apply as an additive impulse*/
	Impulse,

	/** Apply as an additive velocity */
	AdditiveVelocity,

	/** Apply as an override velocity */
	OverrideVelocity,
};

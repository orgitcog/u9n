// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosMover/Character/Modes/ChaosCharacterMovementMode.h"

#include "ChaosFallingMode.generated.h"

/**
 * Chaos character falling mode
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class CHAOSMOVER_API UChaosFallingMode : public UChaosCharacterMovementMode
{
	GENERATED_BODY()

public:
	UChaosFallingMode(const FObjectInitializer& ObjectInitializer);

	virtual void GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

protected:
	/**
	 * If true, actor will land and lose all speed in the vertical direction upon landing. If false, actor's vertical speed will be redirected based on the surface normal it hit.
	 * Note: Actor's horizontal speed will not be affected if true. If false, horizontal speed may be increased on landing.
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite)
	bool bCancelVerticalSpeedOnLanding;

	/**
	 * When falling, amount of movement control available to the actor.
	 * 0 = no control, 1 = full control
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1.0"))
	float AirControlPercentage;

	/**
	 * Deceleration to apply to air movement when falling slower than terminal velocity.
	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float FallingDeceleration;

	/**
	 * Deceleration to apply to air movement when falling faster than terminal velocity
	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float OverTerminalSpeedFallingDeceleration;

	/**
	 * If the actor's movement plane velocity is greater than this speed falling will start applying OverTerminalSpeedFallingDeceleration instead of FallingDeceleration
	 * The expected behavior is to set OverTerminalSpeedFallingDeceleration higher than FallingDeceleration so the actor will slow down faster
	 * when going over TerminalMovementPlaneSpeed.
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalMovementPlaneSpeed;

	/** When exceeding maximum vertical speed, should it be enforced via a hard clamp? If false, VerticalFallingDeceleration will be used for a smoother transition to the terminal speed limit. */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite)
	bool bShouldClampTerminalVerticalSpeed;

	/** Deceleration to apply to vertical velocity when it's greater than TerminalVerticalSpeed. Only used if bShouldClampTerminalVerticalSpeed is false. */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "!bShouldClampTerminalVerticalSpeed", ClampMin = "0", ForceUnits = "cm/s^2"))
	float VerticalFallingDeceleration;

	/**
	 * If the actors vertical velocity is greater than this speed VerticalFallingDeceleration will be applied to vertical velocity
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalVerticalSpeed;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "Chaos/Core.h"

#include "PathedMovementTypes.generated.h"

namespace PathBlackboard
{
	const FName TargetRelativeTransform = TEXT("TargetRelativeTransform");
}

UENUM()
enum class EPathedPhysicsPlaybackBehavior : uint8
{
	/** Progresses from 0 -> 1 and then stops */
	OneShot,
	/** Progresses from 0 -> 1 -> 0 and then stops (basically a one-shot ping-pong) */
	ThereAndBack,
	/** Progresses from 0 -> 1, then resetting to 0, repeating infinitely */
	Looping,
	/** Progresses from 0 -> 1 -> 0 -> 1 (and so on) infinitely */
	PingPong
};

/**
 * Properties that can change during game-time and affect path movement calculations.
 * Split into its own struct because both the input and sync state structs want to hold onto these. 
 */
USTRUCT()
struct FMutablePathedMovementProperties
{
	GENERATED_BODY()

	/**
	 * The server frame of the simulation when movement should/did actually begin
	 * Tracked this way so that we can set a future frame as the actual movement start frame to account for network latency, allowing clients and server to start at the same time irl.
	 * A value < 0 indicates that no movement is happening along the path.
	 */
	UPROPERTY()
	int32 MovementStartFrame = INDEX_NONE;

	UPROPERTY()
	bool bIsInReverse = false;

	UPROPERTY()
	bool bIsJointEnabled = false;

	UPROPERTY()
	EPathedPhysicsPlaybackBehavior PlaybackBehavior = EPathedPhysicsPlaybackBehavior::OneShot;

	UPROPERTY()
	FTransform PathOrigin;

	bool IsMoving() const;
	bool IsLooping() const;
	bool IsPingPonging() const;
	
	void NetSerialize(FArchive& Ar);
	void ToString(FAnsiStringBuilderBase& Out) const;
	
	bool operator==(const FMutablePathedMovementProperties& Other) const;
	bool operator!=(const FMutablePathedMovementProperties& Other) const { return !operator==(Other); }
};

/**
 * Frame inputs for all pathed movement
 * Any property that can change during game time must be delivered in this way, only immutable properties can be safely
 * referenced directly on the object they come from (usually the movement mode)
 */
USTRUCT()
struct FPathedPhysicsMovementInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY()
	FMutablePathedMovementProperties Props;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual FMoverDataStructBase* Clone() const override { return new FPathedPhysicsMovementInputs(*this); }
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
	virtual void Merge(const FMoverDataStructBase& From) override;
};

/** State about the pathed movement that persists from frame to frame */
USTRUCT()
struct FPathedPhysicsMovementState : public FMoverDataStructBase
{
	GENERATED_BODY()
	
	/** The (bounded) playback time along the path where we were the last time movement stopped for any reason */
	UPROPERTY()
	float LastStopPlaybackTime = 0.f;

	UPROPERTY()
	float CurrentProgress = 0.f;
	
	/** The most recent mutable properties received from the input */
	UPROPERTY()
	FMutablePathedMovementProperties MutableProps;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual FMoverDataStructBase* Clone() const override { return new FPathedPhysicsMovementState(*this); }
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};
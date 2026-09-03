// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/StringBuilder.h"
#include "NetworkPredictionReplicationProxy.h"
#include "Engine/NetSerialization.h"
#include "MoverTypes.h"
#include "MoveLibrary/MovementRecord.h"
#include "LayeredMove.h"
#include "MovementModifier.h"
#include "MoverDataModelTypes.h"
#include "UObject/Interface.h"
#include "MoverSimulationTypes.generated.h"

// Names for our default modes
namespace DefaultModeNames
{
	const FName Walking = TEXT("Walking");
	const FName Falling = TEXT("Falling");
	const FName Flying  = TEXT("Flying");
	const FName Swimming  = TEXT("Swimming");
}

// Commonly-used blackboard object keys
namespace CommonBlackboard
{
	const FName LastFloorResult = TEXT("LastFloor");
	const FName LastWaterResult = TEXT("LastWater");
	const FName LastFoundDynamicMovementBase = TEXT("LastFoundDynamicMovementBase");
	const FName LastAppliedDynamicMovementBase = TEXT("LastAppliedDynamicMovementBase");
	const FName TimeSinceSupported = TEXT("TimeSinceSupported");
}


/**
 * Filled out by a MovementMode during simulation tick to indicate its ending state, allowing for a residual time step and switching modes mid-tick
 */
USTRUCT(BlueprintType)
struct FMovementModeTickEndState
{
	GENERATED_BODY()
	
	FMovementModeTickEndState() 
	{ 
		ResetToDefaults(); 
	}

	void ResetToDefaults()
	{
		RemainingMs = 0.f;
		NextModeName = NAME_None;
	}

	// Any unused tick time
	UPROPERTY(BlueprintReadWrite, Category=Mover)
	float RemainingMs;

	UPROPERTY(BlueprintReadWrite, Category=Mover)
	FName NextModeName = NAME_None;
};




/**
 * The client generates this representation of "input" to the simulated actor for one simulation frame. This can be direct mapping
 * of controls, or more abstract data. It is composed of a collection of typed structs that can be customized per project.
 */
USTRUCT(BlueprintType)
struct FMoverInputCmdContext
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FMoverDataCollection InputCollection;

	UScriptStruct* GetStruct() const { return StaticStruct(); }

	void NetSerialize(const FNetSerializeParams& P)
	{
		bool bIgnoredResult(false);
		InputCollection.NetSerialize(P.Ar, nullptr, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		InputCollection.ToString(Out);
	}

	void Interpolate(const FMoverInputCmdContext* From, const FMoverInputCmdContext* To, float Pct)
	{
		InputCollection.Interpolate(From->InputCollection, To->InputCollection, Pct);
	}

	void Reset()
	{
		InputCollection.Empty();
	}
};


/** State we are evolving frame to frame and keeping in sync (frequently changing). It is composed of a collection of typed structs 
 *  that can be customized per project. Mover actors are required to have FMoverDefaultSyncState as one of these structs.
 */
USTRUCT(BlueprintType)
struct FMoverSyncState
{
	GENERATED_BODY()

public:

	// The mode we ended up in from the prior frame, and which we'll start in during the next frame
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Mover)
	FName MovementMode;

	// Additional moves influencing our proposed motion
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Mover)
	FLayeredMoveGroup LayeredMoves;

	// Additional modifiers influencing our simulation
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Mover)
	FMovementModifierGroup MovementModifiers;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FMoverDataCollection SyncStateCollection;

	FMoverSyncState()
	{
		MovementMode = NAME_None;
	}

	UScriptStruct* GetStruct() const { return StaticStruct(); }

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << MovementMode;
		LayeredMoves.NetSerialize(P.Ar);
		MovementModifiers.NetSerialize(P.Ar);

		bool bIgnoredResult(false);
		SyncStateCollection.NetSerialize(P.Ar, nullptr, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("MovementMode: %s\n", TCHAR_TO_ANSI(*MovementMode.ToString()));
		Out.Appendf("Layered Moves: %s\n", TCHAR_TO_ANSI(*LayeredMoves.ToSimpleString()));
		Out.Appendf("Movement Modifiers: %s\n", TCHAR_TO_ANSI(*MovementModifiers.ToSimpleString()));
		SyncStateCollection.ToString(Out);
	}

	bool ShouldReconcile(const FMoverSyncState& AuthorityState) const
	{
		return (MovementMode != AuthorityState.MovementMode) || 
		       SyncStateCollection.ShouldReconcile(AuthorityState.SyncStateCollection);
	}

	void Interpolate(const FMoverSyncState* From, const FMoverSyncState* To, float Pct)
	{
		MovementMode = To->MovementMode;
		LayeredMoves = To->LayeredMoves;
		MovementModifiers = To->MovementModifiers;

		SyncStateCollection.Interpolate(From->SyncStateCollection, To->SyncStateCollection, Pct);
	}

	// Resets the sync state to its default configuration and removes any
	// active or queued layered modes and modifiers
	void Reset()
	{
		MovementMode = NAME_None;
		SyncStateCollection.Empty();
		LayeredMoves.Reset();
		MovementModifiers.Reset();
	}
};

/** 
 *  Double Buffer struct for various Mover data. 
 */
template<typename T>
struct FMoverDoubleBuffer
{
	// Sets all buffered data - usually used for initializing data
	void SetBufferedData(const T& InDataToCopy)
	{
		Buffer[0] = InDataToCopy;
		Buffer[1] = InDataToCopy;
	}
	
	// Gets data that is safe to read and is not being written to
	const T& GetReadable() const
	{
		return Buffer[ReadIndex];
	}

	// Gets data that is being written to and is expected to change
	T& GetWritable()
	{
		return Buffer[(ReadIndex + 1) % 2];
	}

	// Flips which data in the buffer we return for reading and writing
	void Flip()
	{
		ReadIndex = (ReadIndex + 1) % 2;
	}
	
private:
	uint32 ReadIndex = 0;
	T Buffer[2];
};

// Auxiliary state that is input into the simulation (changes rarely)
USTRUCT(BlueprintType)
struct FMoverAuxStateContext
{
	GENERATED_BODY()

public:
	UScriptStruct* GetStruct() const { return StaticStruct(); }

	bool ShouldReconcile(const FMoverAuxStateContext& AuthorityState) const
	{ 
		return AuxStateCollection.ShouldReconcile(AuthorityState.AuxStateCollection); 
	}

	void NetSerialize(const FNetSerializeParams& P)
	{
		bool bIgnoredResult(false);
		AuxStateCollection.NetSerialize(P.Ar, nullptr, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		AuxStateCollection.ToString(Out);
	}

	void Interpolate(const FMoverAuxStateContext* From, const FMoverAuxStateContext* To, float Pct)
	{
		AuxStateCollection.Interpolate(From->AuxStateCollection, To->AuxStateCollection, Pct);
	}

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FMoverDataCollection AuxStateCollection;
};


/**
 * Contains all state data for the start of a simulation tick
 */
USTRUCT(BlueprintType)
struct FMoverTickStartData
{
	GENERATED_BODY()

	FMoverTickStartData() {}
	FMoverTickStartData(
			const FMoverInputCmdContext& InInputCmd,
			const FMoverSyncState& InSyncState,
			const FMoverAuxStateContext& InAuxState)
		:  InputCmd(InInputCmd), SyncState(InSyncState), AuxState(InAuxState)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FMoverInputCmdContext InputCmd;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FMoverSyncState SyncState;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FMoverAuxStateContext AuxState;
};

/**
 * Contains all state data produced by a simulation tick, including new simulation state
 */
USTRUCT(BlueprintType)
struct FMoverTickEndData
{
	GENERATED_BODY()

	FMoverTickEndData() {}
	FMoverTickEndData(
		const FMoverSyncState* SyncState,
		const FMoverAuxStateContext* AuxState)
	{
		this->SyncState = *SyncState;
		this->AuxState = *AuxState;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FMoverSyncState SyncState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FMoverAuxStateContext AuxState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FMovementModeTickEndState MovementEndState;

	FMovementRecord MoveRecord;
};

// Input parameters to provide context for SimulationTick functions
USTRUCT(BlueprintType)
struct FSimulationTickParams
{
	GENERATED_BODY()

	// Components involved in movement by the simulation
	// This will be empty when the simulation is ticked asynchronously
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FMovingComponentSet MovingComps;

	// Blackboard
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	TObjectPtr<UMoverBlackboard> SimBlackboard;

	// Simulation state data at the start of the tick, including Input Cmd
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FMoverTickStartData StartState;

	// Time and frame information for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FMoverTimeStep TimeStep;

	// Proposed movement for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FProposedMove ProposedMove;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UMoverInputProducerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * MoverInputProducerInterface: API for any object that can produce input for a Mover simulation frame
 */
class IMoverInputProducerInterface : public IInterface
{
	GENERATED_BODY()

public:
	/** Contributes additions to the input cmd for this simulation frame. Typically this is translating accumulated user input (or AI state) into parameters that affect movement. */
	UFUNCTION(BlueprintNativeEvent)
	void ProduceInput(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult);
};


/** 
 * FMoverPredictTrajectoryParams: parameter block for querying future trajectory samples based on a starting state
 * See UMoverComponent::GetPredictedTrajectory
 */
USTRUCT(BlueprintType)
struct FMoverPredictTrajectoryParams
{
	GENERATED_BODY()

	/** How many samples to predict into the future, including the first sample, which is always a snapshot of the
	 *  starting state with 0 accumulated time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = 1))
	int32 NumPredictionSamples = 1;

	/* How much time between predicted samples */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = 0.00001))
	float SecondsPerSample = 0.333f;

	/** If true, samples are based on the visual component transform, rather than the 'updated' movement root.
	 *  Typically, this is a mesh with its component location at the bottom of the collision primitive.
	 *  If false, samples are from the movement root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bUseVisualComponentRoot = false;

	/** If true, gravity will not taken into account during prediction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bDisableGravity = false;

 	/** Optional starting sync state. If not set, prediction will begin from the current state. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
 	TOptional<FMoverSyncState> OptionalStartSyncState;
 
 	/** Optional starting aux state. If not set, prediction will begin from the current state. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
 	TOptional<FMoverAuxStateContext> OptionalStartAuxState;

 	/** Optional input cmds to use, one per sample. If none are specified, prediction will begin with last-used inputs. 
 	 *  If too few are specified for the number of samples, the final input in the array will be used repeatedly to cover remaining samples. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
 	TArray<FMoverInputCmdContext> OptionalInputCmds;

};

USTRUCT()
struct FMoverSimulationEventData
{
	GENERATED_BODY()

	FMoverSimulationEventData(float InEventTimeMs)
		: EventTimeMs(InEventTimeMs)
	{
	}
	FMoverSimulationEventData() {}

	// User must override
	MOVER_API virtual UScriptStruct* GetScriptStruct() const;

	template<typename T>
	T* CastTo_Mutable()
	{
		return T::StaticStruct() == GetScriptStruct() ? static_cast<T*>(this) : nullptr;
	}

	template<typename T>
	const T* CastTo() const
	{
		return const_cast<const T*>(const_cast<FMoverSimulationEventData*>(this)->CastTo_Mutable<T>());
	}

	float EventTimeMs = 0.0f;
};

USTRUCT()
struct FMovementModeChangedEventData : public FMoverSimulationEventData
{
	GENERATED_BODY()

	FMovementModeChangedEventData(float InEventTimeMs, const FName InPreviousModeName, const FName InNewModeName)
		: FMoverSimulationEventData(InEventTimeMs)
		, PreviousModeName(InPreviousModeName)
		, NewModeName(InNewModeName)
	{
	}
	FMovementModeChangedEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FMovementModeChangedEventData::StaticStruct();
	}

	FName PreviousModeName = NAME_None;
	FName NewModeName = NAME_None;
};

namespace UE::Mover
{
	struct FSimulationOutputData
	{
		MOVER_API void Reset();
		MOVER_API void Interpolate(const FSimulationOutputData& From, const FSimulationOutputData& To, float Alpha, float SimTimeMs);

		FMoverSyncState SyncState;
		FMoverInputCmdContext LastUsedInputCmd;
		FMoverDataCollection AdditionalOutputData;
		TArray<TSharedPtr<FMoverSimulationEventData>> Events;
	};

	class FSimulationOutputRecord
	{
	public:
		struct FData
		{
			MOVER_API void Reset();

			FMoverTimeStep TimeStep;
			FSimulationOutputData SimOutputData;
		};

		MOVER_API void Add(const FMoverTimeStep& InTimeStep, const FSimulationOutputData& InData);
		MOVER_API const FSimulationOutputData& GetLatest() const;
		MOVER_API void GetInterpolated(float AtBaseTimeMs, FMoverTimeStep& OutTimeStep, FSimulationOutputData& OutData) const;
		MOVER_API void Clear();

	private:
		FData Data[2];
		uint8 CurrentIndex = 1;
	};

} // namespace UE::Mover

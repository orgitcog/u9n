// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/GeometryParticlesfwd.h"
#include "Backends/MoverBackendLiaison.h"
#include "MoverComponent.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "MoverNetworkPhysicsLiaisonBase.generated.h"

#define UE_API MOVER_API

//////////////////////////////////////////////////////////////////////////
// Physics networking

USTRUCT()
struct FNetworkPhysicsMoverInputs : public FNetworkPhysicsData
{
	GENERATED_BODY()

	UPROPERTY()
	FMoverInputCmdContext InputCmdContext;

	/**  Apply the data onto the network physics component */
	UE_API virtual void ApplyData(UActorComponent* NetworkComponent) const override;

	/**  Build the data from the network physics component */
	UE_API virtual void BuildData(const UActorComponent* NetworkComponent) override;

	/** Decay input during resimulation and forward prediction */
	UE_API virtual void DecayData(float DecayAmount) override;

	/**  Serialize data function that will be used to transfer the struct across the network */
	UE_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Interpolate the data in between two inputs data */
	UE_API virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;
	
	/** Merge data into this input */
	UE_API virtual void MergeData(const FNetworkPhysicsData& FromData) override;

	/** Check input data is valid - Input is send from client to server, no need to make sure it's reasonable */
	UE_API virtual void ValidateData(const UActorComponent* NetworkComponent) override;

	UE_API virtual bool CompareData(const FNetworkPhysicsData& PredictedData) override;

	/** Return string with debug information */
	UE_API virtual const FString DebugData() override;
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsMoverInputs> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsMoverInputs>
{
	enum
	{
		WithNetSerializer = true,
	};
};

USTRUCT()
struct FNetworkPhysicsMoverState : public FNetworkPhysicsData
{
	GENERATED_BODY()

	UPROPERTY()
	FMoverSyncState SyncStateContext;

	/**  Apply the data onto the network physics component */
	UE_API virtual void ApplyData(UActorComponent* NetworkComponent) const override;

	/**  Build the data from the network physics component */
	UE_API virtual void BuildData(const UActorComponent* NetworkComponent) override;

	/**  Serialize data function that will be used to transfer the struct across the network */
	UE_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Interpolate the data in between two inputs data */
	UE_API virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;

	UE_API virtual bool CompareData(const FNetworkPhysicsData& PredictedData) override;
	
	/** Return string with debug information */
	UE_API virtual const FString DebugData() override;
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsMoverState> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsMoverState>
{
	enum
	{
		WithNetSerializer = true,
	};
};

struct FNetworkPhysicsMoverTraits
{
	using InputsType = FNetworkPhysicsMoverInputs;
	using StatesType = FNetworkPhysicsMoverState;
};

//////////////////////////////////////////////////////////////////////////
// FSimulationOutputRecord

class FSimulationOutputRecord
{
public:
	struct FData
	{
		double SimTime;
		FMoverSyncState SyncState;
		FMoverInputCmdContext InputCmd;

		void Clear()
		{
			SimTime = 0.f;
			SyncState.Reset();
			InputCmd.Reset();
		}
	};

	void Add(double InSimTime, const FMoverSyncState& InSyncState, const FMoverInputCmdContext& InInputCmd)
	{
		CurrentIndex = (CurrentIndex + 1) % 2;
		Data[CurrentIndex].SimTime = InSimTime;
		Data[CurrentIndex].SyncState = InSyncState;
		Data[CurrentIndex].InputCmd = InInputCmd;
	}

	const FMoverSyncState& GetLatestSyncState() const
	{
		return Data[CurrentIndex].SyncState;
	}

	const FMoverInputCmdContext& GetLatestInputCmd() const
	{
		return Data[CurrentIndex].InputCmd;
	}

	void GetInterpolated(double SimTime, FMoverSyncState& OutSyncState, FMoverInputCmdContext& OutInputCmd) const
	{
		const uint8 PrevIndex = (CurrentIndex + 1) % 2;
		const double PrevTime = Data[PrevIndex].SimTime;
		const double CurrTime = Data[CurrentIndex].SimTime;

		if (FMath::IsNearlyEqual(PrevTime, CurrTime) || (SimTime >= CurrTime))
		{
			OutSyncState = Data[CurrentIndex].SyncState;
			OutInputCmd = Data[CurrentIndex].InputCmd;
		}
		else if (SimTime <= PrevTime)
		{
			OutSyncState = Data[PrevIndex].SyncState;
			OutInputCmd = Data[PrevIndex].InputCmd;
		}
		else
		{
			const float Alpha = FMath::Clamp((SimTime - PrevTime) / (CurrTime - PrevTime), 0.0f, 1.0f);
			OutSyncState.Interpolate(&Data[PrevIndex].SyncState, &Data[CurrentIndex].SyncState, Alpha);
			OutInputCmd.Interpolate(&Data[PrevIndex].InputCmd, &Data[CurrentIndex].InputCmd, Alpha);
		}
	}

	void Clear()
	{
		CurrentIndex = 1;
		Data[0].Clear();
		Data[1].Clear();
	}

	FData Data[2];
	uint8 CurrentIndex = 1;
};

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponentBase

/**
 * Base class for liaison components to act as a middleman between an actor's Mover component and the Chaos physics network prediction system to move
 * the Mover's updated component on the physics thread (PT).
 * 
 * This is accomplished by registering a FPhysicsMoverManagerAsyncCallback and exposing the game thread (GT) and PT methods for the TSimCallback to
 * virtual methods on this liaison.
 */
UCLASS(MinimalAPI, Abstract, Within = MoverComponent)
class UMoverNetworkPhysicsLiaisonComponentBase : public UActorComponent, public IMoverBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	UE_API UMoverNetworkPhysicsLiaisonComponentBase();

	// IMoverBackendLiaisonInterface
	UE_API virtual float GetCurrentSimTimeMs() override;
	UE_API virtual int32 GetCurrentSimFrame() override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult ValidateData(FDataValidationContext& Context, const UMoverComponent& ValidationMoverComp) const override;
#endif

	// UObject interface
	UE_API virtual void OnRegister() override;
	UE_API virtual void OnUnregister() override;
	UE_API virtual void InitializeComponent() override;
	UE_API virtual void UninitializeComponent() override;
	UE_API virtual bool ShouldCreatePhysicsState() const override;
	UE_API virtual bool CanCreatePhysics() const;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Required for Network Physics Rewind/Resim data
	UE_API void GetCurrentInputData(OUT FMoverInputCmdContext& InputCmd) const;
	UE_API void SetCurrentInputData(const FMoverInputCmdContext& InputCmd);
	UE_API void GetCurrentStateData(OUT FMoverSyncState& SyncState) const;
	UE_API void SetCurrentStateData(const FMoverSyncState& SyncState);
	UE_API bool ValidateInputData(FMoverInputCmdContext& InputCmd) const;

	// Used by the manager to uniquely identify the component
	UE_API Chaos::FUniqueIdx GetUniqueIdx() const;

	/** These methods correspond to the methods of the same name on the TSimCallback that we register */
	// "External" = Called on the Game Thread (GT)
	UE_API void ProduceInput_External(int32 PhysicsStep, int32 NumSteps, OUT FPhysicsMoverAsyncInput& Input);
	UE_API virtual void ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, double OutputTimeInSeconds);
	UE_API virtual void PostPhysicsUpdate_External();

	// "Internal" = Called on the Physics Thread (PT)
	UE_API void ProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const;
	UE_API void PreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, OUT FPhysicsMoverAsyncOutput& Output) const;
	UE_API virtual void OnContactModification_Internal(const FPhysicsMoverAsyncInput& Input, Chaos::FCollisionContactModifier& Modifier) const;
	
protected:
	/** Catchall to verify that the state of this object is valid (called from both the PT and the GT) */
	UE_API virtual bool HasValidState() const;
	
	UE_API virtual void PerformProduceInput_External(float DeltaTime, OUT FPhysicsMoverAsyncInput& Input);
	
	/**
	 * Called at the beginning of PreSimulate_Internal to verify that everything is in order.
	 * Separated to ensure that the validation occurs and reduce redundant validity checks in PerformProcessInputs_Internal() overrides
	 */
	UE_API virtual bool CanProcessInputs_Internal(const FPhysicsMoverAsyncInput& Input) const;

	/** Actually do the ProcessInputs work on the PT. CanProcessInputs_Internal is guaranteed to have returned true if this is called. */
	UE_API virtual void PerformProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const;

	/** 
	 * Called at the beginning of PreSimulate_Internal to verify that everything is in order.
	 * Default behavior is simply to call CanProcessInput_Internal(). This is defined separately in case the TickParams or OutputState need validation as well.
	 * Separated to ensure that the validation occurs and reduce redundant validity checks in PerformPreSimulate_Internal() overrides
	 */
	UE_API virtual bool CanSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input) const;
	
	/** Actually do the PreSimulate work on the PT. CanSimulate_Internal is guaranteed to have returned true if this is called. */
	UE_API virtual void PerformPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, OUT FPhysicsMoverAsyncOutput& Output) const;
	
	/** Override in child classes to return the chaos particle for the component the Mover wants to move. */
	UE_API virtual Chaos::FPhysicsObject* GetControlledPhysicsObject() const;

	/** Convenience getters to skip the read interface boilerplate on the Game (aka External) or Physics (aka Internal) threads */
	UE_API Chaos::FPBDRigidParticle* GetControlledParticle_External() const;
	UE_API Chaos::FPBDRigidParticleHandle* GetControlledParticle_Internal() const;
	
	UFUNCTION()
	UE_API virtual void HandleComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange);

	UFUNCTION()
	UE_API void HandleOwningPawnControllerChanged_Server(APawn* OwnerPawn, AController* OldController, AController* NewController);
	
	UE_API Chaos::FPhysicsSolver* GetPhysicsSolver() const;
	
	UE_API void TeleportParticleBy_Internal(Chaos::FGeometryParticleHandle& Particle, const FVector& PositionDelta, const FQuat& RotationDelta) const;
	UE_API void WakeParticleIfSleeping(Chaos::FGeometryParticleHandle* Particle) const;

	UE_API int32 GetNetworkPhysicsTickOffset_Internal() const;
	UE_API int32 GetNetworkPhysicsTickOffset_External() const;

	// Time step on the physics thread when using async physics
	UE_API FMoverTimeStep GetCurrentAsyncMoverTimeStep_Internal() const;

	// Time step on the game thread when using async physics. Uses physics results time
	UE_API FMoverTimeStep GetCurrentAsyncMoverTimeStep_External() const;

	// Time step on the game thread when using non-async physics
	UE_API FMoverTimeStep GetCurrentMoverTimeStep(float DeltaSeconds) const;
	
	UE_API void InitializeSimOutputData();

	UE_API UMoverComponent& GetMoverComponent() const;

	/**
	 * These two properties are ultimately controlled by the NetworkPhysicsComponent (NPC) via FNetworkPhysicsMoverInputs and FNetworkPhysicsMoverState (at top of file).
	 * 
	 * For input, the process responsible for generating input (owning client for pawns, server for anything else) does so, and the NPC replicates
	 * it to the server (if produced on a client) and then to all clients. For input generated on the server, the physics frame/step in which it's processed on the client
	 * does not match the server physics step, even if account for the NPC's NetworkFrameOffset. This is because the server is in "the past" relative to the client.
	 *
	 * For the sync state, the server is always the authority. By default, the sync state from the server is only applied on the client after a rewind, but this can be
	 * changed via PhysicsReplicationCVars::ResimulationCVars::bApplySimProxyStateAtRuntime (or the corresponding override in FNetworkPhysicsSettingsNetworkPhysicsComponent).
	 */
	FMoverInputCmdContext LatestInputCmd;
	mutable FMoverSyncState LatestSyncState;

	UPROPERTY(Transient)
	TObjectPtr<UNetworkPhysicsComponent> NetworkPhysicsComponent;

	// This stores the previous two outputs from the physics thread
	// which can be interpolated to get the new output at a time
	// that matches the interpolated physics particle.
	FSimulationOutputRecord SimOutputRecord;
	
	bool bCachedInputIsValid = false;
	bool bUsingAsyncPhysics = false;
};

#undef UE_API

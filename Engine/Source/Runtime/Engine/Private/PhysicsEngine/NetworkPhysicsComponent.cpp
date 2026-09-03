// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/NetworkPhysicsComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineLogs.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PBDRigidsSolver.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Net/Core/PushModel/PushModel.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#endif // UE_WITH_IRIS

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPhysicsComponent)

namespace PhysicsReplicationCVars
{
	namespace ResimulationCVars
	{
		int32 RedundantInputs = 2;
		static FAutoConsoleVariableRef CVarResimRedundantInputs(TEXT("np2.Resim.RedundantInputs"), RedundantInputs, TEXT("How many extra inputs to send with each unreliable network message, to account for packetloss. From owning client to server and back to owning client. NOTE: This is disabled while np2.Resim.DynamicInputScaling.Enabled is enabled."));
		int32 RedundantRemoteInputs = 1;
		static FAutoConsoleVariableRef CVarResimRedundantRemoteInputs(TEXT("np2.Resim.RedundantRemoteInputs"), RedundantRemoteInputs, TEXT("How many extra inputs to send with each unreliable network message, to account for packetloss. From server to remote clients."));
		int32 RedundantStates = 0;
		static FAutoConsoleVariableRef CVarResimRedundantStates(TEXT("np2.Resim.RedundantStates"), RedundantStates, TEXT("How many extra states to send with each unreliable network message, to account for packetloss."));

		bool bDynamicInputScalingEnabled = true;
		static FAutoConsoleVariableRef CVarDynamicInputScalingEnabled(TEXT("np2.Resim.DynamicInputScaling.Enabled"), bDynamicInputScalingEnabled, TEXT("Enable dynmic scaling of number of inputs sent from owning client to the server to account for packet loss. The server will control the value based on how often the server has a hole in its input buffer. NOTE: This overrides np2.Resim.RedundantInputs"));
		float DynamicInputScalingMaxInputsPercent = 0.1f;
		static FAutoConsoleVariableRef CVarDynamicInputScalingMaxInputsPercent(TEXT("np2.Resim.DynamicInputScaling.MaxInputsPercent"), DynamicInputScalingMaxInputsPercent, TEXT("Default 0.1 (= 10%, value in percent as multiplier). Sets the max scalable number of inputs to network from owning client to server as a percentage of the physics fixed tick-rate. 10% of 30Hz = 3 inputs at max."));
		int32 DynamicInputScalingMinInputs = 2;
		static FAutoConsoleVariableRef CVarDynamicInputScalingMinInputs(TEXT("np2.Resim.DynamicInputScaling.MinInputs"), DynamicInputScalingMinInputs, TEXT("Default 2. Sets the minimum scalable number of inputs to network from owning client to server."));
		float DynamicInputScalingIncreaseAverageMultiplier = 0.2f;
		static FAutoConsoleVariableRef CVarDynamicInputScalingIncreaseAverageMultiplier(TEXT("np2.Resim.DynamicInputScaling.IncreaseAverageMultiplier"), DynamicInputScalingIncreaseAverageMultiplier, TEXT("Default 0.2 (= 20%). Multiplier for how fast the average input scaling value increases. NOTE it's recommended to have a higher value than np2.Resim.DynamicInputScaling.DecreaseAverageMultiplier so the average can grow quick when network conditions gets worse."));
		float DynamicInputScalingDecreaseAverageMultiplier = 0.1f;
		static FAutoConsoleVariableRef CVarDynamicInputScalingDecreaseAverageMultiplier(TEXT("np2.Resim.DynamicInputScaling.DecreaseAverageMultiplier"), DynamicInputScalingDecreaseAverageMultiplier, TEXT("Default 0.1 (= 10%). Multiplier for how fast the average input scaling value decreases. NOTE it's recommended to have a lower value than np2.Resim.DynamicInputScaling.IncreaseAverageMultiplier so the average doesn't try to decrease too quickly which can cause repeated desyncs."));
		float DynamicInputScalingIncreaseTimeInterval = 2.0f;
		static FAutoConsoleVariableRef CVarDynamicInputScalingIncreaseTimeInterval(TEXT("np2.Resim.DynamicInputScaling.IncreaseTimeInterval"), DynamicInputScalingIncreaseTimeInterval, TEXT("Default 2.0 (value in seconds). How often dynamic scaling can increase the number of inputs to send." ));
		float DynamicInputScalingDecreaseTimeInterval = 10.0f;
		static FAutoConsoleVariableRef CVarDynamicInputScalingDecreaseTimeInterval(TEXT("np2.Resim.DynamicInputScaling.DecreaseTimeInterval"), DynamicInputScalingDecreaseTimeInterval, TEXT("Default 10.0 (value in seconds). How often dynamic scaling can decrease the number of inputs to send."));

		bool bAllowRewindToClosestState = true;
		static FAutoConsoleVariableRef CVarResimAllowRewindToClosestState(TEXT("np2.Resim.AllowRewindToClosestState"), bAllowRewindToClosestState, TEXT("When rewinding to a specific frame, if the client doens't have state data for that frame, use closest data available. Only affects the first rewind frame, when FPBDRigidsEvolution is set to Reset."));
		bool bCompareStateToTriggerRewind = false;
		static FAutoConsoleVariableRef CVarResimCompareStateToTriggerRewind(TEXT("np2.Resim.CompareStateToTriggerRewind"), bCompareStateToTriggerRewind, TEXT("When true, cache local FNetworkPhysicsData state in rewind history and compare the predicted state with incoming server state to trigger resimulations if they differ, comparison done through FNetworkPhysicsData::CompareData. Only applies if IsLocallyControlled, to enable this for simulated proxies, where IsLocallyControlled is false, also enable np2.Resim.CompareStateToTriggerRewind.IncludeSimProxies)"));
		bool bCompareStateToTriggerRewindIncludeSimProxies = false;
		static FAutoConsoleVariableRef CVarResimCompareStateToTriggerRewindIncludeSimProxies(TEXT("np2.Resim.CompareStateToTriggerRewind.IncludeSimProxies"), bCompareStateToTriggerRewindIncludeSimProxies, TEXT("When true, include simulated proxies when np2.Resim.CompareStateToTriggerRewind is enabled."));
		bool bCompareInputToTriggerRewind = false;
		static FAutoConsoleVariableRef CVarResimCompareInputToTriggerRewind(TEXT("np2.Resim.CompareInputToTriggerRewind"), bCompareInputToTriggerRewind, TEXT("When true, compare local predicted FNetworkPhysicsData input with incoming server inputs to trigger resimulations if they differ, comparison done through FNetworkPhysicsData::CompareData."));
		bool bEnableUnreliableFlow = true;
		static FAutoConsoleVariableRef CVarResimEnableUnreliableFlow(TEXT("np2.Resim.EnableUnreliableFlow"), bEnableUnreliableFlow, TEXT("When true, allow data to be sent unreliably. Also sends FNetworkPhysicsData not marked with FNetworkPhysicsData::bimportant unreliably over the network."));
		bool bEnableReliableFlow = false;
		static FAutoConsoleVariableRef CVarResimEnableReliableFlow(TEXT("np2.Resim.EnableReliableFlow"), bEnableReliableFlow, TEXT("EXPERIMENTAL -- When true, allow data to be sent reliably. Also send FNetworkPhysicsData marked with FNetworkPhysicsData::bimportant reliably over the network."));
		bool bApplyDataInsteadOfMergeData = false;
		static FAutoConsoleVariableRef CVarResimApplyDataInsteadOfMergeData(TEXT("np2.Resim.ApplyDataInsteadOfMergeData"), bApplyDataInsteadOfMergeData, TEXT("When true, call ApplyData for each data instead of MergeData when having to use multiple data entries in one frame."));
		bool bAllowInputExtrapolation = true;
		static FAutoConsoleVariableRef CVarResimAllowInputExtrapolation(TEXT("np2.Resim.AllowInputExtrapolation"), bAllowInputExtrapolation, TEXT("When true and not locally controlled, allow inputs to be extrapolated from last known and if there is a gap allow interpolation between two known inputs."));
		bool bValidateDataOnGameThread = false;
		static FAutoConsoleVariableRef CVarResimValidateDataOnGameThread(TEXT("np2.Resim.ValidateDataOnGameThread"), bValidateDataOnGameThread, TEXT("When true, perform server-side input validation through FNetworkPhysicsData::ValidateData on the Game Thread, note that LocalFrame will be the same as ServerFrame on Game Thread. If false, perform the call on the Physics Thread."));
		bool bApplySimProxyStateAtRuntime = false;
		static FAutoConsoleVariableRef CVarResimApplySimProxyStateAtRuntime(TEXT("np2.Resim.ApplySimProxyStateAtRuntime"), bApplySimProxyStateAtRuntime, TEXT("When true, call ApplyData on received states for simulated proxies at runtime."));
		bool bApplySimProxyInputAtRuntime = true;
		static FAutoConsoleVariableRef CVarResimApplySimProxyInputAtRuntime(TEXT("np2.Resim.ApplySimProxyInputAtRuntime"), bApplySimProxyInputAtRuntime, TEXT("When true, call ApplyData on received inputs for simulated proxies at runtime."));

		bool bEnableStatefulDeltaSerialization = true;
		static FAutoConsoleVariableRef CVarResimEnableStatefulDeltaSerialization(TEXT("np2.Resim.StatefulDeltaSerialization.Enable"), bEnableStatefulDeltaSerialization, TEXT("Enables stateful delta serialization for FNetworkPhysicsData derived inputs and states. During FNetworkPhysicsData::NetSerialize there will be a valid pointer to a previous FNetworkPhysicsData which can be used for delta serialization, FNetworkPhysicsData::DeltaSourceData. NOTE: Switching this during gameplay might cause disconnections."));
		bool bUseDefaultDeltaForDeltaSourceReplication = true;
		static FAutoConsoleVariableRef CVarResimUseDefaultForDeltaSourceReplication(TEXT("np2.Resim.StatefulDeltaSerialization.UseDefaultForDeltaSourceReplication"), bUseDefaultDeltaForDeltaSourceReplication, TEXT("When false delta sources will use standard serialization when being replicated. When true there will be a valid delta source pointer to default data which can be used for delta serialization when replicating delta sources."));
		float TimeToSyncStatefulDeltaSource = 5.0f;
		static FAutoConsoleVariableRef CVarResimTimeToSyncStatefulDeltaSource(TEXT("np2.Resim.StatefulDeltaSerialization.TimeToSyncStatefulDeltaSource"), TimeToSyncStatefulDeltaSource, TEXT("The time in seconds between synchronizing the stateful delta source from server to clients."));
		
		bool bApplyPredictiveInterpolationWhenBehindServer = true;
		static FAutoConsoleVariableRef CVarResimApplyPredictiveInterpolationWhenBehindServer(TEXT("np2.Resim.ApplyPredictiveInterpolationWhenBehindServer"), bApplyPredictiveInterpolationWhenBehindServer, TEXT("When true, switch over to replicating with Predictive Interpolation temporarily, when the client receive target states from the server for frames that have not yet simulated on the client. When false apply the received target via a resimulation when the client has caught up and simulated the corresponding frame."));
	}
}

bool FNetworkPhysicsRewindDataProxy::NetSerializeBase(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, TUniqueFunction<TUniquePtr<Chaos::FBaseRewindHistory> ()> CreateHistoryFunction , TUniqueFunction<FNetworkPhysicsData* (const int32)> GetDeltaSourceData)
{
	bDeltaSerializationIssue = false;
	Ar << Owner;

	bool bHasData = History.IsValid();
	Ar.SerializeBits(&bHasData, 1);

	if (bHasData)
	{
		if (Ar.IsLoading() && !History.IsValid())
		{
			if(ensureMsgf(Owner, TEXT("FNetRewindDataBase::NetSerialize: owner is null")))
			{
				History = CreateHistoryFunction();
				if (!ensureMsgf(History.IsValid(), TEXT("FNetRewindDataBase::NetSerialize: failed to create history. Owner: %s"), *GetFullNameSafe(Owner)))
				{
					Ar.SetError();
					bOutSuccess = false;
					return true;
				}
			}
			else
			{
				Ar.SetError();
				bOutSuccess = false;
				return true;
			}
		}

		History->NetSerialize(Ar, Map, [&](void* Data, const int32 DataIndex)
			{
				if (FNetworkPhysicsData* NetData = static_cast<FNetworkPhysicsData*>(Data))
				{
					if (Owner)
					{
						// Set the component pointer to the implementation that uses this data
						NetData->SetImplementationComponent(Owner.Get()->ActorComponent.Get());

						// Only use stateful delta source for the first entry in history, the following entries will use the previous entry as delta source
						if (PhysicsReplicationCVars::ResimulationCVars::bEnableStatefulDeltaSerialization && DataIndex == 0 && GetDeltaSourceData)
						{
							// Stateful Delta Serialization
							uint32 DeltaSourceFrame = 0;
							if (Ar.IsLoading())
							{
								Ar.SerializeIntPacked(DeltaSourceFrame);
								
								FNetworkPhysicsData* DeltaSourceData = nullptr;
								if (DeltaSourceFrame > 0)
								{
									// Try get a valid delta source for frame
									DeltaSourceData = GetDeltaSourceData(static_cast<int32>(DeltaSourceFrame) - 1);
								}
								else
								{
									// Sender used default as delta source
									DeltaSourceData = GetDeltaSourceData(/*Default*/ -2);
								}
								
								if (!DeltaSourceData)
								{
#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
									const int32 Frame = static_cast<int32>(DeltaSourceFrame) - 1;
									UE_LOG(LogChaos, Warning, TEXT("[DEBUG Delta Serialization] %s ISSUE, did not find delta source with frame: %d  --  Name: %s")
										, (Owner->HasServerWorld() ? TEXT("[SERVER]    ") : (Owner->IsLocallyControlled() ? TEXT("[AUTONOMOUS]") : TEXT("[SIMULATED] "))), Frame, *AActor::GetDebugName(Owner->GetOwner()));
#endif
									bDeltaSerializationIssue = true;
									DeltaSourceData = GetDeltaSourceData(/*Default*/ -2);
								}
								
								NetData->SetDeltaSourceData(DeltaSourceData);
							}
							else // IsSaving
							{
								if (FNetworkPhysicsData* DeltaSourceData = GetDeltaSourceData(/*Latest*/ -1))
								{
									NetData->SetDeltaSourceData(DeltaSourceData);

									ensure((DeltaSourceData->ServerFrame + 1) >= 0);
									DeltaSourceFrame = static_cast<uint32>(DeltaSourceData->ServerFrame + 1); // +1 since ServerFrame has a default value of -1 and it needs to be serialized unsigned
								}
								else
								{
									ensureMsgf(false, TEXT("Delta Serialization failed to get the latest delta source when sending, should not happen. On the first send the latest index should be populated with a default value, not null."));
									NetData->SetDeltaSourceData(GetDeltaSourceData(/*Default*/ -2));
									DeltaSourceFrame = 0; // Set DeltaSourceFrame to 0 to indicate that default delta source was used
								}

								Ar.SerializeIntPacked(DeltaSourceFrame);
							}
						}
					}
				}
			});
	}

	return true;
}

FNetworkPhysicsRewindDataProxy& FNetworkPhysicsRewindDataProxy::operator=(const FNetworkPhysicsRewindDataProxy& Other)
{
	if (&Other != this)
	{
		Owner = Other.Owner;
		History = Other.History ? Other.History->Clone() : nullptr;
	}

	return *this;
}

#if UE_WITH_IRIS
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataInputProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataRemoteInputProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataStateProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataImportantInputProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataImportantStateProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataDeltaSourceInputProxy);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(NetworkPhysicsRewindDataDeltaSourceStateProxy);
#endif // UE_WITH_IRIS

bool FNetworkPhysicsRewindDataInputProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->InputHelper->CreateUniqueRewindHistory(0); }, [this](const int32 Value) -> FNetworkPhysicsData* { return Owner->GetDeltaSourceInput(Value, /*bValueIsIndex*/ false); });
#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
	if (bDeltaSerializationIssue)
	{
		UE_LOG(LogChaos, Warning, TEXT("		[DEBUG Delta Serialization] INPUT"));
	}
#endif
	return bSuccess;
}

bool FNetworkPhysicsRewindDataRemoteInputProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->InputHelper->CreateUniqueRewindHistory(0); }, [this](const int32 Value) -> FNetworkPhysicsData* { return Owner->GetDeltaSourceInput(Value, /*bValueIsIndex*/ false); });

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
	if (bDeltaSerializationIssue) { UE_LOG(LogChaos, Warning, TEXT("		[DEBUG Delta Serialization] REMOTE INPUT")); }
#endif

	return bSuccess;
}

bool FNetworkPhysicsRewindDataStateProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->StateHelper->CreateUniqueRewindHistory(0); }, [this](const int32 Value) -> FNetworkPhysicsData* { return Owner->GetDeltaSourceState(Value, /*bValueIsIndex*/ false); });

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
	if (bDeltaSerializationIssue) { UE_LOG(LogChaos, Warning, TEXT("		[DEBUG Delta Serialization] STATE")); }
#endif

	return bSuccess;
}

bool FNetworkPhysicsRewindDataImportantInputProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->InputHelper->CreateUniqueRewindHistory(0); }, [this](const int32 Value) -> FNetworkPhysicsData* { return Owner->GetDeltaSourceInput(Value, /*bValueIsIndex*/ false); });

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
	if (bDeltaSerializationIssue) { UE_LOG(LogChaos, Warning, TEXT("		[DEBUG Delta Serialization] IMPORTANT INPUT")); }
#endif

	return bSuccess;
}

bool FNetworkPhysicsRewindDataImportantStateProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->StateHelper->CreateUniqueRewindHistory(0); }, [this](const int32 Value) -> FNetworkPhysicsData* { return Owner->GetDeltaSourceState(Value, /*bValueIsIndex*/ false); });

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
	if (bDeltaSerializationIssue) { UE_LOG(LogChaos, Warning, TEXT("		[DEBUG Delta Serialization] IMPORTANT STATE")); }
#endif

	return bSuccess;
}

bool FNetworkPhysicsRewindDataDeltaSourceInputProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = true;
	if (PhysicsReplicationCVars::ResimulationCVars::bUseDefaultDeltaForDeltaSourceReplication)
	{
		// Use default as base for delta serialization when sending delta source
		bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->InputHelper->CreateUniqueRewindHistory(0); }, [this](const int32 Value) -> FNetworkPhysicsData* { return Owner->GetDeltaSourceInput(/*Default*/ -2, /*bValueIsIndex*/ false); });
	}
	else
	{
		// Standard serialization for delta source
		bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->InputHelper->CreateUniqueRewindHistory(0); }, nullptr);
	}

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
	if (bDeltaSerializationIssue) { UE_LOG(LogChaos, Warning, TEXT("		[DEBUG Delta Serialization] DELTA INPUT")); }
#endif

	return bSuccess;
}

bool FNetworkPhysicsRewindDataDeltaSourceStateProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = true;
	if (PhysicsReplicationCVars::ResimulationCVars::bUseDefaultDeltaForDeltaSourceReplication)
	{
		// Use default as base for delta serialization when sending delta source
		bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->StateHelper->CreateUniqueRewindHistory(0); }, [this](const int32 Value) -> FNetworkPhysicsData* { return Owner->GetDeltaSourceState(/*Default*/ -2, /*bValueIsIndex*/ false); });
	}
	else
	{
		// Standard serialization for delta source
		bSuccess = NetSerializeBase(Ar, Map, bOutSuccess, [this]() { return Owner->StateHelper->CreateUniqueRewindHistory(0); }, nullptr);
	}

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
	if (bDeltaSerializationIssue) { UE_LOG(LogChaos, Warning, TEXT("		[DEBUG Delta Serialization] DELTA STATE")); }
#endif

	return bSuccess;
}

// --------------------------- Network Physics Callback ---------------------------

// Before PreSimulate_Internal
void FNetworkPhysicsCallback::ProcessInputs_Internal(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbacks)
{
	PreProcessInputsInternal.Broadcast(PhysicsStep);
	for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
	{
		SimCallbackObject->ProcessInputs_Internal(PhysicsStep);
	}
	PostProcessInputsInternal.Broadcast(PhysicsStep);
}

void FNetworkPhysicsCallback::PreResimStep_Internal(int32 PhysicsStep, bool bFirst)
{
	if (bFirst)
	{
		for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
		{
			SimCallbackObject->FirstPreResimStep_Internal(PhysicsStep);
		}
	}
}

void FNetworkPhysicsCallback::PostResimStep_Internal(int32 PhysicsStep)
{

}

int32 FNetworkPhysicsCallback::TriggerRewindIfNeeded_Internal(int32 LatestStepCompleted)
{
	int32 ResimFrame = INDEX_NONE;
	for (Chaos::ISimCallbackObject* SimCallbackObject : RewindableCallbackObjects)
	{
		const int32 CallbackFrame = SimCallbackObject->TriggerRewindIfNeeded_Internal(LatestStepCompleted);
		ResimFrame = (ResimFrame == INDEX_NONE) ? CallbackFrame : FMath::Min(CallbackFrame, ResimFrame);
	}

	if (RewindData)
	{
		if (!PhysicsReplicationCVars::ResimulationCVars::bApplyPredictiveInterpolationWhenBehindServer)
		{
			const int32 TargetStateComparisonFrame = RewindData->CompareTargetsToLastFrame();
			ResimFrame = (ResimFrame == INDEX_NONE) ? TargetStateComparisonFrame : (TargetStateComparisonFrame == INDEX_NONE) ? ResimFrame : FMath::Min(TargetStateComparisonFrame, ResimFrame);
		}

		const int32 ReplicationFrame = RewindData->GetResimFrame();
		ResimFrame = (ResimFrame == INDEX_NONE) ? ReplicationFrame : (ReplicationFrame == INDEX_NONE) ? ResimFrame : FMath::Min(ReplicationFrame, ResimFrame);

		if (ResimFrame != INDEX_NONE)
		{
			const int32 ValidFrame = RewindData->FindValidResimFrame(ResimFrame);
#if DEBUG_NETWORK_PHYSICS || DEBUG_REWIND_DATA
			UE_LOG(LogChaos, Log, TEXT("CLIENT | PT | TriggerRewindIfNeeded_Internal | Requested Resim Frame = %d (%d / %d) | Valid Resim Frame = %d"), ResimFrame, TargetStateComparisonFrame, ReplicationFrame, ValidFrame);
#endif
			ResimFrame = ValidFrame;
		}
	}
	
	return ResimFrame;
}

void FNetworkPhysicsCallback::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	InjectInputsExternal.Broadcast(PhysicsStep, NumSteps);
}

void FNetworkPhysicsCallback::ProcessInputs_External(int32 PhysicsStep, const TArray<Chaos::FSimCallbackInputAndObject>& SimCallbackInputs)
{
	for(const Chaos::FSimCallbackInputAndObject& SimCallbackObject : SimCallbackInputs)
	{
		if (SimCallbackObject.CallbackObject && SimCallbackObject.CallbackObject->HasOption(Chaos::ESimCallbackOptions::Rewind))
		{
			SimCallbackObject.CallbackObject->ProcessInputs_External(PhysicsStep);
		}
	}
}


// --------------------------- Network Physics System ---------------------------

UNetworkPhysicsSystem::UNetworkPhysicsSystem()
{}

void UNetworkPhysicsSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UWorld* World = GetWorld();
	check(World);

	if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
	{
		FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UNetworkPhysicsSystem::OnWorldPostInit);
	}
}

void UNetworkPhysicsSystem::Deinitialize()
{}

void UNetworkPhysicsSystem::OnWorldPostInit(UWorld* World, const UWorld::InitializationValues)
{
	if (World != GetWorld())
	{
		return;
	}

	if (UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsPrediction || UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsHistoryCapture)
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{ 
				if (Solver->GetRewindCallback() == nullptr)
				{
					Solver->SetRewindCallback(MakeUnique<FNetworkPhysicsCallback>(World));
				}

				if (UPhysicsSettings::Get()->PhysicsPrediction.bEnablePhysicsHistoryCapture)
				{
					if (Solver->GetRewindData() == nullptr)
					{
						Solver->EnableRewindCapture();
					}
				}
			}
		}
	}
}


// --------------------------- GameThread Network Physics Component ---------------------------

UNetworkPhysicsComponent::UNetworkPhysicsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitPhysics();
}

UNetworkPhysicsComponent::UNetworkPhysicsComponent() : Super()
{
	InitPhysics();
}

void UNetworkPhysicsComponent::InitPhysics()
{
	if (const IConsoleVariable* CVarRedundantInputs = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.RedundantInputs")))
	{
		SetNumberOfInputsToNetwork(CVarRedundantInputs->GetInt() + 1);
	}
	if (const IConsoleVariable* CVarRedundantRemoteInputs = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.RedundantRemoteInputs")))
	{
		SetNumberOfRemoteInputsToNetwork(CVarRedundantRemoteInputs->GetInt() + 1);
	}
	if (const IConsoleVariable* CVarRedundantStates = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.RedundantStates")))
	{
		SetNumberOfStatesToNetwork(CVarRedundantStates->GetInt() + 1);
	}
	if (const IConsoleVariable* CVarCompareStateToTriggerRewind = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.CompareStateToTriggerRewind")))
	{
		bCompareStateToTriggerRewind = CVarCompareStateToTriggerRewind->GetBool();
	}
	if (const IConsoleVariable* CVarCompareStateToTriggerRewind = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.CompareStateToTriggerRewind.IncludeSimProxies")))
	{
		bCompareStateToTriggerRewindIncludeSimProxies = CVarCompareStateToTriggerRewind->GetBool();
	}
	if (const IConsoleVariable* CVarCompareInputToTriggerRewind = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.Resim.CompareInputToTriggerRewind")))
	{
		bCompareInputToTriggerRewind = CVarCompareInputToTriggerRewind->GetBool();
	}

	/** NOTE:
	* If the NetworkPhysicsComponent is added as a SubObject after the actor has processed bAutoActivate and bWantsInitializeComponent
	* SetActive(true) and InitializeComponent() needs to be called manually for the component to function properly. */
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	bAutoActivate = true;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UNetworkPhysicsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Update async component with current component properties
	UpdateAsyncComponent(true);
}

void UNetworkPhysicsComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Cache CVar values
	bEnableUnreliableFlow = PhysicsReplicationCVars::ResimulationCVars::bEnableUnreliableFlow;
	bEnableReliableFlow = PhysicsReplicationCVars::ResimulationCVars::bEnableReliableFlow;
	bValidateDataOnGameThread = PhysicsReplicationCVars::ResimulationCVars::bValidateDataOnGameThread;

	if (AActor* Owner = GetOwner())
	{
		// Get settings from NetworkPhysicsSettingsComponent, if there is one
		UNetworkPhysicsSettingsComponent* SettingsComponent = Owner->FindComponentByClass<UNetworkPhysicsSettingsComponent>();
		if (SettingsComponent)
		{
			SetNumberOfInputsToNetwork(SettingsComponent->NetworkPhysicsComponentSettings.GetRedundantInputs() + 1);
			SetNumberOfRemoteInputsToNetwork(SettingsComponent->NetworkPhysicsComponentSettings.GetRedundantRemoteInputs() + 1);
			SetNumberOfStatesToNetwork(SettingsComponent->NetworkPhysicsComponentSettings.GetRedundantStates() + 1);
			bEnableUnreliableFlow = SettingsComponent->NetworkPhysicsComponentSettings.GetEnableUnreliableFlow();
			bEnableReliableFlow = SettingsComponent->NetworkPhysicsComponentSettings.GetEnableReliableFlow();
			bValidateDataOnGameThread = SettingsComponent->NetworkPhysicsComponentSettings.GetValidateDataOnGameThread();

			if (ReplicatedInputs.History)
			{
				ReplicatedInputs.History->ResizeDataHistory(InputsToNetwork_OwnerDefault);
			}
			if (ReplicatedRemoteInputs.History)
			{
				ReplicatedRemoteInputs.History->ResizeDataHistory(InputsToNetwork_Simulated);
			}
			if (ReplicatedStates.History)
			{
				ReplicatedStates.History->ResizeDataHistory(StatesToNetwork);
			}
		}

		if (!PhysicsObject)
		{
			if (UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
			{
				SetPhysicsObject(RootPrimComp->GetPhysicsObjectByName(NAME_None));
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				// Create async component to run on Physics Thread
				NetworkPhysicsComponent_Internal = Solver->CreateAndRegisterSimCallbackObject_External<FAsyncNetworkPhysicsComponent>();
				NetworkPhysicsComponent_Internal->PhysicsObject = PhysicsObject;
				NetworkPhysicsComponent_Internal->InputsToNetwork_OwnerDefault = InputsToNetwork_OwnerDefault;
				NetworkPhysicsComponent_Internal->InputsToNetwork_Simulated = InputsToNetwork_Simulated;
				NetworkPhysicsComponent_Internal->StatesToNetwork = StatesToNetwork;
				NetworkPhysicsComponent_Internal->bCompareStateToTriggerRewind = bCompareStateToTriggerRewind;
				NetworkPhysicsComponent_Internal->bCompareStateToTriggerRewindIncludeSimProxies = bCompareStateToTriggerRewindIncludeSimProxies;
				NetworkPhysicsComponent_Internal->bCompareInputToTriggerRewind = bCompareInputToTriggerRewind;
				CreateAsyncDataHistory();
				UpdateAsyncComponent(true);

				/** Run OnInitialize_Internal on the ISimCallbackObject first thing on the next physics thread frame */
				FAsyncNetworkPhysicsComponent* AsyncNetworkPhysicsComponent = NetworkPhysicsComponent_Internal;
				Solver->EnqueueCommandImmediate(
					[AsyncNetworkPhysicsComponent]()
					{
						if (AsyncNetworkPhysicsComponent)
						{
							AsyncNetworkPhysicsComponent->OnInitialize_Internal();
						}
					}
				);
			}
		}
	}
}

void UNetworkPhysicsComponent::UninitializeComponent()
{
	Super::UninitializeComponent();

	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			AsyncInput->ActorComponent = nullptr;
			AsyncInput->PhysicsObject = nullptr;
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				/* Run OnUninitialize_Internal on the ISimCallbackObject as a way to unregister input / state history, unsubscribe from delegates etc.
				* After UnregisterAndFreeSimCallbackObject_External the ISimCallbackObject will not get any callbacks anymore, use this as the last safe place to use the cached FPhysicsObject for example */
				FAsyncNetworkPhysicsComponent* AsyncNetworkPhysicsComponent = NetworkPhysicsComponent_Internal;
				Solver->EnqueueCommandImmediate(
					[AsyncNetworkPhysicsComponent]()
					{
						if (AsyncNetworkPhysicsComponent)
						{
							AsyncNetworkPhysicsComponent->OnUninitialize_Internal();
						}
					}
				);

				// Clear async component from Physics Thread and memory
				Solver->UnregisterAndFreeSimCallbackObject_External(NetworkPhysicsComponent_Internal);
			}
		}
	}
	NetworkPhysicsComponent_Internal = nullptr;
}

void UNetworkPhysicsComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams ReplicatedParamsOwner;
	ReplicatedParamsOwner.Condition = COND_OwnerOnly;
	ReplicatedParamsOwner.RepNotifyCondition = REPNOTIFY_Always;
	ReplicatedParamsOwner.bIsPushBased = true;

	FDoRepLifetimeParams ReplicatedParamsRemote;
	ReplicatedParamsRemote.Condition = COND_SkipOwner;
	ReplicatedParamsRemote.RepNotifyCondition = REPNOTIFY_Always;
	ReplicatedParamsRemote.bIsPushBased = true;

	FDoRepLifetimeParams ReplicatedParamsAll;
	ReplicatedParamsAll.Condition = COND_None;
	ReplicatedParamsAll.RepNotifyCondition = REPNOTIFY_Always;
	ReplicatedParamsAll.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UNetworkPhysicsComponent, ReplicatedDeltaSourceInput, ReplicatedParamsAll);
	DOREPLIFETIME_WITH_PARAMS_FAST(UNetworkPhysicsComponent, ReplicatedDeltaSourceState, ReplicatedParamsAll);
	DOREPLIFETIME_CONDITION(UNetworkPhysicsComponent, InputsToNetwork_Owner, COND_OwnerOnly);
	DOREPLIFETIME_WITH_PARAMS_FAST(UNetworkPhysicsComponent, ReplicatedInputs, ReplicatedParamsOwner);
	DOREPLIFETIME_WITH_PARAMS_FAST(UNetworkPhysicsComponent, ReplicatedRemoteInputs, ReplicatedParamsRemote);
	DOREPLIFETIME_WITH_PARAMS_FAST(UNetworkPhysicsComponent, ReplicatedStates, ReplicatedParamsAll);
}

// Called every Game Thread frame
void UNetworkPhysicsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateAsyncComponent(false);
	NetworkMarshaledData();
}

void UNetworkPhysicsComponent::NetworkMarshaledData()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}


	if (NetworkPhysicsComponent_Internal)
	{
		const bool bIsServer = HasServerWorld();
		if (!bIsServer && !IsNetworkPhysicsTickOffsetAssigned())
		{
			// Don't replicate data to the server until networked physics is setup with a synchronized physics tick offset
			return;
		}

		const bool bShouldSyncDeltaSourceInput = bIsServer && PhysicsReplicationCVars::ResimulationCVars::bEnableStatefulDeltaSerialization && World->GetRealTimeSeconds() > TimeToSyncDeltaSourceInput;
		const bool bShouldSyncDeltaSourceState = bIsServer && PhysicsReplicationCVars::ResimulationCVars::bEnableStatefulDeltaSerialization && World->GetRealTimeSeconds() > TimeToSyncDeltaSourceState;
		bool bHasSyncedDeltaSourceInput = false;
		bool bHasSyncedDeltaSourceState = false;

		// Replicate source data for input delta serialization
		auto DeltaSourceInputSyncHelper = [&](const TUniquePtr<Chaos::FBaseRewindHistory>& InputData)
		{
			if (bShouldSyncDeltaSourceInput && !bHasSyncedDeltaSourceInput && IsValidNextDeltaSourceInput(InputData->GetLatestFrame()))
			{
				ReplicatedDeltaSourceInput.History->SetRecordDataIncremental(true); // Only record data that is newer than already cached data
				if (InputData->CopyAllData(*ReplicatedDeltaSourceInput.History, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true))
				{
					MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedDeltaSourceInput, this);
					bHasSyncedDeltaSourceInput = true;
					TimeToSyncDeltaSourceInput = World->GetRealTimeSeconds() + PhysicsReplicationCVars::ResimulationCVars::TimeToSyncStatefulDeltaSource;
					AddDeltaSourceInput();
				}
			}
		};

		// Replicate source data for state delta serialization
		auto DeltaSourceStateSyncHelper = [&](const TUniquePtr<Chaos::FBaseRewindHistory>& StateData)
		{
			if (bShouldSyncDeltaSourceState && !bHasSyncedDeltaSourceState && IsValidNextDeltaSourceState(StateData->GetLatestFrame()))
			{
				ReplicatedDeltaSourceState.History->SetRecordDataIncremental(true); // Only record data that is newer than already cached data
				if (StateData->CopyAllData(*ReplicatedDeltaSourceState.History, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true))
				{
					MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedDeltaSourceState, this);
					bHasSyncedDeltaSourceState = true;
					TimeToSyncDeltaSourceState = World->GetRealTimeSeconds() + PhysicsReplicationCVars::ResimulationCVars::TimeToSyncStatefulDeltaSource;
					AddDeltaSourceState();
				}
			}
		};

		while (Chaos::TSimCallbackOutputHandle<FAsyncNetworkPhysicsComponentOutput> AsyncOutput = NetworkPhysicsComponent_Internal->PopFutureOutputData_External())
		{
			if (AsyncOutput->InputsToNetwork_Owner.IsSet())
			{
				// Only marshaled from PT to GT on the server, InputsToNetwork_Owner is a replicated property towards the owner
				InputsToNetwork_Owner = *AsyncOutput->InputsToNetwork_Owner;
			}

			// Unimportant / Unreliable
			if (bEnableUnreliableFlow
				&& AsyncOutput->InputData
				&& AsyncOutput->InputData->HasDataInHistory())
			{
				if (bIsServer)
				{
					// Replicate source data for delta serialization
					DeltaSourceInputSyncHelper(AsyncOutput->InputData);

					if (IsLocallyControlled())
					{
						// Send inputs to remote clients after getting marshaled from PT if server is the one controlling the component
						ReplicatedRemoteInputs.History->SetRecordDataIncremental(true); // Only record data that is newer than already cached data
						if (AsyncOutput->InputData->CopyAllData(*ReplicatedRemoteInputs.History, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true))
						{
							MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedRemoteInputs, this);
						}
					}

					// Only replicate data to owning client if bDataAltered is true i.e. the input has been altered by the server
					ReplicatedInputs.History->ResizeDataHistory(AsyncOutput->InputData->CountAlteredData(/*bIncludeUnimportant*/ true, /*bIncludeImportant*/ bEnableReliableFlow == false), EAllowShrinking::Yes);
					ReplicatedInputs.History->SetRecordDataIncremental(true); // Only record data that is newer than already cached data

					// Server sends inputs through property replication to owning client
					if (AsyncOutput->InputData->CopyAlteredData(*ReplicatedInputs.History, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ bEnableReliableFlow == false))
					{
						MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedInputs, this);
					}
				}
				else if (IsLocallyControlled()) // Client-side
				{
					ReplicatedInputs.History->ResizeDataHistory(AsyncOutput->InputData->GetHistorySize(), EAllowShrinking::Yes);

					if (AsyncOutput->InputData->CopyAllData(*ReplicatedInputs.History, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ bEnableReliableFlow == false))
					{
						// Clients send inputs through an RPC to the server
						ServerReceiveInputData(ReplicatedInputs);
					}
				}
			}

			// Important / Reliable
			if (bEnableReliableFlow)
			{
				for (const TUniquePtr<Chaos::FBaseRewindHistory>& InputImportant : AsyncOutput->InputDataImportant)
				{
					if (!InputImportant || !InputImportant->HasDataInHistory())
					{
						continue;
					}

					// Replicate source data for delta serialization
					DeltaSourceInputSyncHelper(InputImportant);

					ReplicatedImportantInput.History->ResizeDataHistory(InputImportant->GetHistorySize(), EAllowShrinking::Yes);
					if (InputImportant->CopyAllData(*ReplicatedImportantInput.History, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true))
					{
						if (bIsServer)
						{
							MulticastReceiveImportantInputData(ReplicatedImportantInput);
						}
						else if (IsLocallyControlled())
						{
							ServerReceiveImportantInputData(ReplicatedImportantInput);
						}
					}
				}
			}

			if (bIsServer)
			{
				// Unimportant / Unreliable
				if (bEnableUnreliableFlow
					&& AsyncOutput->StateData
					&& AsyncOutput->StateData->HasDataInHistory())
				{
					// Replicate source data for delta serialization
					DeltaSourceStateSyncHelper(AsyncOutput->StateData);

					if (AsyncOutput->StateData->CopyAllData(*ReplicatedStates.History, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ bEnableReliableFlow == false))
					{
						// If on server we should send the states onto all the clients through repnotify
						MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedStates, this);
					}
				}

				// Important / Reliable
				if (bEnableReliableFlow)
				{
					for (const TUniquePtr<Chaos::FBaseRewindHistory>& StateImportant : AsyncOutput->StateDataImportant)
					{
						if (!StateImportant || !StateImportant->HasDataInHistory())
						{
							continue;
						}

						// Replicate source data for delta serialization
						DeltaSourceStateSyncHelper(StateImportant);

						ReplicatedImportantState.History->ResizeDataHistory(StateImportant->GetHistorySize(), EAllowShrinking::Yes);
						if (StateImportant->CopyAllData(*ReplicatedImportantState.History, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true))
						{
							MulticastReceiveImportantStateData(ReplicatedImportantState);
						}
					}
				}
			}

			if (bStopRelayingLocalInputsDeferred)
			{
				bIsRelayingLocalInputs = false;
				bStopRelayingLocalInputsDeferred = false;
			}
		}
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedDeltaSourceInput()
{
	if (ReplicatedDeltaSourceInput.bDeltaSerializationIssue)
	{
		ensureMsgf(false, TEXT("OnRep_SetReplicatedDeltaSourceInput failed delta serialization, should not happen."));
		return;
	}

	if (!ReplicatedDeltaSourceInput.History)
	{
		return;
	}

	AddDeltaSourceInput();
}

void UNetworkPhysicsComponent::ServerReceiveDeltaSourceInputFrame_Implementation(const int32 Frame)
{
	for (int32 I = 0; I < DeltaSourceInputs.Num(); I++)
	{
		TUniquePtr<FNetworkPhysicsData>& Data = DeltaSourceInputs[I];
		if (Data->ServerFrame == Frame)
		{
			// Set latest delta source index acknowledged by the client so that we can start using this delta source
			LatestAcknowledgedDeltaSourceInputIndex = I;

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
			UE_LOG(LogChaos, Log, TEXT("[DEBUG Delta Serialization] [SERVER]     Owner acknowledged delta source INPUT frame: %d at index: %d  --  Name: %s"), Frame, LatestAcknowledgedDeltaSourceInputIndex, *AActor::GetDebugName(GetOwner()));
#endif

			break;
		}
	}
}

void UNetworkPhysicsComponent::AddDeltaSourceInput()
{
	// Get the data entry for the correct index in the data sources array
	const int32 Index = GetDeltaSourceIndexForFrame(ReplicatedDeltaSourceInput.History->GetLatestFrame());
	check(Index <= DeltaSourceInputs.Num());
	FNetworkPhysicsData* PhysicsData = DeltaSourceInputs[Index].Get();

	// Extract the data from the replicated DeltaSources property
	if (ReplicatedDeltaSourceInput.History->ExtractData(ReplicatedDeltaSourceInput.History->GetLatestFrame(), /*bResetSolver*/false, PhysicsData, /*bExactFrame*/true))
	{
		// The data is now extracted via PhysicsData and stored inside DeltaSourceInputs
		
		if (!HasServerWorld())
		{
			// On the client, set the latest index, to be used when sending inputs to the server
			LatestAcknowledgedDeltaSourceInputIndex = Index;

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
			UE_LOG(LogChaos, Log, TEXT("[DEBUG Delta Serialization] %s Received delta source INPUT for frame: %d at index: %d  --  Name: %s"), (IsLocallyControlled() ? TEXT("[AUTONOMOUS]") : TEXT("[SIMULATED] ")), ReplicatedDeltaSourceInput.History->GetLatestFrame(), LatestAcknowledgedDeltaSourceInputIndex, *AActor::GetDebugName(GetOwner()));
#endif
			if (IsLocallyControlled())
			{
				// If this client is the one controlling this entity, send back an acknowledgment to the server we have received the delta source for ServerFrame
				ServerReceiveDeltaSourceInputFrame(PhysicsData->ServerFrame);
			}
		}
		else if (IsLocallyControlled())
		{
			// If server is locally controlled, set the latest index directly, else wait for the owning client to send back ServerReceiveDeltaSourceInputFrame before the server starts to use this 
			LatestAcknowledgedDeltaSourceInputIndex = Index;
		}

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
		if (HasServerWorld())
		{
			UE_LOG(LogChaos, Log, TEXT("[DEBUG Delta Serialization] [SERVER]     Sent delta source INPUT for frame: %d at index: %d  --  Name: %s"), ReplicatedDeltaSourceInput.History->GetLatestFrame(), Index, *AActor::GetDebugName(GetOwner()));
		}
#endif

		LatestCachedDeltaSourceInputIndex = Index;
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedDeltaSourceState()
{
	if (ReplicatedDeltaSourceState.bDeltaSerializationIssue)
	{
		ensureMsgf(false, TEXT("OnRep_SetReplicatedDeltaSourceState failed delta serialization, should not happen."));
		return;
	}

	if (!ReplicatedDeltaSourceState.History)
	{
		return;
	}

	AddDeltaSourceState();
}

void UNetworkPhysicsComponent::ServerReceiveDeltaSourceStateFrame_Implementation(const int32 Frame)
{
	for (int32 I = 0; I < DeltaSourceStates.Num(); I++)
	{
		TUniquePtr<FNetworkPhysicsData>& Data = DeltaSourceStates[I];
		if (Data->ServerFrame == Frame)
		{
			// Set latest delta source index acknowledged by the client so that we can start using this delta source
			LatestAcknowledgedDeltaSourceStateIndex = I;

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
			UE_LOG(LogChaos, Log, TEXT("[DEBUG Delta Serialization] [SERVER]     Owner acknowledged delta source STATE frame: %d at index: %d  --  Name: %s"), Frame, LatestAcknowledgedDeltaSourceStateIndex, *AActor::GetDebugName(GetOwner()));
#endif

			break;
		}
	}
}

void UNetworkPhysicsComponent::AddDeltaSourceState()
{
	// Get the data entry for the correct index in the data sources array
	const int32 Index = GetDeltaSourceIndexForFrame(ReplicatedDeltaSourceState.History->GetLatestFrame());
	check(Index <= DeltaSourceStates.Num());
	FNetworkPhysicsData* PhysicsData = DeltaSourceStates[Index].Get();

	// Extract the data from the replicated DeltaSources property
	if (ReplicatedDeltaSourceState.History->ExtractData(ReplicatedDeltaSourceState.History->GetLatestFrame(), /*bResetSolver*/false, PhysicsData, /*bExactFrame*/true))
	{
		// The data is now extracted via PhysicsData and stored inside DeltaSourceStates

		if (!HasServerWorld())
		{
			// On the client, set the latest index (unlike for DeltaSourceInput this latest index is not used on the client since the client doesn't send states towards the server, but set the value for the future)
			LatestAcknowledgedDeltaSourceStateIndex = Index;

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
			UE_LOG(LogChaos, Log, TEXT("[DEBUG Delta Serialization] %s Received delta source STATE for frame: %d at index: %d  --  Name: %s"), (IsLocallyControlled() ? TEXT("[AUTONOMOUS]") : TEXT("[SIMULATED] ")), ReplicatedDeltaSourceState.History->GetLatestFrame(), LatestAcknowledgedDeltaSourceStateIndex, *AActor::GetDebugName(GetOwner()));
#endif

			if (IsLocallyControlled())
			{
				// If this client is the one controlling this entity, send back an acknowledgment to the server we have received the delta source for ServerFrame
				ServerReceiveDeltaSourceStateFrame(PhysicsData->ServerFrame);
			}
		}
		else if (IsLocallyControlled())
		{
			// If server is locally controlled, set the latest index directly, else wait for the owning client to send back ServerReceiveDeltaSourceStateFrame before the server starts to use this 
			LatestAcknowledgedDeltaSourceStateIndex = Index;
		}

#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
		if (HasServerWorld())
		{
			UE_LOG(LogChaos, Log, TEXT("[DEBUG Delta Serialization] [SERVER]     Sent delta source STATE for frame: %d at index: %d  --  Name: %s"), ReplicatedDeltaSourceState.History->GetLatestFrame(), Index, *AActor::GetDebugName(GetOwner()));
		}
#endif

		LatestCachedDeltaSourceStateIndex = Index;
	}
}

FNetworkPhysicsData* UNetworkPhysicsComponent::GetDeltaSourceInput(const int32 Value, const bool bValueIsIndexElseFrame)
{
	TUniquePtr<FNetworkPhysicsData>* DataPtr = nullptr;
	if (Value == -1) // Latest
	{
		DataPtr = &DeltaSourceInputs[LatestAcknowledgedDeltaSourceInputIndex];
	}
	else if (Value == -2) // Default
	{
		DataPtr = &InputDataDefault;
	}
	else if(bValueIsIndexElseFrame)
	{
		if (Value < DeltaSourceInputs.Num())
		{
			DataPtr = &DeltaSourceInputs[Value];
		}
	}
	else // Value is Frame
	{
		int32 Index = GetDeltaSourceIndexForFrame(Value);
		if (Index < DeltaSourceInputs.Num())
		{
			if (DeltaSourceInputs[Index]->ServerFrame == Value)
			{
				DataPtr = &DeltaSourceInputs[Index];
			}
		}
	}

	return DataPtr ? DataPtr->Get() : nullptr;
}

FNetworkPhysicsData* UNetworkPhysicsComponent::GetDeltaSourceState(const int32 Value, const bool bValueIsIndexElseFrame)
{
	TUniquePtr<FNetworkPhysicsData>* DataPtr = nullptr;
	if (Value == -1) // Latest
	{
		DataPtr = &DeltaSourceStates[LatestAcknowledgedDeltaSourceStateIndex];
	}
	else if (Value == -2) // Default
	{
		DataPtr = &StateDataDefault;
	}
	else if (bValueIsIndexElseFrame)
	{
		if (Value < DeltaSourceStates.Num())
		{
			DataPtr = &DeltaSourceStates[Value];
		}
	}
	else // Value is Frame
	{
		int32 Index = GetDeltaSourceIndexForFrame(Value);
		if (Index < DeltaSourceStates.Num())
		{
			if (DeltaSourceStates[Index]->ServerFrame == Value)
			{
				DataPtr = &DeltaSourceStates[Index];
			}
		}
	}

	return DataPtr ? DataPtr->Get() : nullptr;
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedStates()
{
	if (ReplicatedStates.bDeltaSerializationIssue)
	{
#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
		ensureMsgf(!IsLocallyControlled(), TEXT("OnRep_SetReplicatedStates failed delta serialization, should not happen on the owning client unless the pawn just got possessed."));
#endif
		return;
	}

	if (!NetworkPhysicsComponent_Internal || !StateHelper || !ReplicatedStates.History)
	{
		return;
	}

	if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
	{
		if (!AsyncInput->StateData)
		{
			AsyncInput->StateData = StateHelper->CreateUniqueRewindHistory(ReplicatedStates.History->GetHistorySize());
		}

		AsyncInput->StateData->ResetFast();
		AsyncInput->StateData->ResizeDataHistory(ReplicatedStates.History->GetHistorySize(), EAllowShrinking::No);
		ReplicatedStates.History->CopyAllData(*AsyncInput->StateData.Get(), /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true);
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedInputs()
{
	if (ReplicatedInputs.bDeltaSerializationIssue)
	{
#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
		ensureMsgf(!IsLocallyControlled(), TEXT("OnRep_SetReplicatedInputs failed delta serialization, should not happen on the owning client unless the pawn just got possessed."));
#endif
		return;
	}

	if (!NetworkPhysicsComponent_Internal || !InputHelper || !ReplicatedInputs.History)
	{
		return ;
	}

	if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
	{
		if (!AsyncInput->InputData)
		{
			AsyncInput->InputData = InputHelper->CreateUniqueRewindHistory(ReplicatedInputs.History->GetHistorySize());
		}

		AsyncInput->InputData->ResetFast();
		AsyncInput->InputData->ResizeDataHistory(ReplicatedInputs.History->GetHistorySize(), EAllowShrinking::No);
		ReplicatedInputs.History->CopyAllData(*AsyncInput->InputData.Get(), /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true);
	}
}

void UNetworkPhysicsComponent::OnRep_SetReplicatedRemoteInputs()
{
	if (ReplicatedRemoteInputs.bDeltaSerializationIssue)
	{
#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
		ensureMsgf(!IsLocallyControlled(), TEXT("OnRep_SetReplicatedRemoteInputs failed delta serialization, should not happen on the owning client unless the pawn just got possessed."));
#endif
		return;
	}

	if (!NetworkPhysicsComponent_Internal || !InputHelper || !ReplicatedRemoteInputs.History)
	{
		return ;
	}

	if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
	{
		if (!AsyncInput->InputData)
		{
			AsyncInput->InputData = InputHelper->CreateUniqueRewindHistory(ReplicatedRemoteInputs.History->GetHistorySize());
		}

		AsyncInput->InputData->ResetFast();
		AsyncInput->InputData->ResizeDataHistory(ReplicatedRemoteInputs.History->GetHistorySize(), EAllowShrinking::No);
		ReplicatedRemoteInputs.History->CopyAllData(*AsyncInput->InputData.Get(), /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true);
	}
}

void UNetworkPhysicsComponent::ServerReceiveInputData_Implementation(const FNetworkPhysicsRewindDataInputProxy& ClientInputs)
{
	if (ClientInputs.bDeltaSerializationIssue)
	{
		ensureMsgf(false, TEXT("ServerReceiveInputData_Implementation failed delta serialization, should not happen on the server."));
		return;
	}

	if (!NetworkPhysicsComponent_Internal || !InputHelper || !ClientInputs.History)
	{
		return;
	}

	if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
	{
		if (!AsyncInput->InputData)
		{
			AsyncInput->InputData = InputHelper->CreateUniqueRewindHistory(ClientInputs.History->GetHistorySize());
		}

		// Validate data in the received inputs
		if (bValidateDataOnGameThread && ActorComponent.IsValid())
		{
			ClientInputs.History->ValidateDataInHistory(ActorComponent.Get());
		}

		AsyncInput->InputData->ResetFast();
		AsyncInput->InputData->ResizeDataHistory(ClientInputs.History->GetHistorySize(), EAllowShrinking::No);
		AsyncInput->InputData->SetRecordDataIncremental(true); // Only record data that is newer than already cached data
		ClientInputs.History->CopyAllData(*AsyncInput->InputData.Get(), /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true);

		// Send received inputs to remote clients
		ReplicatedRemoteInputs.History->SetRecordDataIncremental(true); // Only record data that is newer than already cached data
		ClientInputs.History->CopyAllData(*ReplicatedRemoteInputs.History, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ true);
		MARK_PROPERTY_DIRTY_FROM_NAME(UNetworkPhysicsComponent, ReplicatedRemoteInputs, this);
	}
}

void UNetworkPhysicsComponent::ServerReceiveImportantInputData_Implementation(const FNetworkPhysicsRewindDataImportantInputProxy& ClientInputs)
{
	if (ClientInputs.bDeltaSerializationIssue)
	{
		ensureMsgf(false, TEXT("ServerReceiveImportantInputData_Implementation failed delta serialization, should not happen on the server."));
		return;
	}

	if (!NetworkPhysicsComponent_Internal || !ClientInputs.History)
	{
		return;
	}

	if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
	{
		// Initialize received data since not all data is networked and when we clone this we expect to have fully initialized data
		ClientInputs.History->Initialize();

		// Validate data in the received inputs
		if (bValidateDataOnGameThread && ActorComponent.IsValid())
		{
			ClientInputs.History->ValidateDataInHistory(ActorComponent.Get());
		}

		// Create new data collection for marshaling
		AsyncInput->InputDataImportant.Add(ClientInputs.History->Clone());
	}
}

void UNetworkPhysicsComponent::MulticastReceiveImportantInputData_Implementation(const FNetworkPhysicsRewindDataImportantInputProxy& ServerInputs)
{
	// Ignore Multicast on server
	if (HasServerWorld())
	{
		return;
	}

	if (ServerInputs.bDeltaSerializationIssue)
	{
#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
		ensureMsgf(!IsLocallyControlled(), TEXT("MulticastReceiveImportantInputData_Implementation failed delta serialization, should not happen on the owning client unless the pawn just got possessed."));
#endif
		return;
	}

	if (!NetworkPhysicsComponent_Internal || !ServerInputs.History)
	{
		return;
	}

	if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
	{
		// Initialize received data since not all data is networked and when we clone this we expect to have fully initialized data
		ServerInputs.History->Initialize();

		// Create new data collection for marshaling
		AsyncInput->InputDataImportant.Add(ServerInputs.History->Clone());
	}
}

void UNetworkPhysicsComponent::MulticastReceiveImportantStateData_Implementation(const FNetworkPhysicsRewindDataImportantStateProxy& ServerStates)
{
	// Ignore Multicast on server
	if (HasServerWorld())
	{
		return;
	}

	if (ServerStates.bDeltaSerializationIssue)
	{
#if DEBUG_NETWORK_PHYSICS_DELTASERIALIZATION
		ensureMsgf(!IsLocallyControlled(), TEXT("MulticastReceiveImportantStateData_Implementation failed delta serialization, should not happen on the owning client unless the pawn just got possessed."));
#endif
		return;
	}

	if (!NetworkPhysicsComponent_Internal || !ServerStates.History)
	{
		return;
	}

	if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
	{
		// Initialize received data since not all data is networked and when we clone this we expect to have fully initialized data
		ServerStates.History->Initialize();

		// Create new data collection for marshaling
		AsyncInput->StateDataImportant.Add(ServerStates.History->Clone());
	}
}

/** DEPRECATED UE 5.5 */
const float UNetworkPhysicsComponent::GetCurrentInputDecay(FNetworkPhysicsData* PhysicsData)
{
	if (!PhysicsData)
	{
		return 0.0f;
	}

	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
	if (!PhysScene)
	{
		return 0.0f;
	}

	Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver();
	if (!Solver)
	{
		return 0.0f;
	}

	Chaos::FRewindData* RewindData = Solver->GetRewindData();
	if (!RewindData)
	{
		return 0.0f;
	}
	
	const float NumPredictedInputs = RewindData->CurrentFrame() - PhysicsData->LocalFrame; // Number of frames we have used the same PhysicsData for during resim
	const float MaxPredictedInputs = RewindData->GetLatestFrame() - 1 - PhysicsData->LocalFrame; // Max number of frames PhysicsData registered frame until end of resim

	// Linear decay
	const float PredictionAlpha = MaxPredictedInputs > 0 ? (NumPredictedInputs / MaxPredictedInputs) : 0.0f;

	return PredictionAlpha;
}

bool UNetworkPhysicsComponent::HasServerWorld() const
{
	Chaos::EnsureIsInGameThreadContext();
	return GetWorld()->IsNetMode(NM_DedicatedServer) || GetWorld()->IsNetMode(NM_ListenServer);
}

bool UNetworkPhysicsComponent::IsLocallyControlled() const
{
	Chaos::EnsureIsInGameThreadContext();
	if (bIsRelayingLocalInputs)
	{
		return true;
	}

	if (APlayerController* PlayerController = GetPlayerController())
	{
		return PlayerController->IsLocalController();
	}
	return false;
}

bool UNetworkPhysicsComponent::IsNetworkPhysicsTickOffsetAssigned() const
{
	Chaos::EnsureIsInGameThreadContext();
	if (APlayerController* PlayerController = GetPlayerController())
	{
		return PlayerController->GetNetworkPhysicsTickOffsetAssigned();
	}
	return false;
}

void UNetworkPhysicsComponent::SetCompareStateToTriggerRewind(const bool bInCompareStateToTriggerRewind, const bool bInIncludeSimProxies)
{
	bCompareStateToTriggerRewind = bInCompareStateToTriggerRewind;
	bCompareStateToTriggerRewindIncludeSimProxies = bInIncludeSimProxies;
	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			AsyncInput->bCompareStateToTriggerRewind = bCompareStateToTriggerRewind;
			AsyncInput->bCompareStateToTriggerRewindIncludeSimProxies = bInIncludeSimProxies;
		}
	}
}

void UNetworkPhysicsComponent::SetCompareInputToTriggerRewind(const bool bInCompareInputToTriggerRewind)
{
	bCompareInputToTriggerRewind = bInCompareInputToTriggerRewind;
	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			AsyncInput->bCompareInputToTriggerRewind = bCompareInputToTriggerRewind;
		}
	}
}

APlayerController* UNetworkPhysicsComponent::GetPlayerController() const
{
	Chaos::EnsureIsInGameThreadContext();
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		return PC;
	}

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController * PC = Pawn->GetController<APlayerController>())
		{
			return PC;
		}

		// In this case the APlayerController can be found as the owner of the pawn
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetOwner()))
		{
			return PC;
		}
	}

	return nullptr;
}

void UNetworkPhysicsComponent::SetPhysicsObject(Chaos::FConstPhysicsObjectHandle InPhysicsObject)
{
	if (PhysicsObject == InPhysicsObject)
	{
		return;
	}

	PhysicsObject = InPhysicsObject;

	// Marshal data from Game Thread to Physics Thread
	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			AsyncInput->PhysicsObject = InPhysicsObject;
		}
	}
}

void UNetworkPhysicsComponent::UpdateAsyncComponent(const bool bFullUpdate)
{
	// Marshal data from Game Thread to Physics Thread
	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			if (!HasServerWorld())
			{
				if (UWorld* World = GetWorld())
				{
					if (APlayerController* PC = World->GetFirstPlayerController())
					{
						AsyncInput->NetworkPhysicsTickOffset = PC->GetNetworkPhysicsTickOffset();
					}
				}
				AsyncInput->InputsToNetwork_Owner = InputsToNetwork_Owner;
			}

			// bIsLocallyControlled is marshaled outside of the bFullUpdate because it's not always set when last bFullUpdate is called.
			AsyncInput->bIsLocallyControlled = IsLocallyControlled();

			if (bFullUpdate)
			{
				if (UWorld* World = GetWorld())
				{ 
					AsyncInput->NetMode = World->GetNetMode();
				}

				if (AActor* Owner = GetOwner())
				{ 
					AsyncInput->NetRole = Owner->GetLocalRole();
					AsyncInput->PhysicsReplicationMode = Owner->GetPhysicsReplicationMode();
					AsyncInput->ActorName = AActor::GetDebugName(Owner);
			
					UNetworkPhysicsSettingsComponent* SettingsComponent = Owner->FindComponentByClass<UNetworkPhysicsSettingsComponent>();
					if (SettingsComponent && SettingsComponent->GetNetworkPhysicsSettings_Internal())
					{
						AsyncInput->SettingsComponent = SettingsComponent->GetNetworkPhysicsSettings_Internal();
					}
				}
				
				if (ActorComponent.IsValid())
				{
					AsyncInput->ActorComponent = ActorComponent.Get();
				}
			}
		}
	}
}

void UNetworkPhysicsComponent::CreateAsyncDataHistory()
{
	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			AsyncInput->ActorComponent = ActorComponent.Get();

			if (InputHelper)
			{
				// Marshal the input helper to create both input data and input history on the physics thread
				AsyncInput->InputHelper = InputHelper->Clone();
			}

			if (StateHelper)
			{
				// Marshal the state helper to create both state data and state history on the physics thread
				AsyncInput->StateHelper = StateHelper->Clone();
			}
		}
	}
}

void UNetworkPhysicsComponent::RemoveDataHistory()
{
	// Tell the async network physics component to unregister from RewindData
	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			AsyncInput->bUnregisterDataHistoryFromRewindData = true;
		}
	}
}

void UNetworkPhysicsComponent::AddDataHistory()
{
	// Tell the async network physics component to register in RewindData
	if (NetworkPhysicsComponent_Internal)
	{
		if (FAsyncNetworkPhysicsComponentInput* AsyncInput = NetworkPhysicsComponent_Internal->GetProducerInputData_External())
		{
			AsyncInput->bRegisterDataHistoryInRewindData = true;
		}
	}
}

TSharedPtr<Chaos::FBaseRewindHistory>& UNetworkPhysicsComponent::GetStateHistory_Internal()
{
	if (NetworkPhysicsComponent_Internal)
	{
		return NetworkPhysicsComponent_Internal->StateHistory;
	}
	return StateHistory;
}

TSharedPtr<Chaos::FBaseRewindHistory>& UNetworkPhysicsComponent::GetInputHistory_Internal()
{
	if (NetworkPhysicsComponent_Internal)
	{
		return NetworkPhysicsComponent_Internal->InputHistory;
	}
	return InputHistory;
}


// --------------------------- Async Network Physics Component ---------------------------

// Initialize static
const FNetworkPhysicsSettingsNetworkPhysicsComponent FAsyncNetworkPhysicsComponent::SettingsNetworkPhysicsComponent_Default = FNetworkPhysicsSettingsNetworkPhysicsComponent();

FAsyncNetworkPhysicsComponent::FAsyncNetworkPhysicsComponent() : TSimCallbackObject()
	, bIsLocallyControlled(true)
	, NetMode(ENetMode::NM_Standalone)
	, NetRole(ENetRole::ROLE_Authority)
	, NetworkPhysicsTickOffset(0)
	, PhysicsReplicationMode(EPhysicsReplicationMode::Default)
	, SettingsComponent(nullptr)
	, ActorComponent(nullptr)
	, PhysicsObject(nullptr)
	, bCompareStateToTriggerRewind(false)
	, bCompareStateToTriggerRewindIncludeSimProxies(false)
	, bCompareInputToTriggerRewind(false)
{
}

void FAsyncNetworkPhysicsComponent::OnInitialize_Internal()
{
	if (Chaos::FPhysicsSolverBase* BaseSolver = GetSolver())
	{
		if (BaseSolver->IsNetworkPhysicsPredictionEnabled())
		{
			// Register for Pre- and Post- ProcessInputs_Internal callbacks
			if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(BaseSolver->GetRewindCallback()))
			{
				DelegateOnPreProcessInputs_Internal = SolverCallback->PreProcessInputsInternal.AddRaw(this, &FAsyncNetworkPhysicsComponent::OnPreProcessInputs_Internal);
				DelegateOnPostProcessInputs_Internal = SolverCallback->PostProcessInputsInternal.AddRaw(this, &FAsyncNetworkPhysicsComponent::OnPostProcessInputs_Internal);
			}
		}
		else
		{
			UE_LOG(LogChaos, Warning, TEXT("A NetworkPhysicsComponent is trying to set up but 'Project Settings -> Physics -> Physics Prediction' is not enabled. The component might not work as intended."));
		}
	}
}

void FAsyncNetworkPhysicsComponent::OnUninitialize_Internal()
{
	if (Chaos::FPhysicsSolverBase* BaseSolver = GetSolver())
	{
		// Unregister for Pre- and Post- ProcessInputs_Internal callbacks
		if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(BaseSolver->GetRewindCallback()))
		{
			SolverCallback->PreProcessInputsInternal.Remove(DelegateOnPreProcessInputs_Internal);
			DelegateOnPreProcessInputs_Internal.Reset();

			SolverCallback->PostProcessInputsInternal.Remove(DelegateOnPostProcessInputs_Internal);
			DelegateOnPostProcessInputs_Internal.Reset();
		}
	}

	UnregisterDataHistoryFromRewindData();
}

void FAsyncNetworkPhysicsComponent::OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle InPhysicsObject)
{
	if (PhysicsObject == InPhysicsObject)
	{
		UnregisterDataHistoryFromRewindData();
		PhysicsObject = nullptr;
	}
}

const FNetworkPhysicsSettingsNetworkPhysicsComponent& FAsyncNetworkPhysicsComponent::GetComponentSettings()
{
	return SettingsComponent ? SettingsComponent->Settings.NetworkPhysicsComponentSettings : SettingsNetworkPhysicsComponent_Default;
};

void FAsyncNetworkPhysicsComponent::ConsumeAsyncInput(const int32 PhysicsStep)
{
	if (const FAsyncNetworkPhysicsComponentInput* AsyncInput = GetConsumerInput_Internal())
	{
		const FNetworkPhysicsSettingsNetworkPhysicsComponent& ComponentSettings = GetComponentSettings();

		/** Onetime setup marshaled data */
		{
			if (AsyncInput->SettingsComponent.IsSet())
			{
				SettingsComponent = *AsyncInput->SettingsComponent;
			}
			if (AsyncInput->bIsLocallyControlled.IsSet())
			{
				bIsLocallyControlled = *AsyncInput->bIsLocallyControlled;
			}
			if (AsyncInput->NetMode.IsSet())
			{
				NetMode = *AsyncInput->NetMode;
			}
			if (AsyncInput->NetRole.IsSet())
			{
				NetRole = *AsyncInput->NetRole;
			}
			if (AsyncInput->NetworkPhysicsTickOffset.IsSet())
			{
				NetworkPhysicsTickOffset = *AsyncInput->NetworkPhysicsTickOffset;
			}
			if (AsyncInput->InputsToNetwork_Owner.IsSet())
			{
				// Only marshaled from GT to PT on the client
				InputsToNetwork_Owner = *AsyncInput->InputsToNetwork_Owner;
			}
			if (AsyncInput->PhysicsReplicationMode.IsSet())
			{
				PhysicsReplicationMode = *AsyncInput->PhysicsReplicationMode;
			}
			if (AsyncInput->ActorComponent.IsSet())
			{
				ActorComponent = *AsyncInput->ActorComponent;
			}
			if (AsyncInput->PhysicsObject.IsSet())
			{
				if (PhysicsObject == nullptr || PhysicsObject != *AsyncInput->PhysicsObject)
				{
					PhysicsObject = *AsyncInput->PhysicsObject;
					RegisterDataHistoryInRewindData();
				}
			}
			if (AsyncInput->ActorName.IsSet())
			{
				ActorName = *AsyncInput->ActorName;
			}
			if (AsyncInput->bRegisterDataHistoryInRewindData.IsSet())
			{
				RegisterDataHistoryInRewindData();
			}
			if (AsyncInput->bUnregisterDataHistoryFromRewindData.IsSet())
			{
				UnregisterDataHistoryFromRewindData();
			}
			if (AsyncInput->bCompareStateToTriggerRewind.IsSet())
			{
				bCompareStateToTriggerRewind = *AsyncInput->bCompareStateToTriggerRewind;
			}
			if (AsyncInput->bCompareStateToTriggerRewind.IsSet())
			{
				bCompareStateToTriggerRewindIncludeSimProxies = *AsyncInput->bCompareStateToTriggerRewindIncludeSimProxies;
			}
			if (AsyncInput->bCompareInputToTriggerRewind.IsSet())
			{
				bCompareInputToTriggerRewind = *AsyncInput->bCompareInputToTriggerRewind;
			}
			if (AsyncInput->InputHelper.IsSet())
			{
				// Setup rewind data if not already done, and get history size
				const int32 NumFrames = SetupRewindData();

				// Create input history and local data properties
				InputData = (*AsyncInput->InputHelper)->CreateUniqueData();
				LatestInputReceiveData = (*AsyncInput->InputHelper)->CreateUniqueData();
				InputHistory = MakeShareable((*AsyncInput->InputHelper)->CreateUniqueRewindHistory(NumFrames).Release());
				RegisterDataHistoryInRewindData();
			}
			if (AsyncInput->StateHelper.IsSet())
			{
				// Setup rewind data if not already done, and get history size
				const int32 NumFrames = SetupRewindData();

				// Create state history and local property
				StateData = (*AsyncInput->StateHelper)->CreateUniqueData();
				StateHistory = MakeShareable((*AsyncInput->StateHelper)->CreateUniqueRewindHistory(NumFrames).Release());
				RegisterDataHistoryInRewindData();
			}
		}

		/** Continuously marshaled data */
		{
			const bool bIsServer = IsServer();

			/** Receive data helper */
			auto ReceiveHelper = [&](Chaos::FBaseRewindHistory* History, Chaos::FBaseRewindHistory* ReceiveData, const bool bImportant, const bool bCompareData)
			{
				const bool bCompareDataForRewind = bCompareData && !bIsServer;
				const int32 ResimFrame = History->ReceiveNewData(*ReceiveData, (bIsServer ? 0 : NetworkPhysicsTickOffset), bCompareDataForRewind, bImportant);
				if (bCompareDataForRewind)
				{
					TriggerResimulation(ResimFrame);
				}

#if DEBUG_NETWORK_PHYSICS
				{
					FString NetRoleString = bIsServer ? FString("SERVER") : (IsLocallyControlled() ? FString("AUTONO") : FString("PROXY "));
					ReceiveData->DebugData(FString::Printf(TEXT("%s | PT | RECEIVE DATA | LatestFrame: %d | bImportant: %d | Name: %s"), *NetRoleString, ReceiveData->GetLatestFrame(), bImportant, *GetActorName()));
				}
#endif

				// Reset the received data after having consumed it
				ReceiveData->ResetFast();
			};

			const bool bCompareInput = ComponentSettings.GetCompareInputToTriggerRewind(bCompareInputToTriggerRewind) && IsLocallyControlled();
			const bool bCompareState = ComponentSettings.GetCompareStateToTriggerRewind(bCompareStateToTriggerRewind) && (IsLocallyControlled() || ComponentSettings.GetCompareStateToTriggerRewindIncludeSimProxies(bCompareStateToTriggerRewindIncludeSimProxies));

			// Receive Inputs
			if (AsyncInput->InputData && AsyncInput->InputData->HasDataInHistory())
			{
				/* Extract latest received input from client on the server, to be used if input buffer runs empty
				* TODO, improve flow to not require this before ReceiveHelper */
				FNetworkPhysicsData* PhysicsData = nullptr;
				if (bIsServer && LatestInputReceiveData)
				{
					PhysicsData = LatestInputReceiveData.Get();
					if (AsyncInput->InputData->ExtractData(AsyncInput->InputData->GetLatestFrame(), false, PhysicsData, true) == false)
					{
						// Extraction failed
						ensureMsgf(false, TEXT("Failed to extract latest input data from received inputs"));
						PhysicsData = nullptr;

#if DEBUG_NETWORK_PHYSICS
						AsyncInput->InputData->DebugData(FString::Printf(TEXT("SERVER | PT | Failed to extract LatestInputReceiveData | LatestFrame: %d | Name: %s"), AsyncInput->InputData->GetLatestFrame(), *GetActorName()));
#endif
					}
				}

				// Validate data in the received inputs on the server
				if (!ComponentSettings.GetValidateDataOnGameThread() && bIsServer && ActorComponent.IsValid() && ActorComponent.Get()->IsBeingDestroyed() == false)
				{
					AsyncInput->InputData->ValidateDataInHistory(ActorComponent.Get());
				}
				ReceiveHelper(InputHistory.Get(), AsyncInput->InputData.Get(), /*bImportant*/false, bCompareInput);

				/* If the server-side input history doesn't have any entries ahead of the current physics tick, the input buffer is empty, inject the latest received input as the input for the current tick.
				* This happens during a desync where the client is far behind the server */
				if (bIsServer && InputHistory->GetLatestFrame() < PhysicsStep && PhysicsData)
				{
#if DEBUG_NETWORK_PHYSICS
					UE_LOG(LogChaos, Log, TEXT("SERVER | PT | Input Buffer Empty, Injecting Received Input at frame %d || LocalFrame = %d || ServerFrame = %d || bDataAltered = %d || Data: %s || Actor: %s")
						, PhysicsStep, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->bDataAltered, *PhysicsData->DebugData(), *GetActorName());
#endif
					
					// Record data in InputHistory
					PhysicsData->LocalFrame = PhysicsStep;
					PhysicsData->ServerFrame = PhysicsStep;
					InputHistory->RecordData(PhysicsStep, PhysicsData);
				}

			}

			// Receive States
			if (AsyncInput->StateData && AsyncInput->StateData->HasDataInHistory())
			{
				ReceiveHelper(StateHistory.Get(), AsyncInput->StateData.Get(), /*bImportant*/false, bCompareState);
			}

			// Receive Important Inputs
			for (const TUniquePtr<Chaos::FBaseRewindHistory>& InputImportant : AsyncInput->InputDataImportant)
			{
				if (!InputImportant || !InputImportant->HasDataInHistory())
				{
					continue;
				}
				ReceiveHelper(InputHistory.Get(), InputImportant.Get(), /*bImportant*/true, bCompareInput);
			}

			// Receive Important States
			for (const TUniquePtr<Chaos::FBaseRewindHistory>& StateImportant : AsyncInput->StateDataImportant)
			{
				if (!StateImportant || !StateImportant->HasDataInHistory())
				{
					continue;
				}
				ReceiveHelper(StateHistory.Get(), StateImportant.Get(), /*bImportant*/true, bCompareState);
			}
		}
	}
}

FAsyncNetworkPhysicsComponentOutput& FAsyncNetworkPhysicsComponent::GetAsyncOutput_Internal()
{
	FAsyncNetworkPhysicsComponentOutput& AsyncOutput = GetProducerOutputData_Internal();

	// InputData marshal from PT to GT is needed for: LocallyControlled and Server
	if ((IsLocallyControlled() || IsServer()) && AsyncOutput.InputData == nullptr && InputHistory != nullptr)
	{
		AsyncOutput.InputData = InputHistory->CreateNew();
	}

	// StateData marshal from PT to GT is needed for: Server
	if (IsServer() && AsyncOutput.StateData == nullptr && StateHistory != nullptr)
	{
		AsyncOutput.StateData = StateHistory->CreateNew();
		AsyncOutput.StateData->ResizeDataHistory(StatesToNetwork);
	}

	return AsyncOutput;
}

void FAsyncNetworkPhysicsComponent::OnPreProcessInputs_Internal(const int32 PhysicsStep)
{
	ConsumeAsyncInput(PhysicsStep);

	const FNetworkPhysicsSettingsNetworkPhysicsComponent& ComponentSettings = GetComponentSettings();
	const bool bIsServer = IsServer();

	bool bIsSolverReset = false;
	bool bIsSolverResim = false;
	if (Chaos::FPBDRigidsEvolution* Evolution = GetEvolution())
	{
		bIsSolverResim = Evolution->IsResimming();
		bIsSolverReset = Evolution->IsResetting();
	}

#if DEBUG_NETWORK_PHYSICS
	{
		const int32 InputBufferSize = (bIsServer && InputHistory) ? (InputHistory->GetLatestFrame() - PhysicsStep) : 0;
		const FString NetRoleString = bIsServer ? FString("SERVER") : (IsLocallyControlled() ? FString("AUTONO") : FString("PROXY "));
		UE_LOG(LogChaos, Log, TEXT("%s | PT | OnPreProcessInputsInternal | At Frame %d | IsResim: %d | FirstResimFrame: %d | InputBuffer: %d | Name = %s"), *NetRoleString, PhysicsStep, bIsSolverResim, bIsSolverReset, InputBufferSize, *GetActorName());
	}
#endif

	if (ActorComponent.IsValid() && ActorComponent.Get()->IsBeingDestroyed() == false)
	{
		// Apply replicated state on clients if we are resimulating or on simulated proxies if setting is enabled
		const bool bApplySimProxyState = ComponentSettings.GetApplySimProxyStateAtRuntime() && !bIsServer && !IsLocallyControlled();
		if ((bApplySimProxyState || bIsSolverResim) && StateHistory && StateData)
		{
			FNetworkPhysicsData* PhysicsData = StateData.Get();
			PhysicsData->LocalFrame = PhysicsStep;
			const bool bExactFrame = PhysicsReplicationCVars::ResimulationCVars::bAllowRewindToClosestState ? !bIsSolverReset : true;
			if (StateHistory->ExtractData(PhysicsStep, bIsSolverReset, PhysicsData, (bExactFrame && bIsSolverResim)) && PhysicsData->bReceivedData)
			{
				PhysicsData->ApplyData(ActorComponent.Get());
#if DEBUG_NETWORK_PHYSICS
				UE_LOG(LogChaos, Log, TEXT("			Applying extracted state from history | bExactFrame = %d | LocalFrame = %d | ServerFrame = %d | bDataAltered = %d | Data: %s")
					, bExactFrame, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->bDataAltered, *PhysicsData->DebugData());
#endif
			}
#if DEBUG_NETWORK_PHYSICS
			else if (PhysicsStep <= StateHistory->GetLatestFrame())
			{
				UE_LOG(LogChaos, Log, TEXT("		Non-Determinism: FAILED to extract and apply state from history | bExactFrame = %d | -- Printing history --"), bExactFrame);
				StateHistory->DebugData(FString::Printf(TEXT("StateHistory | Component = %s"), *GetActorName()));
			}
#endif
		}

		// Apply replicated inputs on server and simulated proxies if setting is enabled, and on local player if we are resimulating
		const bool bApplySimProxyInput = ComponentSettings.GetApplySimProxyInputAtRuntime() && !bIsServer && !IsLocallyControlled();
		const bool bApplyServerInput = bIsServer && !IsLocallyControlled();
		if ((bApplyServerInput || bApplySimProxyInput || bIsSolverResim) && InputHistory && InputData)
		{
			FNetworkPhysicsData* PhysicsData = InputData.Get();
			int32 NextExpectedLocalFrame = PhysicsData->LocalFrame + 1;

			// There are important inputs earlier than upcoming input to apply
			if (NewImportantInputFrame < NextExpectedLocalFrame && !bIsSolverResim)
			{
				if (ComponentSettings.GetApplyDataInsteadOfMergeData())
				{
#if DEBUG_NETWORK_PHYSICS
					UE_LOG(LogChaos, Log, TEXT("		Non-Determinism: Reapplying multiple data due to receiving an important data that was previously missed. FromFrame: %d | ToFrame: %d | IsLocallyControlled = %d"), NewImportantInputFrame, (NextExpectedLocalFrame - 1), IsLocallyControlled());
#endif
					// Apply all inputs in range
					InputHistory->ApplyDataRange(NewImportantInputFrame, NextExpectedLocalFrame - 1, ActorComponent.Get(), /*bOnlyImportant*/false);
				}
				else
				{
					// Merge all inputs from earliest new important
					NextExpectedLocalFrame = NewImportantInputFrame;
#if DEBUG_NETWORK_PHYSICS
					UE_LOG(LogChaos, Log, TEXT("		Non-Determinism: Prepare to reapply multiple data through MergeData due to receiving an important data that was previously missed. FromFrame: %d | ToFrame: %d | IsLocallyControlled = %d"), NewImportantInputFrame, (NextExpectedLocalFrame - 1), IsLocallyControlled());
#endif
				}
			}

			if (InputHistory->ExtractData(PhysicsStep, bIsSolverReset, PhysicsData, /*bExactFrame*/(ComponentSettings.GetAllowInputExtrapolation() == false)))
			{
				// Calculate input decay if we are resimulating and we don't have up to date inputs
				if (bIsSolverResim)
				{
					if (PhysicsData->LocalFrame < PhysicsStep)
					{
						const float InputDecay = GetCurrentInputDecay(PhysicsData);
						PhysicsData->DecayData(InputDecay);
					}
				}
				// Check if we have a gap between last used input and current input
				else if (PhysicsData->LocalFrame > NextExpectedLocalFrame)
				{
					if (ComponentSettings.GetApplyDataInsteadOfMergeData())
					{
#if DEBUG_NETWORK_PHYSICS
						UE_LOG(LogChaos, Log, TEXT("		Non-Determinism: Applying multiple data instead of merging, from LocalFrame %d into LocalFrame %d | IsLocallyControlled = %d"), NextExpectedLocalFrame, PhysicsData->LocalFrame, IsLocallyControlled());
#endif
						// Iterate over each input and call ApplyData, except on the last, it will get handled by the normal ApplyData call further down
						const int32 LastFrame = PhysicsData->LocalFrame;
						for (; NextExpectedLocalFrame <= LastFrame; NextExpectedLocalFrame++)
						{
							if (InputHistory->ExtractData(NextExpectedLocalFrame, bIsSolverReset, PhysicsData, true) && NextExpectedLocalFrame < LastFrame)
							{
								PhysicsData->ApplyData(ActorComponent.Get());
							}
						}
					}
					else
					{
#if DEBUG_NETWORK_PHYSICS
						UE_LOG(LogChaos, Log, TEXT("		Non-Determinism: Merging inputs from LocalFrame %d into LocalFrame %d | IsLocallyControlled = %d"), NextExpectedLocalFrame, PhysicsData->LocalFrame, IsLocallyControlled());
#endif
						// Merge all inputs since last used input
						InputHistory->MergeData(NextExpectedLocalFrame, PhysicsData);
					}
				}

				// If the extracted input data was altered (extrapolated, interpolated) on the server, record it into the history for it to get replicated to clients
				if (bIsServer && PhysicsData->bDataAltered)
				{
					// Check that buffer is not empty
					if (PhysicsStep <= InputHistory->GetLatestFrame())
					{
						if (PhysicsData->LocalFrame < PhysicsStep) 
						{
							// A missing input was detected and buffer is not empty, inform the owning client to send more inputs in each RPC to not get a gaps in the buffer
							// NOTE: We don't send more extra inputs when the buffer runs empty since that case is corrected via time dilation, not sending extra inputs
							MissingInputCount++;
						}
					}
#if DEBUG_NETWORK_PHYSICS
					else
					{
						UE_LOG(LogChaos, Log, TEXT("		Non-Determinism: Input buffer Empty, input for frame %d was extrapolated from frame: %d"), PhysicsStep, PhysicsData->LocalFrame);
					}
#endif

					PhysicsData->bReceivedData = true; // Mark the input data as received so that it doesn't get overwritten by incoming client inputs
					PhysicsData->bImportant = false;
					PhysicsData->LocalFrame = PhysicsStep;
					InputHistory->RecordData(PhysicsStep, PhysicsData);
				}

				PhysicsData->ApplyData(ActorComponent.Get());

#if DEBUG_NETWORK_PHYSICS
				{
					UE_LOG(LogChaos, Log, TEXT("			Applying extracted input from history | LocalFrame = %d | ServerFrame = %d | bDataAltered = %d | IsResim = %d | IsLocallyControlled = %d | InputDecay = %f | Data: %s")
						, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->bDataAltered, bIsSolverResim, IsLocallyControlled(), GetCurrentInputDecay(PhysicsData), *PhysicsData->DebugData());
				}
#endif
			}
#if DEBUG_NETWORK_PHYSICS
			else if (PhysicsStep <= InputHistory->GetLatestFrame())
			{
				UE_LOG(LogChaos, Log, TEXT("		Non-Determinism: FAILED to extract and apply input from history | IsResim = %d | IsLocallyControlled = %d | -- Printing history --"), bIsSolverResim, IsLocallyControlled());
				InputHistory->DebugData(FString::Printf(TEXT("InputHistory | Name = %s"), *GetActorName()));
			}
#endif
		}
	}
	NewImportantInputFrame = INT_MAX;
}

void FAsyncNetworkPhysicsComponent::OnPostProcessInputs_Internal(const int32 PhysicsStep)
{
	const FNetworkPhysicsSettingsNetworkPhysicsComponent& ComponentSettings = GetComponentSettings();
	const bool bIsServer = IsServer();

	bool bIsSolverReset = false;
	bool bIsSolverResim = false;
	if (Chaos::FPBDRigidsEvolution* Evolution = GetEvolution())
	{
		bIsSolverResim = Evolution->IsResimming();
		bIsSolverReset = Evolution->IsResetting();
	}

#if DEBUG_NETWORK_PHYSICS
	{
		FString NetRoleString = bIsServer ? FString("SERVER") : (IsLocallyControlled() ? FString("AUTONO") : FString("PROXY "));
		UE_LOG(LogChaos, Log, TEXT("%s | PT | OnPostProcessInputsInternal | At Frame %d | IsResim: %d | FirstResimFrame: %d | Name = %s"), *NetRoleString, PhysicsStep, bIsSolverResim, bIsSolverReset, *GetActorName());
	}
#endif

	if (ActorComponent.IsValid() && ActorComponent.Get()->IsBeingDestroyed() == false)
	{
		// Cache current input if we are locally controlled
		const bool bShouldCacheInputHistory = IsLocallyControlled() && !bIsSolverResim;
		if (bShouldCacheInputHistory && (InputData != nullptr))
		{
			// Prepare to gather input data
			FNetworkPhysicsData* PhysicsData = InputData.Get();
			PhysicsData->PrepareFrame(PhysicsStep, bIsServer, GetNetworkPhysicsTickOffset());

			// Gather input data from implementation
			PhysicsData->BuildData(ActorComponent.Get());

			// Record input in history
			InputHistory->RecordData(PhysicsStep, PhysicsData);

#if DEBUG_NETWORK_PHYSICS
			{
				UE_LOG(LogChaos, Log, TEXT("		Recording input into history | LocalFrame = %d | ServerFrame = %d | bDataAltered = %d | Input: %s ")
					, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->bDataAltered, *PhysicsData->DebugData());
			}
#endif
		}

		// Cache current state if this is the server or we are comparing predicted states clients
		const bool bShouldCacheStateHistory = bIsServer
			|| (ComponentSettings.GetCompareStateToTriggerRewind(bCompareStateToTriggerRewind) && (IsLocallyControlled() || ComponentSettings.GetCompareStateToTriggerRewindIncludeSimProxies(bCompareStateToTriggerRewindIncludeSimProxies)));

		if (StateHistory && StateData && bShouldCacheStateHistory)
		{
			// Prepare to gather state data
			FNetworkPhysicsData* PhysicsData = StateData.Get();
			PhysicsData->PrepareFrame(PhysicsStep, bIsServer, GetNetworkPhysicsTickOffset());

			// Gather input data from implementation
			PhysicsData->BuildData(ActorComponent.Get());

			// Record input in history
			StateHistory->RecordData(PhysicsStep, PhysicsData);

#if DEBUG_NETWORK_PHYSICS
			{
				UE_LOG(LogChaos, Log, TEXT("		Recording state into history | LocalFrame = %d | ServerFrame = %d | bDataAltered = %d | State: %s ")
					, PhysicsData->LocalFrame, PhysicsData->ServerFrame, PhysicsData->bDataAltered, *PhysicsData->DebugData());
			}
#endif
		}
	}

	// Marshal inputs and states from PT to GT for networking
	FAsyncNetworkPhysicsComponentOutput& AsyncOutput = GetAsyncOutput_Internal();
	SendInputData_Internal(AsyncOutput, PhysicsStep);
	SendStateData_Internal(AsyncOutput, PhysicsStep);
	FinalizeOutputData_Internal();
}

void FAsyncNetworkPhysicsComponent::SendInputData_Internal(FAsyncNetworkPhysicsComponentOutput& AsyncOutput, const int32 PhysicsStep)
{
	const bool bIsServer = IsServer();

	if (bIsServer)
	{
		UpdateDynamicInputScaling();
		AsyncOutput.InputsToNetwork_Owner = InputsToNetwork_Owner;
	}

	// Inputs are sent from the server or locally controlled actors/pawns
	if (AsyncOutput.InputData && InputHistory && (IsLocallyControlled() || bIsServer))
	{
		const FNetworkPhysicsSettingsNetworkPhysicsComponent& ComponentSettings = GetComponentSettings();

		// Send latest N frames from history
		const int32 ToFrame = FMath::Max(0, PhysicsStep);

		// -- Default / Unreliable Flow --
		if (ComponentSettings.GetEnableUnreliableFlow())
		{
			const uint16 NumInputsToNetwork = bIsServer ? InputsToNetwork_Simulated : InputsToNetwork_Owner;
			const int32 FromFrame = FMath::Max(0, ToFrame - NumInputsToNetwork - 1); // Remove 1 since both ToFrame and FromFrame are inclusive

			AsyncOutput.InputData->ResizeDataHistory(NumInputsToNetwork);

			if (InputHistory->CopyData(*AsyncOutput.InputData, FromFrame, ToFrame, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ ComponentSettings.GetEnableReliableFlow() == false))
			{
#if DEBUG_NETWORK_PHYSICS
				{
					const int32 LocalFrame = GetRigidSolver()->GetCurrentFrame();
					const int32 ServerFrame = bIsServer ? LocalFrame : LocalFrame + GetNetworkPhysicsTickOffset();
					FString NetRoleString = bIsServer ? FString("SERVER") : (IsLocallyControlled() ? FString("AUTONO") : FString("PROXY "));
					AsyncOutput.InputData->DebugData(FString::Printf(TEXT("%s | PT | SendInputData_Internal | UNRELIABLE | CurrentLocalFrame = %d | CurrentServerFrame = %d | Name: %s"), *NetRoleString, LocalFrame, ServerFrame, *GetActorName()));
				}
#endif
			}
		}

		// -- Important / Reliable flow --
		if (ComponentSettings.GetEnableReliableFlow())
		{
			/* Get the latest valid frame that can hold new important data:
			* 1. Frame after last time we called SendInputData
			* 2. Earliest possible frame in history */
			const int32 FromFrame = FMath::Max(LastInputSendFrame + 1, ToFrame - InputHistory->GetHistorySize());

			// Check if we have important data to marshal
			const int32 Count = InputHistory->CountValidData(FromFrame, ToFrame, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true);
			if (Count > 0)
			{
				// Create new data collection for marshaling
				const int32 Idx = AsyncOutput.InputDataImportant.Add(InputHistory->CreateNew());
				AsyncOutput.InputDataImportant[Idx]->ResizeDataHistory(Count);

				// Copy over data
				if (InputHistory->CopyData(*AsyncOutput.InputDataImportant[Idx], FromFrame, ToFrame, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true))
				{
#if DEBUG_NETWORK_PHYSICS
					{
						const int32 LocalFrame = GetRigidSolver()->GetCurrentFrame();
						const int32 ServerFrame = bIsServer ? LocalFrame : LocalFrame + GetNetworkPhysicsTickOffset();
						FString NetRoleString = bIsServer ? FString("SERVER") : (IsLocallyControlled() ? FString("AUTONO") : FString("PROXY "));
						AsyncOutput.InputDataImportant[Idx]->DebugData(FString::Printf(TEXT("%s | PT | SendInputData_Internal | RELIABLE | CurrentLocalFrame = %d | CurrentServerFrame = %d | Name: %s"), *NetRoleString, LocalFrame, ServerFrame, *GetActorName()));
					}
#endif
				}
				
			}
		}
		LastInputSendFrame = InputHistory->GetLatestFrame();
	}
}

void FAsyncNetworkPhysicsComponent::SendStateData_Internal(FAsyncNetworkPhysicsComponentOutput& AsyncOutput, const int32 PhysicsStep)
{
	if (IsServer() && StateHistory && AsyncOutput.StateData)
	{
		const FNetworkPhysicsSettingsNetworkPhysicsComponent& ComponentSettings = GetComponentSettings();

		// Send latest N frames from history
		const int32 ToFrame = FMath::Max(0, PhysicsStep);

		// -- Default / Unreliable Flow --
		if (ComponentSettings.GetEnableUnreliableFlow())
		{
			const int32 FromFrame = FMath::Max(0, ToFrame - StatesToNetwork - 1); // Remove 1 since both ToFrame and FromFrame are inclusive

			// Resize marshaling history if needed
			AsyncOutput.StateData->ResizeDataHistory(StatesToNetwork);

			if (StateHistory->CopyData(*AsyncOutput.StateData, FromFrame, ToFrame, /*bIncludeUnimportant*/ true, /*bIncludeImportant*/ ComponentSettings.GetEnableReliableFlow() == false))
			{
#if DEBUG_NETWORK_PHYSICS
				{
					const int32 LocalFrame = GetRigidSolver()->GetCurrentFrame();
					const int32 ServerFrame = IsServer() ? LocalFrame : LocalFrame + GetNetworkPhysicsTickOffset();
					AsyncOutput.StateData->DebugData(FString::Printf(TEXT("SERVER | PT | SendStateData_Internal | UNRELIABLE | CurrentLocalFrame = %d | CurrentServerFrame = %d | Name: %s"), LocalFrame, ServerFrame, *GetActorName()));
				}
#endif
			}
		}

		// -- Important / Reliable flow --
		if (ComponentSettings.GetEnableReliableFlow())
		{
			/* Get the latest valid frame that can hold new important data:
			* 1. Frame after last time we called SendStateData
			* 2. Earliest possible frame in history */
			const int32 FromFrame = FMath::Max(LastStateSendFrame + 1, ToFrame - StateHistory->GetHistorySize());

			// Check if we have important data to marshal
			const int32 Count = StateHistory->CountValidData(FromFrame, ToFrame, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true);
			if (Count > 0)
			{
				// Create new data collection for marshaling
				const int32 Idx = AsyncOutput.StateDataImportant.Add(StateHistory->CreateNew());
				AsyncOutput.StateDataImportant[Idx]->ResizeDataHistory(Count);

				// Copy over data
				if (StateHistory->CopyData(*AsyncOutput.StateDataImportant[Idx], FromFrame, ToFrame, /*bIncludeUnimportant*/ false, /*bIncludeImportant*/ true))
				{
#if DEBUG_NETWORK_PHYSICS
					{
						const int32 LocalFrame = GetRigidSolver()->GetCurrentFrame();
						const int32 ServerFrame = IsServer() ? LocalFrame : LocalFrame + GetNetworkPhysicsTickOffset();
						AsyncOutput.StateDataImportant[Idx]->DebugData(FString::Printf(TEXT("SERVER | PT | SendStateData_Internal | RELIABLE | CurrentLocalFrame = %d | CurrentServerFrame = %d | Name: %s"), LocalFrame, ServerFrame, *GetActorName()));
					}
#endif
				}
			}
		}
		LastStateSendFrame = StateHistory->GetLatestFrame();
	}
}

Chaos::FPBDRigidsSolver* FAsyncNetworkPhysicsComponent::GetRigidSolver()
{
	return static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
}

Chaos::FPBDRigidsEvolution* FAsyncNetworkPhysicsComponent::GetEvolution()
{
	if (Chaos::FPBDRigidsSolver* RigidSolver = GetRigidSolver())
	{
		return RigidSolver->GetEvolution();
	}
	return nullptr;
}

void FAsyncNetworkPhysicsComponent::TriggerResimulation(int32 ResimFrame)
{
	if (ResimFrame != INDEX_NONE)
	{
		if (Chaos::FPBDRigidsSolver* RigidSolver = GetRigidSolver())
		{
			if (Chaos::FRewindData* RewindData = RigidSolver->GetRewindData())
			{
				Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
				Chaos::FPBDRigidParticleHandle* Particle = Interface.GetRigidParticle(PhysicsObject);

				// Set resim frame in rewind data
				RewindData->RequestResimulation(ResimFrame, Particle);
			}
		}
	}
}

const float FAsyncNetworkPhysicsComponent::GetCurrentInputDecay(const FNetworkPhysicsData* PhysicsData)
{
	if (!PhysicsData)
	{
		return 0.0f;
	}
	
	Chaos::FPhysicsSolverBase* BaseSolver = GetSolver();
	if (!BaseSolver)
	{
		return 0.0f;
	}

	Chaos::FRewindData* RewindData = BaseSolver->GetRewindData();
	if (!RewindData)
	{
		return 0.0f;
	}

	const FNetworkPhysicsSettingsNetworkPhysicsComponent& ComponentSettings = GetComponentSettings();
	const FRuntimeFloatCurve& InputDecayCurve = ComponentSettings.GetInputDecayCurve();

	const float NumPredictedInputs = RewindData->CurrentFrame() - PhysicsData->LocalFrame; // Number of frames we have used the same PhysicsData for during resim
	const float MaxPredictedInputs = RewindData->GetLatestFrame() - 1 - PhysicsData->LocalFrame; // Max number of frames PhysicsData registered frame until end of resim

	// Linear decay
	const float PredictionAlpha = MaxPredictedInputs > 0 ? (NumPredictedInputs / MaxPredictedInputs) : 0.0f;

	// Get decay from curve
	const float InputDecay = InputDecayCurve.GetRichCurveConst()->Eval(PredictionAlpha);

	return InputDecay;
}

void FAsyncNetworkPhysicsComponent::UpdateDynamicInputScaling()
{
	if (!PhysicsReplicationCVars::ResimulationCVars::bDynamicInputScalingEnabled)
	{
		InputsToNetwork_Owner = InputsToNetwork_OwnerDefault;
		return;
	}

	if (!IsServer())
	{
		return;
	}

	Chaos::FPBDRigidsSolver* RigidSolver = GetRigidSolver();
	if (!RigidSolver)
	{
		return;
	}

	const float TimeSinceLastDynamicScaling = RigidSolver->GetSolverTime() - TimeOfLastDynamicInputScaling;

	if (MissingInputCount > 0)
	{
		if (TimeSinceLastDynamicScaling > PhysicsReplicationCVars::ResimulationCVars::DynamicInputScalingIncreaseTimeInterval)
		{
			const uint16 MaxInputsValue = static_cast<uint16>(FMath::CeilToInt32(PhysicsReplicationCVars::ResimulationCVars::DynamicInputScalingMaxInputsPercent / (RigidSolver->GetAsyncDeltaTime())));

			// Increase the amount of inputs the owner sends
			InputsToNetwork_Owner++;

			// Update the average value for minimum clamping
			DynamicInputScalingAverageInputs += (static_cast<float>(InputsToNetwork_Owner) - DynamicInputScalingAverageInputs) * PhysicsReplicationCVars::ResimulationCVars::DynamicInputScalingIncreaseAverageMultiplier;
			
			// Clamp to maximum valid value
			InputsToNetwork_Owner = FMath::Min(InputsToNetwork_Owner, MaxInputsValue);

			TimeOfLastDynamicInputScaling = RigidSolver->GetSolverTime();
			MissingInputCount = 0;
		}
	}
	else if (TimeSinceLastDynamicScaling > PhysicsReplicationCVars::ResimulationCVars::DynamicInputScalingDecreaseTimeInterval)
	{
		// Decrease the amount of inputs the owner sends
		InputsToNetwork_Owner--;

		// Update the average value for minimum clamping, perform before clamping to allow for decreasing average even if the clamp might still round up.
		DynamicInputScalingAverageInputs += (static_cast<float>(InputsToNetwork_Owner) - DynamicInputScalingAverageInputs) * PhysicsReplicationCVars::ResimulationCVars::DynamicInputScalingDecreaseAverageMultiplier;

		// Clamp to minimum valid value
		const uint16 MinInputsValue = static_cast<uint16>(FMath::Max(static_cast<uint16>(FMath::RoundToInt(DynamicInputScalingAverageInputs)), static_cast<uint16>(PhysicsReplicationCVars::ResimulationCVars::DynamicInputScalingMinInputs)));
		InputsToNetwork_Owner = FMath::Max(InputsToNetwork_Owner, MinInputsValue);

		TimeOfLastDynamicInputScaling = RigidSolver->GetSolverTime();
	}
}

void FAsyncNetworkPhysicsComponent::RegisterDataHistoryInRewindData()
{
	if (Chaos::FPhysicsSolverBase* BaseSolver = GetSolver())
	{
		if (Chaos::FRewindData* RewindData = BaseSolver->GetRewindData())
		{
			UnregisterDataHistoryFromRewindData();

			Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			Chaos::FGeometryParticleHandle* Particle = Interface.GetParticle(PhysicsObject);

			if (InputHistory)
			{
				RewindData->AddInputHistory(InputHistory, Particle);
			}
			if (StateHistory)
			{
				RewindData->AddStateHistory(StateHistory, Particle);
			}
		}
	}
}

void FAsyncNetworkPhysicsComponent::UnregisterDataHistoryFromRewindData()
{
	if (Chaos::FPhysicsSolverBase* BaseSolver = GetSolver())
	{
		if (Chaos::FRewindData* RewindData = BaseSolver->GetRewindData())
		{
			Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			Chaos::FGeometryParticleHandle* Particle = Interface.GetParticle(PhysicsObject);

			RewindData->RemoveInputHistory(InputHistory, Particle);
			RewindData->RemoveStateHistory(StateHistory, Particle);
		}
	}
}

const int32 FAsyncNetworkPhysicsComponent::SetupRewindData()
{
	int32 NumFrames = 0;

	if (Chaos::FPBDRigidsSolver* RigidSolver = GetRigidSolver())
	{
		NumFrames = FMath::Max<int32>(1, FMath::CeilToInt32((0.001f * Chaos::FPBDRigidsSolver::GetPhysicsHistoryTimeLength()) / RigidSolver->GetAsyncDeltaTime()));

		if (IsServer())
		{
			return NumFrames;
		}

		// Don't let this actor initialize RewindData if not using resimulation
		if (GetPhysicsReplicationMode() == EPhysicsReplicationMode::Resimulation)
		{
			if (RigidSolver->IsNetworkPhysicsPredictionEnabled() && RigidSolver->GetRewindData() == nullptr)
			{
				RigidSolver->EnableRewindCapture();
			}
		}

		if (Chaos::FRewindData* RewindData = RigidSolver->GetRewindData())
		{
			NumFrames = RewindData->Capacity();
		}
	}

	return NumFrames;
}

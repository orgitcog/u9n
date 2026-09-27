// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMoverSubsystem.h"

#include "ChaosMoverAsyncCallback.h"

#include "ChaosMover/Backends/ChaosMoverBackend.h"
#include "ChaosMover/ChaosMoverDeveloperSettings.h"
#include "GameFramework/PlayerController.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/NetworkPhysicsComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosMoverSubsystem)

bool UChaosMoverSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UChaosMoverSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (FPhysScene* PhysScene = InWorld.GetPhysicsScene())
	{
		PhysScenePostTickCallbackHandle = PhysScene->OnPhysScenePostTick.AddUObject(this, &UChaosMoverSubsystem::OnPostPhysicsTick);

		if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
		{
			AsyncCallback = Solver->CreateAndRegisterSimCallbackObject_External<UE::ChaosMover::FAsyncCallback>();

			if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
			{
				InjectInputsExternalCallbackHandle = SolverCallback->InjectInputsExternal.AddUObject(this, &UChaosMoverSubsystem::InjectInputs_External);
			}
		}
	}
}

void UChaosMoverSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (InjectInputsExternalCallbackHandle.IsValid())
				{
					if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
					{
						SolverCallback->InjectInputsExternal.Remove(InjectInputsExternalCallbackHandle);
					}
				}

				if (PhysScenePostTickCallbackHandle.IsValid())
				{
					PhysScene->OnPhysScenePostTick.Remove(PhysScenePostTickCallbackHandle);
				}

				if (AsyncCallback)
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(AsyncCallback);
				}
			}
		}
	}

	Super::Deinitialize();
}

void UChaosMoverSubsystem::Register(TWeakObjectPtr<UChaosMoverBackendComponent> InBackend)
{
	Backends.AddUnique(InBackend);
}

void UChaosMoverSubsystem::Unregister(TWeakObjectPtr<UChaosMoverBackendComponent> InBackend)
{
	Backends.Remove(InBackend);
}

void UChaosMoverSubsystem::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	if (!AsyncCallback)
	{
		return;
	}

	// Clear invalid data before we attempt to use it.
	Backends.RemoveAllSwap([](const TWeakObjectPtr<UChaosMoverBackendComponent> Backend) {
		return !Backend.IsValid();
		});

	UE::ChaosMover::FAsyncCallbackInput* AsyncInput = AsyncCallback->GetProducerInputData_External();
	AsyncInput->Reset();
	AsyncInput->InputData.SetNum(Backends.Num());

	for (TWeakObjectPtr<UChaosMoverBackendComponent>& Backend : Backends)
	{
		AsyncInput->Backends.Add(Backend);
	}

	// Refresh the network physics tick offset in the async input
	const int32 NetPhysicsTickOffset = GetNetworkPhysicsTickOffset();
	AsyncInput->NetworkPhysicsTickOffset = NetPhysicsTickOffset;

	// Compute the time step
	FMoverTimeStep TimeStep;
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* Scene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
			{
				AsyncInput->PhysicsSolver = Solver;

				if (Solver->IsUsingAsyncResults())
				{
					TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
				}
				else
				{
					TimeStep.StepMs = FMath::Clamp(World->GetDeltaSeconds(), Solver->GetMinDeltaTime_External(), Solver->GetMaxDeltaTime_External()) * 1000.0f;
				}

				TimeStep.ServerFrame = Solver->GetCurrentFrame() + NetPhysicsTickOffset;
				TimeStep.BaseSimTimeMs = Solver->GetPhysicsResultsTime_External() * 1000.0f + NetPhysicsTickOffset * TimeStep.StepMs;
			}
		}
	}

	auto LambdaParallelUpdate = [NetPhysicsTickOffset, PhysicsStep, NumSteps, AsyncInput, &TimeStep, this](int32 Idx) {
		Backends[Idx]->ProduceInputData(PhysicsStep, NumSteps, TimeStep, AsyncInput->InputData[Idx]);
	};

	const bool ForceSingleThread = UE::ChaosMover::CVars::bForceSingleThreadedGT;
	Chaos::PhysicsParallelFor(Backends.Num(), LambdaParallelUpdate, ForceSingleThread);
}

void UChaosMoverSubsystem::OnPostPhysicsTick(FChaosScene* Scene)
{
	if (!AsyncCallback)
	{
		return;
	}

	const bool ForceSingleThread = UE::ChaosMover::CVars::bForceSingleThreadedGT;

	// Pop each async output
	float LastDtInMs = 0.0f;

	while (Chaos::TSimCallbackOutputHandle<UE::ChaosMover::FAsyncCallbackOutput> AsyncOutput = AsyncCallback->PopFutureOutputData_External())
	{
		auto LambdaParallelUpdate = [&AsyncOutput](int32 Idx) {
			if (UChaosMoverBackendComponent* Backend = AsyncOutput->Backends[Idx].Get())
			{
				Backend->ConsumeOutputData(AsyncOutput->TimeStep[Idx], AsyncOutput->OutputData[Idx]);
			}
		};

		if (AsyncOutput->TimeStep.Num() > 0)
		{
			LastDtInMs = AsyncOutput->TimeStep[0].StepMs;
		}

		Chaos::PhysicsParallelFor(AsyncOutput->Backends.Num(), LambdaParallelUpdate, ForceSingleThread);
	}

	float ResultsTimeInMs = 0.0f;
	if (Scene)
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			// Want the base time at the start of the frame
			ResultsTimeInMs = Solver->GetPhysicsResultsTime_External() * 1000.0f - LastDtInMs;
		}
	}

	// Call finalize frame
	auto LambdaParallelUpdate = [this, &ResultsTimeInMs](int32 Idx) {
		if (UChaosMoverBackendComponent* Backend = Backends[Idx].Get())
		{
			Backend->FinalizeFrame(ResultsTimeInMs);
		}
	};

	Chaos::PhysicsParallelFor(Backends.Num(), LambdaParallelUpdate, ForceSingleThread);
}

int32 UChaosMoverSubsystem::GetNetworkPhysicsTickOffset() const
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			return PlayerController->GetNetworkPhysicsTickOffset();
		}
	}

	return 0;
}
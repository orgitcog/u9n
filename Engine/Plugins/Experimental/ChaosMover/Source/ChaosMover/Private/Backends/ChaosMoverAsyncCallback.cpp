// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMoverAsyncCallback.h"

#include "Chaos/Framework/Parallel.h"
#include "ChaosMover/Backends/ChaosMoverBackend.h"
#include "ChaosMover/ChaosMoverDeveloperSettings.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "PBDRigidsSolver.h"

namespace UE::ChaosMover
{
	void FAsyncCallback::OnPreSimulate_Internal()
	{
		const FAsyncCallbackInput* AsyncInput = GetConsumerInput_Internal();

		if (!AsyncInput || AsyncInput->InputData.IsEmpty())
		{
			return;
		}

		FMoverTimeStep TimeStep;
		TimeStep.BaseSimTimeMs = GetSimTime_Internal() * 1000.0f;
		TimeStep.StepMs = GetDeltaTime_Internal() * 1000.0f;
		if (AsyncInput->PhysicsSolver)
		{
			TimeStep.ServerFrame = AsyncInput->PhysicsSolver->GetCurrentFrame() + AsyncInput->NetworkPhysicsTickOffset;
			TimeStep.BaseSimTimeMs += AsyncInput->NetworkPhysicsTickOffset * TimeStep.StepMs;
			TimeStep.bIsResimulating = AsyncInput->PhysicsSolver->GetEvolution()->IsResimming();
		}

		FAsyncCallbackOutput& AsyncOutput = GetProducerOutputData_Internal();
		AsyncOutput.OutputData.SetNum(AsyncInput->InputData.Num());
		AsyncOutput.TimeStep.SetNum(AsyncInput->InputData.Num());

		auto LambdaParallelUpdate = [&TimeStep, &AsyncInput, &AsyncOutput, this](int32 Idx) {
			AsyncOutput.TimeStep[Idx] = TimeStep;
			if (TStrongObjectPtr<UChaosMoverBackendComponent> Backend = AsyncInput->Backends[Idx].Pin())
			{
				Backend->GetSimulation()->SimulationTick(TimeStep, AsyncInput->InputData[Idx], AsyncOutput.OutputData[Idx]);
				AsyncOutput.Backends.Add(Backend.Get());
			}
		};

		const bool ForceSingleThread = UE::ChaosMover::CVars::bForceSingleThreadedPT;
		Chaos::PhysicsParallelFor(AsyncInput->Backends.Num(), LambdaParallelUpdate, ForceSingleThread);
	}

	void FAsyncCallback::OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier)
	{
		const FAsyncCallbackInput* AsyncInput = GetConsumerInput_Internal();

		if (!AsyncInput)
		{
			return;
		}

		FMoverTimeStep TimeStep;
		TimeStep.BaseSimTimeMs = GetSimTime_Internal() * 1000.0f;
		TimeStep.StepMs = GetDeltaTime_Internal() * 1000.0f;
		if (AsyncInput->PhysicsSolver)
		{
			TimeStep.ServerFrame = AsyncInput->PhysicsSolver->GetCurrentFrame() + AsyncInput->NetworkPhysicsTickOffset;
			TimeStep.BaseSimTimeMs += AsyncInput->NetworkPhysicsTickOffset * TimeStep.StepMs;
			TimeStep.bIsResimulating = AsyncInput->PhysicsSolver->GetEvolution()->IsResimming();
		}

		const FAsyncCallbackOutput& AsyncOutput = GetProducerOutputData_Internal();
		for (int Idx = 0; Idx < AsyncInput->Backends.Num(); ++Idx)
		{
			if (TStrongObjectPtr<const UChaosMoverBackendComponent> Backend = AsyncInput->Backends[Idx].Pin())
			{
				Backend->GetSimulation()->ModifyContacts(TimeStep, AsyncInput->InputData[Idx], AsyncOutput.OutputData[Idx], Modifier);
			}
		}
	}
}
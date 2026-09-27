// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/SimCallbackObject.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"

class UChaosMoverBackendComponent;

namespace UE::ChaosMover
{
	struct FAsyncCallbackInput : public Chaos::FSimCallbackInput
	{
		TArray<FSimulationInputData> InputData;
		TArray<TWeakObjectPtr<UChaosMoverBackendComponent>> Backends;
		Chaos::FPhysicsSolver* PhysicsSolver;
		int32 NetworkPhysicsTickOffset;

		void Reset()
		{
			InputData.Empty();
			Backends.Empty();
			PhysicsSolver = nullptr;
			NetworkPhysicsTickOffset = 0;
		}
	};

	struct FAsyncCallbackOutput : public Chaos::FSimCallbackOutput
	{
		TArray<FSimulationOutputData> OutputData;
		TArray<TWeakObjectPtr<UChaosMoverBackendComponent>> Backends;
		TArray<FMoverTimeStep> TimeStep;
		void Reset()
		{
			OutputData.Empty();
			Backends.Empty();
			TimeStep.Empty();
		}
	};

	class FAsyncCallback : public Chaos::TSimCallbackObject<
		FAsyncCallbackInput,
		FAsyncCallbackOutput,
		Chaos::ESimCallbackOptions::Presimulate |
		Chaos::ESimCallbackOptions::ContactModification |
		Chaos::ESimCallbackOptions::Rewind>
	{
	protected:
		virtual void OnPreSimulate_Internal() override;
		virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override;
	};
}
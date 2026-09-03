// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsMoverManager.h"

#include "Backends/MoverNetworkPhysicsLiaisonBase.h"
#include "Chaos/Character/CharacterGroundConstraint.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsMover/PhysicsMoverManagerAsyncCallback.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsMoverManager)

//////////////////////////////////////////////////////////////////////////

void UPhysicsMoverManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (FPhysScene* PhysScene = InWorld.GetPhysicsScene())
	{
		PhysScenePostTickCallbackHandle = PhysScene->OnPhysScenePostTick.AddUObject(this, &UPhysicsMoverManager::OnPostPhysicsTick);

		if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
		{
			AsyncCallback = Solver->CreateAndRegisterSimCallbackObject_External<FPhysicsMoverManagerAsyncCallback>();

			if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
			{
				InjectInputsExternalCallbackHandle = SolverCallback->InjectInputsExternal.AddUObject(this, &UPhysicsMoverManager::InjectInputs_External);
			}
		}
	}
}

void UPhysicsMoverManager::Deinitialize()
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

	PhysicsMoverComponents.Empty();

	Super::Deinitialize();
}

bool UPhysicsMoverManager::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

//////////////////////////////////////////////////////////////////////////

void UPhysicsMoverManager::RegisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> InPhysicsMoverComp)
{
	PhysicsMoverComponents.AddUnique(InPhysicsMoverComp);
}

void UPhysicsMoverManager::UnregisterPhysicsMoverComponent(TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> InPhysicsMoverComp)
{
	PhysicsMoverComponents.Remove(InPhysicsMoverComp);
}

//////////////////////////////////////////////////////////////////////////

void UPhysicsMoverManager::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	// Clear invalid data before we attempt to use it.
	PhysicsMoverComponents.RemoveAllSwap([](const TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> MoverComp) {
		return (MoverComp.IsValid() == false) || (MoverComp->GetUniqueIdx().IsValid() == false);
		});


	FPhysicsMoverManagerAsyncInput* ManagerAsyncInput = AsyncCallback->GetProducerInputData_External();
	ManagerAsyncInput->Reset();
	ManagerAsyncInput->AsyncInput.Reserve(PhysicsMoverComponents.Num());

	for (TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> PhysicsMoverComp : PhysicsMoverComponents)
	{
		TUniquePtr<FPhysicsMoverAsyncInput> InputData = MakeUnique<FPhysicsMoverAsyncInput>();
		PhysicsMoverComp->ProduceInput_External(PhysicsStep, NumSteps, *InputData);
		if (InputData->IsValid())
		{
			//@todo DanH: either check if it's "valid" or just establish the idx and sim associated with the input up-front
			ManagerAsyncInput->AsyncInput.Add(MoveTemp(InputData));	
		}
	}
}

void UPhysicsMoverManager::OnPostPhysicsTick(FChaosScene*)
{
	while (Chaos::TSimCallbackOutputHandle<FPhysicsMoverManagerAsyncOutput> ManagerAsyncOutput = AsyncCallback->PopFutureOutputData_External())
	{
		const double OutputTime = ManagerAsyncOutput->InternalTime;
		for (TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> PhysicsMoverComp : PhysicsMoverComponents)
		{
			Chaos::FUniqueIdx Idx = PhysicsMoverComp->GetUniqueIdx();
			if (Idx.IsValid())
			{
				if (TUniquePtr<FPhysicsMoverAsyncOutput>* OutputData = ManagerAsyncOutput->PhysicsMoverToAsyncOutput.Find(Idx))
				{
					PhysicsMoverComp->ConsumeOutput_External(**OutputData, OutputTime);
				}
			}
		}
	}

	for (TWeakObjectPtr<UMoverNetworkPhysicsLiaisonComponentBase> PhysicsMoverComp : PhysicsMoverComponents)
	{
		PhysicsMoverComp->PostPhysicsUpdate_External();
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialReadinessSubsystem.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

using namespace Chaos;

USpatialReadiness::USpatialReadiness()
	: Super()
	, SpatialReadiness(this, &This::AddUnreadyVolume, &This::RemoveUnreadyVolume) { }

USpatialReadiness::USpatialReadiness(FVTableHelper& Helper)
	: Super(Helper)
	, SpatialReadiness(this, &This::AddUnreadyVolume, &This::RemoveUnreadyVolume) { }

bool USpatialReadiness::ShouldCreateSubsystem(UObject* Outer) const
{
	// TODO: Where should the setting for this exist? Is it enough
	//       to just not load the physics readiness module if we don't
	//       want to use it?
	return true;
}

void USpatialReadiness::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	CreateSimCallback();
}

void USpatialReadiness::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USpatialReadiness::Deinitialize()
{
	DestroySimCallback();
	Super::Deinitialize();
}

FSpatialReadinessVolume USpatialReadiness::AddReadinessVolume(const FBox& Bounds, const FString& Description)
{
	// Create a readiness volume and return it's "handle".
	// This call will trigger the associated AddUnreadyVolume method
	// since volumes are unready by default.
	return SpatialReadiness.CreateVolume(Bounds, Description);
}

bool USpatialReadiness::QueryReadiness(const FBox& Bounds, TArray<FString>& OutDescriptions, const bool bAllDescriptions) const
{
	// Add the physics unready volume
	if (ensureMsgf(SimCallback, TEXT("Tried to query for readiness when no sim callback exists")))
	{
		// Query for readiness volumes in the sim callback object which tracks them
		TArray<int32> VolumeIndices;
		const bool bReady = SimCallback->QueryReadiness_GT(Bounds, VolumeIndices, bAllDescriptions);

		// If the macro is set to build with descriptions, populate the output
		// descriptions array
#if WITH_SPATIAL_READINESS_DESCRIPTIONS
		if (bReady == false)
		{
			OutDescriptions.Reset(VolumeIndices.Num());
			for (int32 VolumeIndex : VolumeIndices)
			{
				OutDescriptions.Add(SimCallback->GetVolumeData_GT(VolumeIndex)->Description);
			}
		}
#endif
		return bReady;
	}

	// Default to not-ready if there was a problem
	return false;
}

int32 USpatialReadiness::AddUnreadyVolume(const FBox& Bounds, const FString& Description)
{
	// Add the physics unready volume
	if (ensureMsgf(SimCallback, TEXT("Tried to add unready volume when no sim callback exists")))
	{
		return SimCallback->AddUnreadyVolume_GT(Bounds, Description);
	}

	return INDEX_NONE;
}

void USpatialReadiness::RemoveUnreadyVolume(int32 UnreadyVolumeIndex)
{
	// Remove the physics unready volume
	if (ensureMsgf(SimCallback, TEXT("Tried to add unready volume when no sim callback exists")))
	{
		SimCallback->RemoveUnreadyVolume_GT(UnreadyVolumeIndex);
	}
}

bool USpatialReadiness::CreateSimCallback()
{
	// If we already have a sim callback, destroy it.
	if (SimCallback)
	{
		DestroySimCallback();
	}

	// If we still have a sim callback at this point, then we
	// must have failed to delete it which means proceding would
	// leave a dangling pointer.
	if (!ensureMsgf(SimCallback == nullptr, TEXT("Tried and failed to destroy existing sim callback so that a new one could be created.")))
	{
		return false;
	}

	// The sim callback takes a scene reference in its constructor
	FPhysScene_Chaos* Scene = GetScene();
	if (!ensureMsgf(Scene, TEXT("Trying to create sim callback when there's no physics scene")))
	{
		return false;
	}

	// We need the solver to create the scene callback
	Chaos::FPhysicsSolver* Solver = GetSolver();
	if (!ensureMsgf(Solver, TEXT("Trying to create sim callback when there's no physics solver")))
	{
		return false;
	}

	// Request creation of the scene callback from the solver
	SimCallback = Solver->CreateAndRegisterSimCallbackObject_External<FSpatialReadinessSimCallback>(*Scene);
	if (!ensureMsgf(SimCallback, TEXT("Sim callback creation failed")))
	{
		return false;
	}

	// Return true to indicate successful creation of SimCallback
	return true;
}

bool USpatialReadiness::DestroySimCallback()
{
	if (SimCallback)
	{
		if (Chaos::FPhysicsSolver* Solver = GetSolver())
		{
			Solver->UnregisterAndFreeSimCallbackObject_External(SimCallback);
			SimCallback = nullptr;
			return true;
		}
	}

	return false;
}

FPhysScene_Chaos* USpatialReadiness::GetScene()
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* Scene = World->GetPhysicsScene())
		{
			return static_cast<FPhysScene_Chaos*>(Scene);
		}
	}
	return nullptr;
}

Chaos::FPhysicsSolver* USpatialReadiness::GetSolver()
{
	if (FPhysScene* Scene = GetScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			return Solver;
		}
	}

	return nullptr;
}


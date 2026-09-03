// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialReadinessSimCallback.h"
#include "Chaos/Box.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Chaos/Collision/SimSweep.h"
#include "Chaos/MidPhaseModification.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "SpatialReadinessVolume.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Physics/PhysicsFiltering.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "Physics/Experimental/ChaosInterfaceWrapper.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/GenericPhysicsInterface.h"
#include "SpatialReadinessStats.h"
#include "Engine/OverlapResult.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "DrawDebugHelpers.h"
#include "Chaos/ChaosDebugDraw.h"
#include "Chaos/DebugDrawQueue.h"
#include "SpatialReadinessDebug.h"

using namespace Chaos;

FSpatialReadinessSimCallback::FSpatialReadinessSimCallback(FPhysScene_Chaos& InPhysicsScene)
	: PhysicsScene(InPhysicsScene)
	, UnreadyVolumeData_GT(256)
	, ParticleDataCache_PT()
{ }

FPBDRigidsEvolution* FSpatialReadinessSimCallback::GetEvolution()
{
	if (FPBDRigidsSolver* MySolver = static_cast<FPBDRigidsSolver*>(GetSolver()))
	{
		return MySolver->GetEvolution();
	}

	return nullptr;
}

int32 FSpatialReadinessSimCallback::AddUnreadyVolume_GT(const FBox& Bounds, const FString& Description)
{
	SCOPE_CYCLE_COUNTER(STAT_SpatialReadiness_Physics_AddUnreadyVolumeGT)

	// Get a pointer to the input struct - if this is null just cancel. If we create a
	// particle without the input, then we'll lose track of it.
	FSpatialReadinessSimCallbackInput* SimInputPtr = GetProducerInputData_External();
	if (!ensureMsgf(SimInputPtr, TEXT("Failed to access sim callback object input")))
	{
		return INDEX_NONE;
	}
	FSpatialReadinessSimCallbackInput& SimInput = *SimInputPtr;

	// Create a box implicit geometry from the bounds
	const FVec3 BoxCenter = (Bounds.Min + Bounds.Max) * .5f;
	const FVec3 BoxHalfExtent = (Bounds.Max - Bounds.Min) * .5f;
	Chaos::FImplicitObjectPtr BoxGeom = MakeImplicitObjectPtr<TBox<FReal, 3>>(-BoxHalfExtent, BoxHalfExtent);

	// Create a new static particle to represent the volume
	FActorCreationParams Params;
	Params.bSimulatePhysics = false;
	Params.bStatic = true;
	Params.InitialTM = FTransform(FQuat::Identity, BoxCenter);
	Params.Scene = &PhysicsScene;
	FSingleParticlePhysicsProxy* ParticleProxy = nullptr;
	FChaosEngineInterface::CreateActor(Params, ParticleProxy);
	if (!ensureMsgf(ParticleProxy, TEXT("Failed to create new particle proxy")))
	{
		return INDEX_NONE;
	}
	FRigidBodyHandle_External& ParticleHandle = ParticleProxy->GetGameThreadAPI();

	// Create collision response container
	FCollisionResponseContainer CollisionResponse;
	// We want this to block everything that simulates, even if it isn't in the ECC_PhysicsBody channel.
	// Since bodies can be in any channel they want, we have to block all channels.
	CollisionResponse.SetAllChannels(ECollisionResponse::ECR_Block);

	// Create collision filter data for the particle
	FCollisionFilterData QueryFilterData, SimFilterData;
	CreateShapeFilterData(
		/* MyChannel */            static_cast<uint8>(ECollisionChannel::ECC_WorldDynamic),
		/* MaskFilter */           FMaskFilter(0),
		/* SourceObjectID */       0,
		/* ResponseToChannels */   CollisionResponse,
		/* ComponentID */          0,
		/* BodyIndex */            0,
		/* OutQueryData */         QueryFilterData,
		/* OutSimData */           SimFilterData,
		/* bEnableCCD */           true,
		/* bEnableContactNotify */ false,
		/* bStaticShape */         true);

	// Notes of query filter data:
	// * We want to block all channels in sim but overlap all channels query.
	// * By default CreateShapeFilterData uses CollisionResponse for both
	// * Word1 is blocking data, and Word2 is overlap data.
	// * So we OR query Word1 into Word2 so that we block overlaps and clear
	//   Word1 so that we don't block any queries.
	QueryFilterData.Word2 |= QueryFilterData.Word1;
	QueryFilterData.Word1 = 0;
	QueryFilterData.Word3 |= EPDF_SimpleCollision | EPDF_ComplexCollision;

	// Make the geometry
	ParticleHandle.SetGeometry(BoxGeom);
	ParticleHandle.SetShapeSimCollisionEnabled(0, true);
	ParticleHandle.SetShapeQueryCollisionEnabled(0, true);
	ParticleHandle.SetShapeSimData(0, SimFilterData);
	ParticleHandle.ShapesArray()[0]->SetIsProbe(true);
	ParticleHandle.ShapesArray()[0]->SetQueryData(QueryFilterData);
#if CHAOS_DEBUG_NAME
	ParticleHandle.SetDebugName(MakeShared<FString>(TEXT("Unready Volume")));
#endif

	// Add the new particle to the scene
	TArray<FPhysicsActorHandle> Actors = { ParticleProxy };
	PhysicsScene.AddActorsToScene_AssumesLocked(Actors);

	// Save the proxy in our list of GT particles.
	//
	// Technically, the index of the volume just has to be unique, it doesn't
	// have to be the same as the particle index. However, we do this
	// just so that when querying there's no need to map from hit particles
	// back to volume index - we'll already have direct access to the index.
	const int32 ParticleIndex = ParticleHandle.UniqueIdx().Idx;
	const int32 VolumeIndex = ParticleIndex;
	ensureMsgf(UnreadyVolumeData_GT.TryAdd(VolumeIndex,
		FUnreadyVolumeData_GT(
			ParticleProxy,
			Description
		)), TEXT("Failed to add volume data to map - VolumeIndex already exists!"));

	// Queue up the particle proxy for processing on PT
	SimInput.UnreadyVolumesToAdd.Add(ParticleProxy);
	SimInput.UnreadyVolumesToRemove.Remove(ParticleProxy);

	return VolumeIndex;
}

void FSpatialReadinessSimCallback::RemoveUnreadyVolume_GT(int32 UnreadyVolumeIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_SpatialReadiness_Physics_RemoveUnreadyVolumeGT)

	FUnreadyVolumeData_GT* VolumeData = UnreadyVolumeData_GT.Find(UnreadyVolumeIndex);
	if (!ensureMsgf(VolumeData, TEXT("Trying to remove unready volume whos index is not being tracked")))
	{
		return;
	}

	// Get the proxy associated with this index
	FSingleParticlePhysicsProxy* ParticleProxy = VolumeData->Proxy;
	if (!ensureMsgf(ParticleProxy, TEXT("Particle proxy associated with unready volume index was null")))
	{
		return;
	}
	
	// Free the index in our GT tracker
	UnreadyVolumeData_GT.Remove(UnreadyVolumeIndex);

	// Tell the PT to remove it's tracking of this proxy as well
	FSpatialReadinessSimCallbackInput* SimInput = GetProducerInputData_External();
	if (ensureMsgf(SimInput, TEXT("Failed to access sim input data")))
	{
		SimInput->UnreadyVolumesToRemove.Add(ParticleProxy);
		SimInput->UnreadyVolumesToAdd.Remove(ParticleProxy);
	}

	// Delete the particle
	FChaosEngineInterface::ReleaseActor(ParticleProxy, &PhysicsScene);
}

bool FSpatialReadinessSimCallback::QueryReadiness_GT(const FBox& Bounds, TArray<int32>& OutVolumeIndices, bool bAllUnreadyVolumes) const
{
	// Constants that we'll use to set up query parameters
	constexpr static ECollisionChannel Channel = ECollisionChannel::ECC_PhysicsBody;//Static;
	constexpr static uint8 ChannelBit = static_cast<uint8>(Channel);
	constexpr static bool bComplex = false;
	const bool bMulti = bAllUnreadyVolumes;

	// Query objects
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ReadinessQuery), false);
	QueryParams.bTraceComplex = false;
	FCollisionObjectQueryParams ObjectParams(static_cast<uint8>(Channel));
	FCollisionResponseContainer ResponseContainer;
	FCollisionFilterData CollisionFilterData = CreateQueryFilterData(ChannelBit, bComplex, ResponseContainer, QueryParams, ObjectParams, bMulti);
	FOverlapAllQueryCallback QueryCallback;
	EQueryFlags QueryFlags = EQueryFlags::PreFilter;// | EQueryFlags::SkipNarrowPhase;
	ChaosInterface::FQueryFilterData QueryFilterData = ChaosInterface::MakeQueryFilterData(CollisionFilterData, QueryFlags, QueryParams);
	ChaosInterface::FQueryDebugParams DebugParams = { };

	// Create the geometry needed for the query
	Chaos::FImplicitBox3 Geom = Chaos::FImplicitBox3(Bounds.Min, Bounds.Max, 0);

	// Do the query
	FPhysicsHitCallback<ChaosInterface::FOverlapHit> HitBuffer;
	FPhysicsCommand::ExecuteRead(&PhysicsScene,
	[this, &Geom, &HitBuffer, &QueryFlags, &CollisionFilterData, &QueryFilterData, &QueryCallback, &DebugParams]()
	{
		Chaos::Private::LowLevelOverlap(PhysicsScene, Geom, FTransform::Identity, HitBuffer, QueryFlags, CollisionFilterData, QueryFilterData, &QueryCallback, DebugParams);
	});

	// Make sure the hits from the buffer are actually in our list of unready volumes,
	// and collect their indices
	OutVolumeIndices.Reset();
	for (int32 HitIdx = 0; HitIdx < HitBuffer.GetNumHits(); ++HitIdx)
	{
		// Get the hit result from this index
		const ChaosInterface::FOverlapHit& Hit = HitBuffer.GetHits()[HitIdx];

		// Get the particle that we hit
		Chaos::FGeometryParticle* HitParticle = Hit.Actor;
		if (HitParticle == nullptr)
		{
			continue;
		}

		// Get the particle index
		//
		// NOTE: Since we're just directly using particle unique indices for
		// volume indices, this mapping is simplified. We may at some point
		// want to use a more complex mapping though, in which case we'll need
		// to do something different here.
		const uint32 VolumeIndex = HitParticle->UniqueIdx().Idx;

		// Make sure that we have an actual entry for this particle in our
		// unready volumes list
		//if (!UnreadyVolumeData_GT.IsValidIndex(VolumeIndex))
		if (UnreadyVolumeData_GT.Find(VolumeIndex) == nullptr)
		{
			continue;
		}

		// Add the index to the output list
		OutVolumeIndices.Add(VolumeIndex);
	}

	// If we didn't hit any unready volumes then that means this volume is "ready"
	return OutVolumeIndices.Num() == 0;
}

const FUnreadyVolumeData_GT* FSpatialReadinessSimCallback::GetVolumeData_GT(int32 VolumeIndex) const
{
	return UnreadyVolumeData_GT.Find(VolumeIndex);
}

void FSpatialReadinessSimCallback::ForEachVolumeData_GT(const TFunction<void(const FUnreadyVolumeData_GT&)>& Func)
{
	for (int32 Index = 0; Index < UnreadyVolumeData_GT.Num(); ++Index)
	{
		Func(UnreadyVolumeData_GT.At(Index));
	}
}

bool FSpatialReadinessSimCallback::QueryReadiness_PT(const FAABB3& Bounds, TArray<const FSingleParticlePhysicsProxy*>& OutVolumeProxies)
{
	// Get the evolution, and get the acceleration structure from it
	FPBDRigidsEvolution* Evolution = GetEvolution();
	if (Evolution == nullptr)
	{
		return false;
	}

	// Set up particle filters for the query, so that we only get unready volumes
	// NOTE: If we could speed up this bit here, that'd be great
	const auto& ParticleFilter = [this](const FGeometryParticleHandle* Particle) -> bool
	{
		// If the particle has a single particle physics proxy (and it does,
		// I can almost guarantee it), then count it as a hit if it's in our
		// list of unready volume particles.
		if (const IPhysicsProxyBase* Proxy = Particle->PhysicsProxy())
		{
			if (Proxy->GetType() == EPhysicsProxyType::SingleParticleProxy)
			{
				return UnreadyVolumeParticles_PT.Contains(
					static_cast<const Chaos::FSingleParticlePhysicsProxy*>(Proxy));
			}
		}
		return false;
	};

	// We know that unready volumes will all be boxes, so in theory we could
	// probably filter a bunch of interactions before getting to the particle
	// filter, however the particle filter is applied first so there's no point
	// in implementing a shape filter.
	const auto& ShapeFilter = [](const FPerShapeData* Shape, const FImplicitObject* Implicit)
	{
		return true;
	};

	// Make a lambda for collecting 
	TArray<Private::FSimOverlapParticleShape> Overlaps;
	const auto& OverlapCollector = [&Overlaps](const Private::FSimOverlapParticleShape& Overlap)
	{
		Overlaps.Add(Overlap);
	};

	// Get the broadphase and query against it to see if this new rigid particle
	// generates midphases with any unready volumes.
	//
	// NOTE: In theory we could have a first-hit version of this function which
	// would potentially avoid some unnecessary checking.
	//
	// NOTE: The above note is only valid until we actually start registering
	// freeze locks with unready volumes...
	Private::SimOverlapBounds(Evolution->GetSpatialAcceleration(), Bounds, ParticleFilter, ShapeFilter, OverlapCollector);

	// Convert overlaps to list of hit proxies
	OutVolumeProxies.Reset();
	for (const Private::FSimOverlapParticleShape& Overlap : Overlaps)
	{
		const Chaos::FGeometryParticleHandle* ParticleHandle = Overlap.HitParticle;
		if (ParticleHandle == nullptr)
		{
			continue;
		}

		const IPhysicsProxyBase* Proxy = ParticleHandle->PhysicsProxy();
		if (Proxy == nullptr)
		{
			continue;
		}

		if (Proxy->GetType() != EPhysicsProxyType::SingleParticleProxy)
		{
			continue;
		}

		OutVolumeProxies.Add(static_cast<const Chaos::FSingleParticlePhysicsProxy*>(Proxy));
	}

	// Return true if there were NO overlaps - meaning, the volume is ready
	return OutVolumeProxies.Num() == 0;
}

void FSpatialReadinessSimCallback::FreezeParticles_PT()
{
	SCOPE_CYCLE_COUNTER(STAT_SpatialReadiness_Physics_FreezeParticlesPT)

	FPBDRigidsEvolution* Evolution = GetEvolution();
	if (!ensureMsgf(Evolution, TEXT("Attempted to freeze particle, but had no evolution")))
	{
		return;
	}

	for (FPBDRigidParticleHandle* RigidParticle : UnreadyRigidParticles_PT)
	{
		const EObjectStateType ObjectState = RigidParticle->ObjectState();
		if (ObjectState == EObjectStateType::Static)
		{
			return;
		}

		// Get the geometry particle
		FGeometryParticleHandle* GeometryParticle = RigidParticle;

		// Cache the current object state
		ParticleDataCache_PT.Add({ GeometryParticle, ObjectState });

		// Set the object state to static
		Evolution->SetParticleObjectState(RigidParticle, EObjectStateType::Static);
	}
}

void FSpatialReadinessSimCallback::UnFreezeParticles_PT()
{
	SCOPE_CYCLE_COUNTER(STAT_SpatialReadiness_Physics_UnFreezeParticlesPT)

	FPBDRigidsEvolution* Evolution = GetEvolution();
	if (!ensureMsgf(Evolution, TEXT("Attempted to un-freeze particles, but had no evolution")))
	{
		return;
	}

	// For each particle that we froze, unfreeze it
	for (TPair<FGeometryParticleHandle*, EObjectStateType>& ParticleData : ParticleDataCache_PT)
	{
		FPBDRigidParticleHandle* RigidParticle = ParticleData.Key->CastToRigidParticle();
		if (RigidParticle == nullptr)
		{
			continue;
		}

		// Restore the object state of this particle
		Evolution->SetParticleObjectState(RigidParticle, ParticleData.Value);
	}

	// Clear the array
	ParticleDataCache_PT.Reset();
}

void FSpatialReadinessSimCallback::OnPreSimulate_Internal()
{
	SCOPE_CYCLE_COUNTER(STAT_SpatialReadiness_Physics_PreSimulate)

	// Process inputs we may have gotten from the game thread
	if (const FSpatialReadinessSimCallbackInput* Input = GetConsumerInput_Internal())
	{
		// Process additions
		for (FSingleParticlePhysicsProxy* ParticleProxy : Input->UnreadyVolumesToAdd)
		{
			UnreadyVolumeParticles_PT.Add(ParticleProxy);
		}

		// Process removals
		for (FSingleParticlePhysicsProxy* ParticleProxy : Input->UnreadyVolumesToRemove)
		{
			UnreadyVolumeParticles_PT.Remove(ParticleProxy);
		}
	}
}

void FSpatialReadinessSimCallback::OnParticlesRegistered_Internal(TArray<FSingleParticlePhysicsProxy*>& RegisteredProxies)
{
	SCOPE_CYCLE_COUNTER(STAT_SpatialReadiness_Physics_ParticlesRegistered)

	// For each newly added particle, query against unready volumes to see if
	// it should be frozen.
	for (FSingleParticlePhysicsProxy* ParticleProxy : RegisteredProxies)
	{
		FGeometryParticleHandle* GeometryParticle = ParticleProxy->GetHandle_LowLevel();
		if (GeometryParticle == nullptr)
		{
			continue;
		}

		FPBDRigidParticleHandle* RigidParticle = GeometryParticle->CastToRigidParticle();
		if (RigidParticle == nullptr)
		{
			continue;
		}

		// Get the bounds to use for the query
		TArray<const FSingleParticlePhysicsProxy*> VolumeProxies;
		const FAABB3 QueryBounds = RigidParticle->WorldSpaceInflatedBounds();
		if (!QueryReadiness_PT(QueryBounds, VolumeProxies))
		{
			// If this volume hit an unready volume, query readiness returns false
			// and we must mark the particle as frozen
			UnreadyRigidParticles_PT.Add(RigidParticle);
		}
	}
}

void FSpatialReadinessSimCallback::OnMidPhaseModification_Internal(FMidPhaseModifierAccessor& Accessor)
{
	SCOPE_CYCLE_COUNTER(STAT_SpatialReadiness_Physics_MidPhase)

	// Clear the list of unready particles
	UnreadyRigidParticles_PT.Reset();

	// Go through every unready volume
	for (FSingleParticlePhysicsProxy* UnreadyProxy : UnreadyVolumeParticles_PT)
	{
		if (UnreadyProxy->GetMarkedDeleted())
		{
			continue;
		}

		FGeometryParticleHandle* UnreadyVolume = UnreadyProxy->GetHandle_LowLevel();
		if (UnreadyVolume == nullptr)
		{
			continue;
		}

#if ENABLE_DRAW_DEBUG
		if (CVarSpatialReadinessDebugDraw.GetValueOnAnyThread())
		{
			const FAABB3 Bounds = UnreadyVolume->WorldSpaceInflatedBounds();
			Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(Bounds.Center(), Bounds.Extents() * .5f, FQuat::Identity, FColor::Red, false, -1.f, -1, 0.f);
		}
#endif

		// Go through every mid-phase which involves this volume
		for (FMidPhaseModifier& MidPhase : Accessor.GetMidPhases(UnreadyVolume))
		{
			// Get the particle that is not the unready volume
			FGeometryParticleHandle* GeometryParticle = MidPhase.GetOtherParticle(UnreadyVolume);
			if (GeometryParticle == nullptr)
			{
				continue;
			}

			FPBDRigidParticleHandle* RigidParticle = GeometryParticle->CastToRigidParticle();
			if (RigidParticle == nullptr)
			{
				continue;
			}

#if ENABLE_DRAW_DEBUG
			if (CVarSpatialReadinessDebugDraw.GetValueOnAnyThread())
			{
				const FAABB3 Bounds = RigidParticle->WorldSpaceInflatedBounds();
				Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(Bounds.Center(), Bounds.Extents() * .5f, FQuat::Identity, FColor::Yellow, false, -1.f, -1, 0.f);
			}
#endif

			// Add the particle to the list of particles to freeze
			UnreadyRigidParticles_PT.Add(RigidParticle);

			// Disable the midphase
			MidPhase.Disable();
		}
	}
}

void FSpatialReadinessSimCallback::OnPreIntegrate_Internal()
{
	FreezeParticles_PT();
}

void FSpatialReadinessSimCallback::OnPostIntegrate_Internal()
{
	UnFreezeParticles_PT();
}

uint32 FSpatialReadinessSimCallback::FHashMapTraits::GetElementID(const FUnreadyVolumeData_GT& Element)
{
	if (FSingleParticlePhysicsProxy* Proxy = Element.Proxy)
	{
		return Proxy->GetGameThreadAPI().UniqueIdx().Idx;
	}
	return INDEX_NONE;
}


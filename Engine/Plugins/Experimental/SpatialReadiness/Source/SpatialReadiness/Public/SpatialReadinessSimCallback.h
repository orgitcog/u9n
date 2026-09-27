// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/SimCallbackObject.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "Chaos/Particle/ObjectState.h"
#include "Chaos/Framework/HashMappedArray.h"
#include "SpatialReadinessVolume.h"
#include "PhysicsSolver.h"

using namespace Chaos;

class FPhysScene_Chaos;
namespace Chaos
{
	class FSingleParticlePhysicsProxy;
}

struct FUnreadyVolumeData_GT
{
	FUnreadyVolumeData_GT(FSingleParticlePhysicsProxy* InProxy, const FString& InDescription)
		: Proxy(InProxy)
#if WITH_SPATIAL_READINESS_DESCRIPTIONS
		, Description(InDescription)
#endif
	{ }
	FSingleParticlePhysicsProxy* Proxy;
#if WITH_SPATIAL_READINESS_DESCRIPTIONS
	FString Description;
#endif
};

struct FSpatialReadinessSimCallbackInput : public FSimCallbackInput
{
	TSet<FSingleParticlePhysicsProxy*> UnreadyVolumesToAdd;
	TSet<FSingleParticlePhysicsProxy*> UnreadyVolumesToRemove;

	void Reset()
	{
		UnreadyVolumesToAdd.Reset();
		UnreadyVolumesToRemove.Reset();
	}
};

struct FSpatialReadinessSimCallback
	: public TSimCallbackObject<
		FSpatialReadinessSimCallbackInput,
		FSimCallbackNoOutput,
		ESimCallbackOptions::Presimulate |
		ESimCallbackOptions::ParticleRegister |
		ESimCallbackOptions::MidPhaseModification |
		ESimCallbackOptions::PreIntegrate |
		ESimCallbackOptions::PostIntegrate>
{
	using This = FSpatialReadinessSimCallback;

public:
	FSpatialReadinessSimCallback(FPhysScene_Chaos& InPhysicsScene);

	// Game thread functions for adding and removing unready volumes
	int32 AddUnreadyVolume_GT(const FBox& Bounds, const FString& Description);
	void RemoveUnreadyVolume_GT(int32 UnreadyVolumeIndex);

	// Game thread function for querying for unready volumes
	//
	// If bAllUnreadyVolumes is true, then a multi-query will be used and
	// OutVolumeIndices will be populated with every index that encroaches.
	//
	// For performance, we only return the index of the first unread volume
	// that we find.
	bool QueryReadiness_GT(const FBox& Bounds, TArray<int32>& OutVolumeIndices, bool bAllUnreadyVolumes=false) const;

	// Given a volume index, get it's description
	const FUnreadyVolumeData_GT* GetVolumeData_GT(int32 VolumeIndex) const;

	// Iterate over each unready volume and get 
	void ForEachVolumeData_GT(const TFunction<void(const FUnreadyVolumeData_GT&)>& Func);

	/* begin: TSimCallbackObject */
protected:
	virtual void OnPreSimulate_Internal() override;
	virtual void OnParticlesRegistered_Internal(TArray<FSingleParticlePhysicsProxy*>& RegisteredProxies) override;
	virtual void OnMidPhaseModification_Internal(FMidPhaseModifierAccessor& Accessor) override;
	virtual void OnPreIntegrate_Internal() override;
	virtual void OnPostIntegrate_Internal() override;
	/* end: TSimCallbackObject */

	// Helpers
	FPBDRigidsEvolution* GetEvolution();

	// Functions for freezing and unfreezing all particles in the
	// UnreadyRigidParticles_PT set.
	void FreezeParticles_PT();
	void UnFreezeParticles_PT();

	// Physics thread function for querying for unready volumes
	// TODO: Make const
	bool QueryReadiness_PT(const FAABB3& Bounds, TArray<const FSingleParticlePhysicsProxy*>& OutVolumeProxies);

	// Keep a ref to the phys scene so we can add and remove particles
	FPhysScene_Chaos& PhysicsScene;

	// List of unready volume physics proxies. We directly use single particle
	// physics proxy rather than something more generic because we know that
	// we are only going to create static single particles for these volumes. 
	struct FHashMapTraits
	{
		static uint32 GetIDHash(const int32 Idx) { return MurmurFinalize32(Idx); }
		static uint32 GetElementID(const FUnreadyVolumeData_GT& Element);
	};
	Private::THashMappedArray<int32, FUnreadyVolumeData_GT, FHashMapTraits> UnreadyVolumeData_GT;

	// List of particle handles which represent unready volumes
	TSet<FSingleParticlePhysicsProxy*> UnreadyVolumeParticles_PT;

	// List of particle handles which represent particles that interacted with unready volumes
	TSet<FPBDRigidParticleHandle*> UnreadyRigidParticles_PT;

	// "Unready" particles are forced to be stationary in PreSimulate, and restored
	// to their previous state in PostIntegrate. The values needed for restoration
	// are stored and mapped back to particle id's in this hash-mapped array.
	TArray<TPair<FGeometryParticleHandle*, EObjectStateType>> ParticleDataCache_PT;
};

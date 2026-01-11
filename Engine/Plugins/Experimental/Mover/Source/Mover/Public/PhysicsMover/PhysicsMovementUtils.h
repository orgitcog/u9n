// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ParticleHandleFwd.h"
#include "Containers/Array.h"
#include "Math/MathFwd.h"

#define UE_API MOVER_API

struct FFloorCheckResult;
struct FWaterCheckResult;
struct FHitResult;
class UPrimitiveComponent;

/**
 * PhysicsMovementUtils: a collection of stateless static functions for a variety of physics movement-related operations
 */
class UPhysicsMovementUtils
{
public:
	static UE_API void FloorSweep_Internal(const FVector& Location, const FVector& DeltaPos, const UPrimitiveComponent* UpdatedPrimitive, const FVector& UpDir,
		float QueryRadius, float QueryDistance, float MaxWalkSlopeCosine, float TargetHeight, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult);

	// If the hit result hit something, return the particle handle
	static UE_API Chaos::FPBDRigidParticleHandle* GetRigidParticleHandleFromHitResult(const FHitResult& HitResult);

	static UE_API Chaos::FPBDRigidParticleHandle* GetRigidParticleHandleFromComponent(UPrimitiveComponent* PrimComp);

	// Checks if any hit is with water and, if so, fills in the OutWaterResult
	static UE_API bool GetWaterResultFromHitResults(const TArray<FHitResult>& Hits, const FVector& Location, const float TargetHeight, FWaterCheckResult& OutWaterResult);

	// Returns the current ground velocity at the character position
	static UE_API FVector ComputeGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds);

	// Returns the integrated with gravity velocity of the ground at the character position
	static UE_API FVector ComputeIntegratedGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds);
};

#undef UE_API

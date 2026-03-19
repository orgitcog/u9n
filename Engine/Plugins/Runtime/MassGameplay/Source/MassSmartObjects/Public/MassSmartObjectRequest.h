// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "SmartObjectSubsystem.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "Containers/StaticArray.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "SmartObjectRequestTypes.h"
#include "ZoneGraphTypes.h"
#include "MassSmartObjectRequest.generated.h"

/**
 * Structure that represents a potential smart object slot for a MassEntity during the search
 */
USTRUCT()
struct FSmartObjectCandidateSlot
{
	GENERATED_BODY()

	FSmartObjectCandidateSlot() = default;
	FSmartObjectCandidateSlot(const FSmartObjectRequestResult InResult, const float InCost)	: Result(InResult), Cost(InCost) {}

	UPROPERTY(Transient)
	FSmartObjectRequestResult Result;

	UPROPERTY(Transient)
	float Cost = 0.f;
};

/**
 * Identifier associated to a request for smart object candidates. We use a 1:1 match
 * with an FMassEntityHandle since all requests are batched together using the EntitySubsystem.
 */
USTRUCT()
struct FMassSmartObjectRequestID
{
	GENERATED_BODY()

	FMassSmartObjectRequestID() = default;
	explicit FMassSmartObjectRequestID(const FMassEntityHandle InEntity) : Entity(InEntity) {}

	bool IsSet() const { return Entity.IsSet(); }
	void Reset() { Entity.Reset(); }

	explicit operator FMassEntityHandle() const { return Entity; }

private:
	UPROPERTY(Transient)
	FMassEntityHandle Entity;
};

/**
 * Struct that holds status and results of a candidate finder request
 */
USTRUCT(BlueprintType)
struct FMassSmartObjectCandidateSlots
{
	GENERATED_BODY()

	void Reset()
	{
		NumSlots = 0;
	}
	
	static constexpr uint32 MaxNumCandidates = 4;
	TStaticArray<FSmartObjectCandidateSlot, MaxNumCandidates> Slots;

	UPROPERTY(Transient)
	uint8 NumSlots = 0;
};

/**
 * Fragment that holds the result of a request to find candidates.
 */
USTRUCT()
struct FMassSmartObjectRequestResultFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FMassSmartObjectCandidateSlots Candidates;

	UPROPERTY(Transient)
	bool bProcessed = false;
};

/**
 * Fragment used to build a list potential smart objects to use. Once added to an entity
 * this will be processed by the candidates finder processor to fill a SmartObjectCandidates
 * fragment that could then be processed by the reservation processor
 */
USTRUCT()
struct FMassSmartObjectWorldLocationRequestFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FVector SearchOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	FMassEntityHandle RequestingEntity;

	UPROPERTY(Transient)
	FGameplayTagContainer UserTags;

	UPROPERTY(Transient)
	FGameplayTagQuery ActivityRequirements;
};

/**
 * Fragment used to build a list potential smart objects to use. Once added to an entity
 * this will be processed by the candidates finder processor to fill a SmartObjectCandidates
 * fragment that could then be processed by the reservation processor
 */
USTRUCT()
struct FMassSmartObjectLaneLocationRequestFragment : public FMassFragment
{
	GENERATED_BODY()

	FZoneGraphCompactLaneLocation CompactLaneLocation;

	UPROPERTY(Transient)
	FMassEntityHandle RequestingEntity;

	UPROPERTY(Transient)
	FGameplayTagContainer UserTags;

	UPROPERTY(Transient)
	FGameplayTagQuery ActivityRequirements;
};

/**
 * Special tag to mark processed requests
 */
USTRUCT()
struct FMassSmartObjectCompletedRequestTag : public FMassTag
{
	GENERATED_BODY()
};

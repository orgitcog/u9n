// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGGenSourceBase.h"

#include "PCGGenSourceWPStreamingSource.generated.h"

#define UE_API PCG_API

struct FWorldPartitionStreamingSource;

/**
 * This GenerationSource captures WorldPartitionStreamingSources for RuntimeGeneration.
 * 
 * UPCGGenSourceWPStreamingSources are created per tick and live only until the end of the tick.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGenSourceWPStreamingSource : public UObject, public IPCGGenSourceBase
{
	GENERATED_BODY()

public:
	/** Returns the world space position of this gen source. */
	UE_API virtual TOptional<FVector> GetPosition() const override;

	/** Returns the normalized forward vector of this gen source. */
	UE_API virtual TOptional<FVector> GetDirection() const override;

public:
	const FWorldPartitionStreamingSource* StreamingSource;
};

#undef UE_API

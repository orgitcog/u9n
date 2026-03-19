// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollisionQueryParams.h"

struct FFloorCheckResult;
struct FWaterCheckResult;
class UWorld;

namespace UE::ChaosMover::Utils
{
	struct FFloorSweepParams
	{
		FCollisionResponseParams ResponseParams;
		FCollisionQueryParams QueryParams;
		FVector Location;
		FVector DeltaPos;
		FVector UpDir;
		const UWorld* World;
		float QueryDistance;
		float QueryRadius;
		float MaxWalkSlopeCosine;
		float TargetHeight;
		ECollisionChannel CollisionChannel;
	};

	extern CHAOSMOVER_API void FloorSweep_Internal(const FFloorSweepParams& Params, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult);

}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("SpatialReadiness"), STATGROUP_SpatialReadiness, STATCAT_Advanced);

// Physics
DECLARE_CYCLE_STAT(TEXT("SpatialReadiness_Physics_AddUnreadyVolumeGT"), STAT_SpatialReadiness_Physics_AddUnreadyVolumeGT, STATGROUP_SpatialReadiness);
DECLARE_CYCLE_STAT(TEXT("SpatialReadiness_Physics_RemoveUnreadyVolumeGT"), STAT_SpatialReadiness_Physics_RemoveUnreadyVolumeGT, STATGROUP_SpatialReadiness);
DECLARE_CYCLE_STAT(TEXT("SpatialReadiness_Physics_FreezeParticlesPT"), STAT_SpatialReadiness_Physics_FreezeParticlesPT, STATGROUP_SpatialReadiness);
DECLARE_CYCLE_STAT(TEXT("SpatialReadiness_Physics_UnFreezeParticlesPT"), STAT_SpatialReadiness_Physics_UnFreezeParticlesPT, STATGROUP_SpatialReadiness);
DECLARE_CYCLE_STAT(TEXT("SpatialReadiness_Physics_PreSimulate"), STAT_SpatialReadiness_Physics_PreSimulate, STATGROUP_SpatialReadiness);
DECLARE_CYCLE_STAT(TEXT("SpatialReadiness_Physics_ParticlesRegistered"), STAT_SpatialReadiness_Physics_ParticlesRegistered, STATGROUP_SpatialReadiness);
DECLARE_CYCLE_STAT(TEXT("SpatialReadiness_Physics_MidPhase"), STAT_SpatialReadiness_Physics_MidPhase, STATGROUP_SpatialReadiness);


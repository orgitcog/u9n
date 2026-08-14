// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineDefines.h"

class UObject;
struct FMassEntityHandle;
struct FMassMoveTargetFragment;
struct FMassNavMeshShortPathFragment;

namespace UE::MassNavigation
{
	MASSNAVMESHNAVIGATION_API bool ActivateActionStand(
		const UObject* Requester, const FMassEntityHandle Entity, const float DesiredSpeed, FMassMoveTargetFragment& InOutMoveTarget, FMassNavMeshShortPathFragment& OutShortPath);

	MASSNAVMESHNAVIGATION_API bool ActivateActionAnimate(
		const UObject* Requester, const FMassEntityHandle Entity, FMassMoveTargetFragment& MoveTarget);
};

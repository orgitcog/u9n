// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Particle/ObjectState.h"
#include "Containers/UnrealString.h"

class FString;

namespace Chaos
{
	CHAOS_API FString ToString(const EObjectStateType ObjectState);
	CHAOS_API FString ToString(const FRotation3& V);
	CHAOS_API FString ToString(const FRotation3f& V);
	CHAOS_API FString ToString(const FVec3& V);
	CHAOS_API FString ToString(const FVec3f& V);
	CHAOS_API FString ToString(const TBitArray<>& BitArray);
}
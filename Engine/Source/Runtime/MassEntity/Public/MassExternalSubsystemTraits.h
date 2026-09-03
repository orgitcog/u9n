// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6

/**
 * Traits describing how a given piece of code can be used by Mass. We require author or user of a given subsystem to 
 * define its traits. To do it add the following in an accessible location. 
 *
 * template<>
 * struct TMassExternalSubsystemTraits<UMyCustomManager>
 * {
 *		enum { GameThreadOnly = false; }
 * }
 *
 * this will let Mass know it can access UMyCustomManager on any thread.
 *
 * This information is being used to calculate processor and query dependencies as well as appropriate distribution of
 * calculations across threads.
 */
template <typename T>
struct TMassExternalSubsystemTraits final
{
	enum
	{
		// Unless configured otherwise each subsystem will be treated as "game-thread only".
		GameThreadOnly = true,

		// If set to true all RW and RO operations will be viewed as RO when calculating processor dependencies
		ThreadSafeWrite = !GameThreadOnly,
	};
};

/** 
 * Shared Fragments' traits.
 * @see TMassExternalSubsystemTraits
 */
template <typename T>
struct TMassSharedFragmentTraits final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};

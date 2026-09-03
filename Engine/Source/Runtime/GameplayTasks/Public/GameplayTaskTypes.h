// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Templates/Casts.h"

#ifndef WITH_GAMEPLAYTASK_DEBUG
#define WITH_GAMEPLAYTASK_DEBUG (WITH_UNREAL_DEVELOPER_TOOLS || !(UE_BUILD_SHIPPING || UE_BUILD_TEST))
#endif // WITH_GAMEPLAYTASK_DEBUG

namespace FGameplayTasks
{
	inline constexpr uint8 DefaultPriority = 127;
	inline constexpr uint8 ScriptedPriority = 192;
}


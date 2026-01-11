// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::ChaosMover
{
	namespace CVars
	{
		CHAOSMOVER_API extern bool bForceSingleThreadedGT;
		CHAOSMOVER_API extern bool bForceSingleThreadedPT;
		CHAOSMOVER_API extern bool bDrawGroundQueries;
		CHAOSMOVER_API extern bool bSkipGenerateMoveIfOverridden;
	}
};

#ifndef CHAOSMOVER_DEBUG_DRAW
#define CHAOSMOVER_DEBUG_DRAW (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)
#endif
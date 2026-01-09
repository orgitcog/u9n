// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/ChaosMoverDeveloperSettings.h"

#include "HAL/IConsoleManager.h"

namespace UE::ChaosMover
{
	namespace CVars
	{
		bool bForceSingleThreadedGT = false;
		FAutoConsoleVariableRef CVarChaosMoverForceSingleThreadedGT(TEXT("ChaosMover.ForceSingleThreadedGT"),
			bForceSingleThreadedGT, TEXT("Force updates on the game thread to be single threaded."));

		bool bForceSingleThreadedPT = true;
		FAutoConsoleVariableRef CVarChaosMoverForceSingleThreadedPT(TEXT("ChaosMover.ForceSingleThreadedPT"),
			bForceSingleThreadedPT, TEXT("Force updates on the physics thread to be single threaded."));

		bool bDrawGroundQueries = false;
		FAutoConsoleVariableRef CVarChaosMoverDrawGroundQueries(TEXT("ChaosMover.DebugDraw.GroundQueries"),
			bDrawGroundQueries, TEXT("Draw ground queries."));

		bool bSkipGenerateMoveIfOverridden = true;
		FAutoConsoleVariableRef CVarChaosMoverSkipGenerateMoveIfOverridden(TEXT("ChaosMover.Perf.SkipGenerateMoveIfOverridden"),
			bSkipGenerateMoveIfOverridden, TEXT("If true and we have a layered move fully overriding movement, then we will skip calling OnGenerateMove on the active movement mode for better performance\n")
		);
	}
}
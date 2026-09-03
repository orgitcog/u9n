// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MetaHumanFaceAnimationSolver.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanFaceAnimationSolverTest, "MetaHuman.FaceAnimationSolver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMetaHumanFaceAnimationSolverTest::RunTest(const FString& InParameters)
{
	bool bIsOK = true;

	UMetaHumanFaceAnimationSolver* Solver = LoadObject<UMetaHumanFaceAnimationSolver>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/Solver/GenericFaceAnimationSolver.GenericFaceAnimationSolver"));

	if (Solver)
	{
		bIsOK &= TestFalse(TEXT("Override device config"), Solver->bOverrideDeviceConfig);
		bIsOK &= TestNull(TEXT("Device config"), Solver->DeviceConfig);
		bIsOK &= TestFalse(TEXT("Override depth map influence"), Solver->bOverrideDepthMapInfluence);
		bIsOK &= TestFalse(TEXT("Override eye smoothness"), Solver->bOverrideEyeSolveSmoothness);
	}
	else
	{
		bIsOK &= TestTrue(TEXT("Loaded asset"), false);
	}

	return bIsOK;
}

#endif
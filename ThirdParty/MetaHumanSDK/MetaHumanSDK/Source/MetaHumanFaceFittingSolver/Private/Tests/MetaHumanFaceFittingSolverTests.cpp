// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MetaHumanFaceFittingSolver.h"
#include "MetaHumanFaceAnimationSolver.h"
#include "MetaHumanConfig.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanFaceFittingSolverTest, "MetaHuman.FaceFittingSolver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMetaHumanFaceFittingSolverTest::RunTest(const FString& InParameters)
{
	bool bIsOK = true;

	UMetaHumanFaceFittingSolver* FaceFittingSolver = LoadObject<UMetaHumanFaceFittingSolver>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/MeshFitting/GenericFaceFittingSolver.GenericFaceFittingSolver"));
	UMetaHumanFaceAnimationSolver* FaceAnimationSolver = LoadObject<UMetaHumanFaceAnimationSolver>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/Solver/GenericFaceAnimationSolver.GenericFaceAnimationSolver"));
	UMetaHumanConfig* PredictiveSolver = LoadObject<UMetaHumanConfig>(GetTransientPackage(), TEXT("/MetaHumanDepthProcessing/Solver/GenericPredictiveSolver.GenericPredictiveSolver"));

	if (FaceFittingSolver && FaceAnimationSolver && PredictiveSolver)
	{
		bIsOK &= TestFalse(TEXT("Override device config"), FaceFittingSolver->bOverrideDeviceConfig);
		bIsOK &= TestNull(TEXT("Device config"), FaceFittingSolver->DeviceConfig);
		bIsOK &= TestEqual(TEXT("Face animation solver"), FaceFittingSolver->FaceAnimationSolver.Get(), FaceAnimationSolver);
		bIsOK &= TestEqual(TEXT("Predictive solver"), FaceFittingSolver->PredictiveSolver.Get(), PredictiveSolver);
	}
	else
	{
		bIsOK &= TestTrue(TEXT("Loaded asset"), false);
	}

	return bIsOK;
}

#endif
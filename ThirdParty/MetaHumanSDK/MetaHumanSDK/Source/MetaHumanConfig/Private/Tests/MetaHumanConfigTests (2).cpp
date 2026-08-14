// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MetaHumanConfig.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMetaHumanConfigTest, "MetaHuman.Config", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FMetaHumanConfigTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("Count");
	Tests.Add("Solver-iphone12-Solver-iPhone 12"); // [Asset directory]-[Asset filename]-[Config type]-[Config name]
	Tests.Add("Solver-iphone13-Solver-iPhone 13");
	Tests.Add("Solver-stereo_hmc-Solver-Stereo HMC");
	Tests.Add("Solver-GenericPredictiveSolver-Predictive Solver-Predictive solvers");
	Tests.Add("MeshFitting-iphone12-Fitting-iPhone 12");
	Tests.Add("MeshFitting-iphone13-Fitting-iPhone 13");
	Tests.Add("MeshFitting-stereo_hmc-Fitting-Stereo HMC");
	Tests.Add("MeshFitting-Mesh2MetaHuman-Fitting-Mesh2MetaHuman");

	for (const FString& Test : Tests)
	{
		OutBeautifiedNames.Add(Test);
		OutTestCommands.Add(Test);
	}
}

bool FMetaHumanConfigTest::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;

	if (InTestCommand == TEXT("Count"))
	{
		TArray<FString> SolverFiles;
		IFileManager::Get().FindFiles(SolverFiles, *(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir() + TEXT("/Solver")), TEXT("uasset"));
		bIsOK &= TestEqual(TEXT("Number of Solver files"), SolverFiles.Num(), 4); // 3 device configs plus GenericFaceAnimationSolver

		TArray<FString> MeshFittingFiles;
		IFileManager::Get().FindFiles(MeshFittingFiles, *(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir() + TEXT("/MeshFitting")), TEXT("uasset"));
		bIsOK &= TestEqual(TEXT("Number of MeshFitting files"), MeshFittingFiles.Num(), 5); // 4 device configs plus GenericFaceFittingSolver
	}
	else
	{
		TArray<FString> Tokens;
		InTestCommand.ParseIntoArray(Tokens, TEXT("-"), true);
		bIsOK &= TestEqual<int32>(TEXT("Well formed Parameters"), Tokens.Num(), 4);

		if (bIsOK)
		{
			FString PluginName = UE_PLUGIN_NAME;
			if (InTestCommand.Contains("GenericPredictiveSolver"))
			{
				PluginName = "MetaHumanDepthProcessing";
			}
			UMetaHumanConfig* Config = LoadObject<UMetaHumanConfig>(GetTransientPackage(), *(TEXT("/" + PluginName + "/") + Tokens[0] + TEXT("/") + Tokens[1] + TEXT(".") + Tokens[1]));

			if (Config)
			{
				bIsOK &= TestEqual(TEXT("Config type"), StaticEnum<EMetaHumanConfigType>()->GetDisplayNameTextByValue(static_cast<int32>(Config->Type)).ToString(), Tokens[2]);
				bIsOK &= TestEqual(TEXT("Config name"), Config->Name, Tokens[3]);
			}
			else
			{
				bIsOK &= TestTrue(TEXT("Loaded config"), false);
			}
		}
	}

	return bIsOK;
}

#endif
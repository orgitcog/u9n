// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Interfaces/IPluginManager.h"

#include "GuiToRawControlsUtils.h"
#include "ControlsTestData.h"
#include "MetaHumanConfig.h"
#include "DNAAsset.h"
#include "DNAUtils.h"
#include "MetaHumanFaceTrackerInterface.h"
#include "Features/IModularFeatures.h"


#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGuiToRawControlsConversionTest, "MetaHuman.ControlsConversion.Gui To Raw Controls Conversion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FGuiToRawControlsConversionTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add("Face tracker conversion gui controls");
	OutTestCommands.Add("FaceTracker_InGui");

	OutBeautifiedNames.Add("Face tracker conversion min gui controls");
	OutTestCommands.Add("FaceTracker_InMinGui");

	OutBeautifiedNames.Add("Face tracker conversion max gui controls");
	OutTestCommands.Add("FaceTracker_InMaxGui");

	OutBeautifiedNames.Add("Face tracker conversion half gui controls");
	OutTestCommands.Add("FaceTracker_InHalfGui");

	OutBeautifiedNames.Add("Gui to raw utils gui controls");
	OutTestCommands.Add("GuiToRawUtils_InGui");

	OutBeautifiedNames.Add("Gui to raw utils min gui controls");
	OutTestCommands.Add("GuiToRawUtils_InMinGui");

	OutBeautifiedNames.Add("Gui to raw utils max gui controls");
	OutTestCommands.Add("GuiToRawUtils_InMaxGui");

	OutBeautifiedNames.Add("Gui to raw utils half gui controls");
	OutTestCommands.Add("GuiToRawUtils_InHalfGui");
}

#include "ConversionDataGenerator.h"

bool FGuiToRawControlsConversionTest::RunTest(const FString& InParameters)
{
	FString ConversionType;
	FString GuiControls;
	InParameters.Split("_", &ConversionType, &GuiControls);

	// Get input solve controls and expected rig controls
	TMap<FString, float> InputGuiControls;
	TMap<FString, float> ExpectedRawControls;
	if (GuiControls == "InGui")
	{
		InputGuiControls = SolveControlsTestData::InputSolveControls;
		ExpectedRawControls = SolveControlsTestData::ExpectedRigControls;
	}
	else if (GuiControls == "InMinGui")
	{
		InputGuiControls = MinControlsTestData::InputSolveControls;
		ExpectedRawControls = MinControlsTestData::ExpectedRigControls;
	}
	else if (GuiControls == "InMaxGui")
	{
		InputGuiControls = MaxControlsTestData::InputSolveControls;
		ExpectedRawControls = MaxControlsTestData::ExpectedRigControls;
	}
	else if (GuiControls == "InHalfGui")
	{
		InputGuiControls = ControlsHalfTestData::InputSolveControls;
		ExpectedRawControls = ControlsHalfTestData::ExpectedRigControls;
	}

	// Do conversion
	TMap<FString, float> OutputRawControls;

	if (ConversionType == "FaceTracker")
	{
		const FName& FeatureName = IFaceTrackerNodeImplFactory::GetModularFeatureName();
		UTEST_TRUE("Check modular feature is available", IModularFeatures::Get().IsModularFeatureAvailable(FeatureName))
	
		IFaceTrackerNodeImplFactory& TrackerPostProcessingFactory = IModularFeatures::Get().GetModularFeature<IFaceTrackerNodeImplFactory>(FeatureName);
		TSharedPtr<IFaceTrackerPostProcessingInterface> Tracker = TrackerPostProcessingFactory.CreateFaceTrackerPostProcessingImplementor();
		
		UMetaHumanConfig* DeviceConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/Solver/iphone12.iphone12"));
		check(DeviceConfig);

		const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
		const FString PathToDNA = PluginDir + TEXT("/IdentityTemplate/Face_Archetype.ardna");
		TObjectPtr<UDNAAsset> DNAAsset = GetDNAAssetFromFile(PathToDNA, GetTransientPackage());
		UTEST_NOT_NULL("DNAAsset should be valid", DNAAsset.Get());

		UTEST_TRUE("Check init post process tracker", Tracker->Init(DeviceConfig->GetSolverTemplateData(), DeviceConfig->GetSolverConfigData()));
		UTEST_TRUE("Check dna load post process tracker", Tracker->LoadDNA(DNAAsset.Get(), DeviceConfig->GetSolverHierarchicalDefinitionsData()));

		UTEST_TRUE("Convert solve controls using post processing tracker", Tracker->ConvertUIControlsToRawControls(InputGuiControls, OutputRawControls));
	}
	else if (ConversionType == "GuiToRawUtils")
	{
		OutputRawControls = GuiToRawControlsUtils::ConvertGuiToRawControls(InputGuiControls);
	}


	// Check controls are expected
	UTEST_EQUAL("Number of raw controls match expected", OutputRawControls.Num(), ExpectedRawControls.Num());

	for (const auto& ExpectedControl : ExpectedRawControls)
	{
		const float* RawValue = OutputRawControls.Find(ExpectedControl.Key);
		UTEST_NOT_NULL(FString::Format(TEXT("Raw value found for {0}"), {ExpectedControl.Key}), RawValue);

		if (RawValue)
		{
			UTEST_NEARLY_EQUAL(FString::Format(TEXT("Raw value matches expected for {0}"), {ExpectedControl.Key}), *RawValue, ExpectedControl.Value, KINDA_SMALL_NUMBER);
		}
	}

	return true;
}

#endif
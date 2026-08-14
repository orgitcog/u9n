// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "ImageCore.h"
#include "Interfaces/IPluginManager.h"
#include "Math/Color.h"
#include "Misc/AutomationTest.h"
#include "Stats/StatsMisc.h"

#include "MetaHumanFaceTextureSynthesizer.h"

//#define METAHUMAN_TEXTURESYNTHESIS_TEST_SAVEOUTPUT
#ifdef METAHUMAN_TEXTURESYNTHESIS_TEST_SAVEOUTPUT
#include "ImageUtils.h"
#include "Misc/Paths.h"
#endif


namespace UE::MetaHuman
{
	static FString GetTestModelPath()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
		if (!Plugin.IsValid())
		{
	 		return FString{};
		}
	 
		const FString TextureSynthesisModelDir = Plugin->GetContentDir() + TEXT("/TextureSynthesis/Models");
		const FString DefaultModelName(TEXT("TS-1.2-B_UE_res-128_nchr-003"));

		return TextureSynthesisModelDir / DefaultModelName;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanFaceTextureSynthesizerTest, "MetaHuman.Creator.FaceTextureSynthesizerTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMetaHumanFaceTextureSynthesizerTest::RunTest(const FString& InParams)
{
	// Test model texture size
	const int32 ExpectedModelResolution = 128;

	FMetaHumanFaceTextureSynthesizer FaceTextureSynthesizer;

	// Initialize the synthesizer
	UTEST_TRUE("Face Texture Synthesizer initialization", FaceTextureSynthesizer.Init(UE::MetaHuman::GetTestModelPath()));
	UTEST_VALID("Face Texture Synthesizer is valid", FaceTextureSynthesizer);

	// Test for the test model resolution
	UTEST_EQUAL("Face Texture Synthesizer size X", FaceTextureSynthesizer.GetTextureSizeX(), ExpectedModelResolution);
	UTEST_EQUAL("Face Texture Synthesizer size Y", FaceTextureSynthesizer.GetTextureSizeY(), ExpectedModelResolution);

	const FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams TextureSynthesisParams{
		.SkinUVFromUI = FVector2f{ 0.5f, 0.5f },
		.HighFrequencyIndex = 0,
		.MapType = FMetaHumanFaceTextureSynthesizer::EMapType::Base
	};

	// Smoke tests for synthesize functions
	FImage OutAlbedo;
	OutAlbedo.Init(FaceTextureSynthesizer.GetTextureSizeX(), FaceTextureSynthesizer.GetTextureSizeY(), FaceTextureSynthesizer.GetTextureFormat(), FaceTextureSynthesizer.GetTextureColorSpace());
	UTEST_TRUE("Synthesize albedo map", FaceTextureSynthesizer.SynthesizeAlbedo(TextureSynthesisParams, OutAlbedo));
	UTEST_EQUAL("Synthesized albedo size X", OutAlbedo.SizeX, FaceTextureSynthesizer.GetTextureSizeX());
	UTEST_EQUAL("Synthesized albedo size Y", OutAlbedo.SizeY, FaceTextureSynthesizer.GetTextureSizeY());
	UTEST_EQUAL("Synthesized albedo pixel format", OutAlbedo.Format, FaceTextureSynthesizer.GetTextureFormat());
	UTEST_EQUAL("Synthesized albedo Gamma space", OutAlbedo.GammaSpace, FaceTextureSynthesizer.GetTextureColorSpace());

	FImage OutNormal;
	OutNormal.Init(FaceTextureSynthesizer.GetTextureSizeX(), FaceTextureSynthesizer.GetTextureSizeY(), FaceTextureSynthesizer.GetTextureFormat(), FaceTextureSynthesizer.GetTextureColorSpace());
	UTEST_TRUE("Synthesize normal map", FaceTextureSynthesizer.SelectNormal(TextureSynthesisParams, OutNormal));
	UTEST_EQUAL("Synthesized normal size X", OutNormal.SizeX, FaceTextureSynthesizer.GetTextureSizeX());
	UTEST_EQUAL("Synthesized normal size Y", OutNormal.SizeY, FaceTextureSynthesizer.GetTextureSizeY());
	UTEST_EQUAL("Synthesized normal pixel format", OutNormal.Format, FaceTextureSynthesizer.GetTextureFormat());
	UTEST_EQUAL("Synthesized normal Gamma space", OutNormal.GammaSpace, FaceTextureSynthesizer.GetTextureColorSpace());

	FImage OutCavity;
	OutCavity.Init(FaceTextureSynthesizer.GetTextureSizeX(), FaceTextureSynthesizer.GetTextureSizeY(), FaceTextureSynthesizer.GetTextureFormat(), FaceTextureSynthesizer.GetTextureColorSpace());
	UTEST_TRUE("Synthesize normal map", FaceTextureSynthesizer.SelectCavity(0, OutCavity));
	UTEST_EQUAL("Synthesized cavity size X", OutCavity.SizeX, FaceTextureSynthesizer.GetTextureSizeX());
	UTEST_EQUAL("Synthesized cavity size Y", OutCavity.SizeY, FaceTextureSynthesizer.GetTextureSizeY());
	UTEST_EQUAL("Synthesized cavity pixel format", OutCavity.Format, FaceTextureSynthesizer.GetTextureFormat());
	UTEST_EQUAL("Synthesized cavity Gamma space", OutCavity.GammaSpace, FaceTextureSynthesizer.GetTextureColorSpace());

#ifdef METAHUMAN_TEXTURESYNTHESIS_TEST_SAVEOUTPUT
	UTEST_TRUE("Save synthesized albedo image", FImageUtils::SaveImageByExtension(*(FPaths::ProjectSavedDir() + FString(TEXT("MetaHumanTextureSynthesisTests_OutAlbedo.png"))), OutAlbedo));
	UTEST_TRUE("Save synthesized normal image", FImageUtils::SaveImageByExtension(*(FPaths::ProjectSavedDir() + FString(TEXT("MetaHumanTextureSynthesisTests_OutNormal.png"))), OutNormal));
	UTEST_TRUE("Save synthesized cavity image", FImageUtils::SaveImageByExtension(*(FPaths::ProjectSavedDir() + FString(TEXT("MetaHumanTextureSynthesisTests_OutCavity.png"))), OutCavity));
#endif

	// Smoke test for titan memory clean up
	FaceTextureSynthesizer.Clear();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetaHumanFaceTextureSynthesizerPerfTest, "MetaHuman.Creator.FaceTextureSynthesizerPerfTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMetaHumanFaceTextureSynthesizerPerfTest::RunTest(const FString& InParams)
{
	// Simple timing performance test for the synthesize functions

	// Initialize the synthesizer
	FMetaHumanFaceTextureSynthesizer FaceTextureSynthesizer;
	UTEST_TRUE("Face Texture Synthesizer initialization", FaceTextureSynthesizer.Init(UE::MetaHuman::GetTestModelPath()));
	UTEST_VALID("Face Texture Synthesizer is valid", FaceTextureSynthesizer);

	const FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams TextureSynthesisParams{
		.SkinUVFromUI = FVector2f{ 0.5f, 0.5f },
		.HighFrequencyIndex = 0,
		.MapType = FMetaHumanFaceTextureSynthesizer::EMapType::Base
	};

	// Timing tests
	bool bSynthesizeResult = true;
	FImage OutAlbedo;
	OutAlbedo.Init(FaceTextureSynthesizer.GetTextureSizeX(), FaceTextureSynthesizer.GetTextureSizeY(), FaceTextureSynthesizer.GetTextureFormat(), FaceTextureSynthesizer.GetTextureColorSpace());
	{
		// Time only the synthesize call		
		FScopeLogTime LogTimePtr(TEXT("FMetaHumanFaceTextureSynthesizer::SynthesizeAlbedo"),nullptr, FScopeLogTime::ScopeLog_Milliseconds);
		bSynthesizeResult = FaceTextureSynthesizer.SynthesizeAlbedo(TextureSynthesisParams, OutAlbedo);
	}
	// Check call was valid without errors
	UTEST_TRUE("Synthesize albedo map", bSynthesizeResult);

	FImage OutNormal;
	OutNormal.Init(FaceTextureSynthesizer.GetTextureSizeX(), FaceTextureSynthesizer.GetTextureSizeY(), FaceTextureSynthesizer.GetTextureFormat(), FaceTextureSynthesizer.GetTextureColorSpace());
	{
		FScopeLogTime LogTimePtr(TEXT("FMetaHumanFaceTextureSynthesizer::SelectNormal"), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
		bSynthesizeResult = FaceTextureSynthesizer.SelectNormal(TextureSynthesisParams, OutNormal);
	}
	// Check call was valid without errors
	UTEST_TRUE("Synthesize normal map", bSynthesizeResult);

	FImage OutCavity;
	OutCavity.Init(FaceTextureSynthesizer.GetTextureSizeX(), FaceTextureSynthesizer.GetTextureSizeY(), FaceTextureSynthesizer.GetTextureFormat(), FaceTextureSynthesizer.GetTextureColorSpace());
	{
		FScopeLogTime LogTimePtr(TEXT("FMetaHumanFaceTextureSynthesizer::SelectCavity"), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
		bSynthesizeResult = FaceTextureSynthesizer.SelectCavity(0, OutCavity);
	}
	// Check call was valid without errors
	UTEST_TRUE("Select cavity map", bSynthesizeResult);

	return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS

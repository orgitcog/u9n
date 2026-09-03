// Copyright Epic Games, Inc. All Rights Reserved.

#include "FramePathResolverSingleFile.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MetaHuman::Private
{
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFramePathResolverSingleFileTest,
	"MetaHuman.Capture.CaptureData.FramePathResolverSingleFile",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FFramePathResolverSingleFileTest::RunTest(const FString& InParameters)
{
	const FString FilePath = TEXT("/Some/Path/Frame_1234.png");
	const FFramePathResolverSingleFile FramePathResolver(FilePath);

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(0);
		UTEST_EQUAL("Resolved path", *ResolvedPath, *FilePath);
	}

	{
		const FString ResolvedPath = FramePathResolver.ResolvePath(20);
		UTEST_EQUAL("Resolved path", *ResolvedPath, *FilePath);
	}

	return true;
}

}

#endif // WITH_DEV_AUTOMATION_TESTS
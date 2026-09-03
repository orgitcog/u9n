// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "AutoRTFM.h"
#include "CompGeom/ConvexHull3.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoRTFMConvexHullTests, "AutoRTFM + TConvexHull3", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext)

bool FAutoRTFMConvexHullTests::RunTest(const FString& Parameters)
{
	if (!AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, TEXT("SKIPPED 'FAutoRTFMConvexHullTests' test. AutoRTFM disabled.")));
		return true;
	}

	using Vec3 = UE::Math::TVector<float>;

	AutoRTFM::Commit([this]
	{
		static const Vec3 Pyramid[] = 
		{
			{ 0.f,  0.f,  0.f},
			{20.f,  0.f,  0.f},
			{ 0.f, 20.f,  0.f},
			{20.f, 20.f,  0.f},
			{10.f, 10.f,  3.f},
		};

		UE::Geometry::TConvexHull3<float> Hull;
		double Volume = Hull.ComputeVolume(Pyramid);

		TestTrueExpr(FMath::IsNearlyEqual(Volume, 400.));
	});

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

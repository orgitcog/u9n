// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFMTesting.h"
#include "Catch2Includes.h"
#include "CoreTypes.h"
#include "CrossCUTests.h"

UE_DISABLE_OPTIMIZATION_SHIP

TEST_CASE("CrossCU.Call")
{
    AutoRTFM::Testing::Commit([&]
	{
		int Value = CrossCU::SomeFunction(0);
		REQUIRE(Value == 42);
	});
}

// Test calling a function that will have an LLVM byval() attributed parameter in another CU.
// This is to test for FORT-823033
TEST_CASE("CrossCU.LargeStruct")
{
	CrossCU::FLargeStruct Struct
	{
		{
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
			0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
			0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
		}
	};
	
	const int Expected = CrossCU::FLargeStruct::Sum(Struct);

	int Result = 0;
	AutoRTFM::Testing::Commit([&]
	{
		Result = CrossCU::FLargeStruct::Sum(Struct);
	});
	REQUIRE(Expected == Result);
}

UE_ENABLE_OPTIMIZATION_SHIP

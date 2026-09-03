// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "MassEntityTestTypes.h"
#include "MassTypeManager.h"

#define LOCTEXT_NAMESPACE "MassTest"

UE_DISABLE_OPTIMIZATION_SHIP

namespace UE::Mass::Test
{
	struct FTypeManager_Subsystem : FEntityTestBase
	{
		virtual bool InstantTest() override
		{
			const FSubsystemTypeTraits TestSubsystemTraits = FSubsystemTypeTraits::Make<UMassTestWorldSubsystem>();

			AITEST_EQUAL("Subsystem bGameThreadOnly value", TestSubsystemTraits.bGameThreadOnly, TMassExternalSubsystemTraits<UMassTestWorldSubsystem>::GameThreadOnly)
			AITEST_EQUAL("Subsystem ThreadSafeWrite value", TestSubsystemTraits.bThreadSafeWrite, TMassExternalSubsystemTraits<UMassTestWorldSubsystem>::ThreadSafeWrite)

			const FSharedFragmentTypeTraits TestSharedFragmentTraits = FSharedFragmentTypeTraits::Make<FTestSharedFragment_Int>();
			AITEST_EQUAL("Shared fragment bGameThreadOnly value", TestSubsystemTraits.bGameThreadOnly, TMassExternalSubsystemTraits<UMassTestWorldSubsystem>::GameThreadOnly)

			return true;
		}
	};
	IMPLEMENT_AI_INSTANT_TEST(FTypeManager_Subsystem, "System.Mass.TypeManager.StaticSubsystem");
} // UE::Mass::Test

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
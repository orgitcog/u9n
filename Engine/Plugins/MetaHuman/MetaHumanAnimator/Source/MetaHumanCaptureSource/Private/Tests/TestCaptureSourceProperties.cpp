// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "MetaHumanCaptureSourceSync.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TCaptureSourceProperties, "MetaHumanCaptureSource.Synchronous.MetaHumanCaptureSourcePropertyNotVisible", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TCaptureSourceProperties::RunTest(const FString& InParameters)
{
	const UClass* const UClassPtr = UMetaHumanCaptureSourceSync::StaticClass();
	UTEST_NOT_NULL(TEXT("UMetaHumanCaptureSourceSync class ptr"), UClassPtr);

	const FProperty* const CaptureSourceProperty = UClassPtr->FindPropertyByName("MetaHumanCaptureSource");
	UTEST_NOT_NULL(TEXT("MetaHumanCaptureSource property"), CaptureSourceProperty);

	// Check that the MetaHumanCaptureSource property on the synchronous capture source is not visible in blueprints or in the editor. This property
	// exists only for garbage collection purposes and so should not get in people's way and should not be editable.
	const bool bHasExpectedFlags = CaptureSourceProperty->HasAllPropertyFlags(
		EPropertyFlags::CPF_Transient
	);

	const bool bHasUnexpectedFlags = CaptureSourceProperty->HasAnyPropertyFlags(
		EPropertyFlags::CPF_BlueprintVisible |
		EPropertyFlags::CPF_Edit |
		EPropertyFlags::CPF_AssetRegistrySearchable
	);

	UTEST_TRUE(TEXT("Expected property flag check"), bHasExpectedFlags);
	UTEST_FALSE(TEXT("Unexpected property flag check"), bHasUnexpectedFlags);

	return true;
}

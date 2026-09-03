// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestDriver.h"

#include "OnlineSubsystemCatchHelper.h"
#include "Helpers/OnlineSubsystemTestFixtures.h"

#define IDENTITY_TAG "[suite_identity]"
#define EG_IDENTITY_LOGIN_TAG IDENTITY_TAG "[login]"
#define EG_IDENTITY_LOGIN_LEGACY_FLOWEOS_TAG IDENTITY_TAG "[login][.EOS]"

#define IDENTITY_TEST_CASE(x, ...) ONLINESUBSYSTEM_TEST_CASE(x, IDENTITY_TAG __VA_ARGS__)

#define IDENTITY_LEGACY_LOGIN_EOS_TEST(x, ...) ONLINESUBSYSTEM_TEST_CASE_FIXTURE(FOnlineSubsystemEOSLegacyTestFixture, x, IDENTITY_TAG __VA_ARGS__)

IDENTITY_LEGACY_LOGIN_EOS_TEST("Verify calling Identity Login with valid inputs and using legacy login flow returns the expected result(Success Case)", EG_IDENTITY_LOGIN_LEGACY_FLOWEOS_TAG)
{
	int32 NumUsersToImplicitLogin = 2;

	GetLoginPipeline(NumUsersToImplicitLogin);

	RunToCompletion();
}

IDENTITY_TEST_CASE("Verify calling Identity Login with valid inputs returns the expected result(Success Case)", EG_IDENTITY_LOGIN_TAG)
{
	int32 NumUsersToImplicitLogin = 2;

	GetLoginPipeline(NumUsersToImplicitLogin);

	RunToCompletion();
}
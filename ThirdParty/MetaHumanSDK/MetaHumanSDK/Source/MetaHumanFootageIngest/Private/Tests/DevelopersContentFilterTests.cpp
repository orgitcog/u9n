// Copyright Epic Games, Inc. All Rights Reserved.

#include "DevelopersContentFilter.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_Wide_NoDevelopers, "MetaHuman.DevelopersContentFilter.Wide.NoDevelopers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_Wide_Developers, "MetaHuman.DevelopersContentFilter.Wide.Developers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_Wide_OtherDevelopers, "MetaHuman.DevelopersContentFilter.Wide.OtherDevelopers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_Wide_AllDevelopers, "MetaHuman.DevelopersContentFilter.Wide.AllDevelopers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_NotWide_NoDevelopers, "MetaHuman.DevelopersContentFilter.NotWide.NoDevelopers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_NotWide_Developers, "MetaHuman.DevelopersContentFilter.NotWide.Developers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_NotWide_OtherDevelopers, "MetaHuman.DevelopersContentFilter.NotWide.OtherDevelopers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopersContentFilterTest_NotWide_AllDevelopers, "MetaHuman.DevelopersContentFilter.NotWide.AllDevelopers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::SmokeFilter)

enum class EPathKind
{
	Wide,
	NotWide
};

static FString CreateAssetName(const EPathKind InPathKind)
{
	return InPathKind == EPathKind::Wide ? TEXT("ᎷӇĆ𝕊_ꓔ𝕣𝜶ո𝑠íȅ𝚗𝞽") : TEXT("MHCS_Transient");
}

class FTestFixture
{
public:

	FTestFixture(
		UE::MetaHuman::EDevelopersContentVisibility InDevelopersContentVisibility,
		UE::MetaHuman::EOtherDevelopersContentVisibility InOtherDevelopersContentVisibility
	);

	bool UserDeveloperVisible(EPathKind InPathKind) const;
	bool OtherDeveloperVisible(EPathKind InPathKind) const;
	bool NonDeveloperVisible(EPathKind InPathKind) const;
	bool RootDeveloperVisible(EPathKind InPathKind) const;

private:
	FString BaseDeveloperPath;
	FString OtherDeveloperPath;
	FString UserDeveloperPath;
	FString NonDeveloperPath;
	UE::MetaHuman::FDevelopersContentFilter DevelopersContentFilter;
};

FTestFixture::FTestFixture(
	const UE::MetaHuman::EDevelopersContentVisibility InDevelopersContentVisibility,
	const UE::MetaHuman::EOtherDevelopersContentVisibility InOtherDevelopersContentVisibility
) :
	BaseDeveloperPath(TEXT("/Game/Developers")),
	OtherDeveloperPath(BaseDeveloperPath + TEXT("/someotheruser/")),
	UserDeveloperPath(BaseDeveloperPath + TEXT("/") + *FPaths::GameUserDeveloperFolderName() + TEXT("/")),
	NonDeveloperPath(TEXT("/Game/SomePlace")),
	DevelopersContentFilter(InDevelopersContentVisibility, InOtherDevelopersContentVisibility)
{
}

bool FTestFixture::UserDeveloperVisible(const EPathKind InPathKind) const
{
	return DevelopersContentFilter.PassesFilter(UserDeveloperPath / CreateAssetName(InPathKind));
}

bool FTestFixture::OtherDeveloperVisible(const EPathKind InPathKind) const
{
	return DevelopersContentFilter.PassesFilter(OtherDeveloperPath / CreateAssetName(InPathKind));
}

bool FTestFixture::NonDeveloperVisible(const EPathKind InPathKind) const
{
	return DevelopersContentFilter.PassesFilter(NonDeveloperPath / CreateAssetName(InPathKind));
}

bool FTestFixture::RootDeveloperVisible(const EPathKind InPathKind) const
{
	return DevelopersContentFilter.PassesFilter(BaseDeveloperPath / CreateAssetName(InPathKind));
}

bool FDevelopersContentFilterTest_Wide_NoDevelopers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::NotVisible, EOtherDevelopersContentVisibility::NotVisible);
	const EPathKind PathKind = EPathKind::Wide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));

	UTEST_FALSE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	return true;
}

bool FDevelopersContentFilterTest_Wide_Developers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::Visible, EOtherDevelopersContentVisibility::NotVisible);
	const EPathKind PathKind = EPathKind::Wide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	UTEST_FALSE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));

	return true;
}

bool FDevelopersContentFilterTest_Wide_OtherDevelopers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::NotVisible, EOtherDevelopersContentVisibility::Visible);
	const EPathKind PathKind = EPathKind::Wide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));

	UTEST_FALSE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	return true;
}

bool FDevelopersContentFilterTest_Wide_AllDevelopers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::Visible, EOtherDevelopersContentVisibility::Visible);
	const EPathKind PathKind = EPathKind::Wide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	return true;
}

bool FDevelopersContentFilterTest_NotWide_NoDevelopers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::NotVisible, EOtherDevelopersContentVisibility::NotVisible);
	const EPathKind PathKind = EPathKind::NotWide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));

	UTEST_FALSE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	return true;
}

bool FDevelopersContentFilterTest_NotWide_Developers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::Visible, EOtherDevelopersContentVisibility::NotVisible);
	const EPathKind PathKind = EPathKind::NotWide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	UTEST_FALSE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));

	return true;
}

bool FDevelopersContentFilterTest_NotWide_OtherDevelopers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::NotVisible, EOtherDevelopersContentVisibility::Visible);
	const EPathKind PathKind = EPathKind::NotWide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));

	UTEST_FALSE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));
	UTEST_FALSE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	return true;
}

bool FDevelopersContentFilterTest_NotWide_AllDevelopers::RunTest(const FString& InParameters)
{
	using namespace UE::MetaHuman;

	const FTestFixture TestFixture(EDevelopersContentVisibility::Visible, EOtherDevelopersContentVisibility::Visible);
	const EPathKind PathKind = EPathKind::NotWide;

	UTEST_TRUE(TEXT("Non-developer check"), TestFixture.NonDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("User developer check"), TestFixture.UserDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("Other developer check"), TestFixture.OtherDeveloperVisible(PathKind));
	UTEST_TRUE(TEXT("Root developer check"), TestFixture.RootDeveloperVisible(PathKind));

	return true;
}

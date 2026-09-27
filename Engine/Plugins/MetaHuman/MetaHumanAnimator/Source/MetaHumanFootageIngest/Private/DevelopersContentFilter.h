// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

namespace UE::MetaHuman
{
enum class EDevelopersContentVisibility
{
	Visible,
	NotVisible
};

enum class EOtherDevelopersContentVisibility
{
	Visible,
	NotVisible
};

class FDevelopersContentFilter
{
public:
	FDevelopersContentFilter(
		EDevelopersContentVisibility InDevelopersContentVisibility,
		EOtherDevelopersContentVisibility InOtherDevelopersContentVisibility
	);

	bool PassesFilter(const FString& InAssetPath) const;
	EDevelopersContentVisibility GetDevelopersContentVisibility() const;
	EOtherDevelopersContentVisibility GetOtherDevelopersContentVisibility() const;

private:
	FString BaseDeveloperPath;
	TArray<ANSICHAR> BaseDeveloperPathAnsi;
	FString UserDeveloperPath;
	TArray<ANSICHAR> UserDeveloperPathAnsi;
	EDevelopersContentVisibility DevelopersContentVisibility;
	EOtherDevelopersContentVisibility OtherDevelopersContentVisibility;
};

}

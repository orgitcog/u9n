// Copyright Epic Games, Inc. All Rights Reserved.

#include "DevelopersContentFilter.h"

#include "Misc/TextFilterUtils.h"
#include "Misc/Paths.h"

namespace UE::MetaHuman
{

FDevelopersContentFilter::FDevelopersContentFilter(const EDevelopersContentVisibility InDevelopersContentVisibility, const EOtherDevelopersContentVisibility InOtherDevelopersContentVisibility) :
	// Trailing slash is important for filtering out content directly in the developers folder
	BaseDeveloperPath(TEXT("/Game/Developers/")),
	UserDeveloperPath(BaseDeveloperPath + FPaths::GameUserDeveloperFolderName()),
	DevelopersContentVisibility(InDevelopersContentVisibility),
	OtherDevelopersContentVisibility(InOtherDevelopersContentVisibility)
{
	// We try to create ANSI forms of the base paths, so we can fall back to ANSI string comparison in the event the package path string is not wide.
	// If these conversions fail it simply means the paths contain non-ANSI characters and can not be compared against ANSI strings anyway, so this
	// is not an error, as the comparison against an ANSI package path will fail anyway. 
	TextFilterUtils::TryConvertWideToAnsi(BaseDeveloperPath, BaseDeveloperPathAnsi);
	TextFilterUtils::TryConvertWideToAnsi(UserDeveloperPath, UserDeveloperPathAnsi);
}

bool FDevelopersContentFilter::PassesFilter(const FString& InAssetPath) const
{
	const bool bShowDevelopersContent = DevelopersContentVisibility == EDevelopersContentVisibility::Visible;
	const bool bShowOtherDevelopersContent = OtherDevelopersContentVisibility == EOtherDevelopersContentVisibility::Visible;

	bool bIsDeveloperContent = !TextFilterUtils::NameStrincmp(
		*InAssetPath,
		BaseDeveloperPath,
		BaseDeveloperPathAnsi,
		BaseDeveloperPath.Len()
	);

	if (bIsDeveloperContent)
	{
		// Use the parent path so we're not including any trailing slash for the comparison against the base developer path. This is needed to avoid
		// matching against content directly in the /Game/Developers folder.
		FString ParentPath = FPaths::GetPath(InAssetPath);
		bIsDeveloperContent = ParentPath.StartsWith(BaseDeveloperPath);

		if (bIsDeveloperContent)
		{
			if (ParentPath.StartsWith(UserDeveloperPath))
			{
				// User developer content
				return bShowDevelopersContent;
			}
			else
			{
				// Other user's developer content
				return bShowDevelopersContent && bShowOtherDevelopersContent;
			}
		}

		return bShowDevelopersContent;
	}

	// All other content should not be filtered out
	return true;
}

EDevelopersContentVisibility FDevelopersContentFilter::GetDevelopersContentVisibility() const
{
	return DevelopersContentVisibility;
}

EOtherDevelopersContentVisibility FDevelopersContentFilter::GetOtherDevelopersContentVisibility() const
{
	return OtherDevelopersContentVisibility;
}

}

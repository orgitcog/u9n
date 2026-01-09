// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCaptureSourceAutoReimport.h"

#include "Logging/LogMacros.h"
#include "Templates/Greater.h"

DEFINE_LOG_CATEGORY_STATIC(LogCaptureSourceAutoReimport, Log, All);

namespace UE::MetaHuman
{
static bool CaptureSourceDirectoryConfigIsEqual(const FAutoReimportDirectoryConfig& Lhs, const FAutoReimportDirectoryConfig& Rhs)
{
	// We use a named equivalence function rather than rolling our own operator==, just to avoid conflicts if such an operator gets added to FAutoReimportDirectoryConfig

	if (Lhs.SourceDirectory != Rhs.SourceDirectory)
	{
		return false;
	}

	if (Lhs.MountPoint != Rhs.MountPoint)
	{
		return false;
	}

	if (Lhs.Wildcards.Num() != Rhs.Wildcards.Num())
	{
		return false;
	}

	for (int32 WildcardIndex = 0; WildcardIndex < Lhs.Wildcards.Num(); ++WildcardIndex)
	{
		const FAutoReimportWildcard& LhsWildcard = Lhs.Wildcards[WildcardIndex];
		const FAutoReimportWildcard& RhsWildcard = Rhs.Wildcards[WildcardIndex];

		if (LhsWildcard.Wildcard != RhsWildcard.Wildcard)
		{
			return false;
		}

		if (LhsWildcard.bInclude != RhsWildcard.bInclude)
		{
			return false;
		}
	}

	return true;
}

TArray<FAutoReimportDirectoryConfig> CaptureSourceUpdateAutoReimportExclusion(
	const FString& InSourceDirectory,
	const FString& InWildcard,
	const TArray<FAutoReimportDirectoryConfig>& InDirectoryConfigs
)
{
	TArray<FAutoReimportDirectoryConfig> DirectoryConfigs = InDirectoryConfigs;

	// Make sure we always have a trailing slash
	const FString SanitizedSourceDirectory = InSourceDirectory / FString();

	bool bWildcardAddedForThisSourceDirectory = false;
	TSet<int32> DirectoryConfigIndicesToRemove;
	constexpr bool bIsIncludeWildcard = false;

	// We need to consolidate the existing directory configs so that we only have a single entry for the requested
	// wildcard under a single source directory. We have previously serialized configs which contain duplicate entries
	// and so we need to clean them up and remove them, as we can't have multiple directory watchers for the same source
	// directory.

	for (int32 DirectoryConfigIndex = 0; DirectoryConfigIndex < DirectoryConfigs.Num(); ++DirectoryConfigIndex)
	{
		FAutoReimportDirectoryConfig& DirectoryConfig = DirectoryConfigs[DirectoryConfigIndex];

		// Make sure we always have a trailing slash
		const FString SanitizedConfigSourceDirectory = DirectoryConfig.SourceDirectory / FString();

		if (SanitizedConfigSourceDirectory != SanitizedSourceDirectory)
		{
			continue;
		}

		int32 NumPreviousWildcards = 0;
		const int NumWildcards = DirectoryConfig.Wildcards.Num();
		TSet<int32> WildcardIndicesToRemove;

		for (int32 WildcardIndex = 0; WildcardIndex < NumWildcards; ++WildcardIndex)
		{
			const FAutoReimportWildcard& ConfigWildcard = DirectoryConfig.Wildcards[WildcardIndex];

			if (ConfigWildcard.Wildcard != InWildcard)
			{
				continue;
			}

			if (!bWildcardAddedForThisSourceDirectory && ConfigWildcard.bInclude == bIsIncludeWildcard)
			{
				// Desired wildcard already exists (with the correct include flag), keep it to preserve the ordering
				bWildcardAddedForThisSourceDirectory = true;
			}
			else
			{
				// Mark duplicates or wildcards with the wrong include flag to be removed
				WildcardIndicesToRemove.Add(WildcardIndex);
			}

			// Any wildcard matching the expected string counts, regardless of whether it has the correct include flag
			++NumPreviousWildcards;
		}

		WildcardIndicesToRemove.Sort(TGreater<>());

		for (const int32 IndexToRemove : WildcardIndicesToRemove)
		{
			const FString RemovedWildcard = DirectoryConfig.Wildcards[IndexToRemove].Wildcard;
			DirectoryConfig.Wildcards.RemoveAt(IndexToRemove);
			UE_LOG(LogCaptureSourceAutoReimport, Display, TEXT("Removed redundant auto reimport wildcard \"%s\" from source directory %s"), *RemovedWildcard, *DirectoryConfig.SourceDirectory);
		}

		if (!bWildcardAddedForThisSourceDirectory)
		{
			// No existing wildcard found for this source directory, so we add it to this config
			DirectoryConfig.Wildcards.Emplace(InWildcard, bIsIncludeWildcard);
			bWildcardAddedForThisSourceDirectory = true;
		}

		// We only want to remove the config if the wildcard was previously present (i.e. an old config we need to remove). We
		// don't want to remove any other entries the user might have in their config.
		if (NumPreviousWildcards > 0 && DirectoryConfig.Wildcards.IsEmpty())
		{
			// No more wildcards, so we mark this config for removal, otherwise it will cause a watch collision (same 
			// source directory as the config we are moving the wildcard to).
			DirectoryConfigIndicesToRemove.Add(DirectoryConfigIndex);
		}
	}

	DirectoryConfigIndicesToRemove.Sort(TGreater<>());

	for (const int32 IndexToRemove : DirectoryConfigIndicesToRemove)
	{
		const FString RemovedSourceDirectory = DirectoryConfigs[IndexToRemove].SourceDirectory;
		DirectoryConfigs.RemoveAt(IndexToRemove);
		UE_LOG(LogCaptureSourceAutoReimport, Display, TEXT("Removed empty auto reimport directory config: %s"), *RemovedSourceDirectory);
	}

	// No directory config was found for the requested source directory, so add it
	if (!bWildcardAddedForThisSourceDirectory)
	{
		FAutoReimportDirectoryConfig DirectoryConfig;
		DirectoryConfig.SourceDirectory = SanitizedSourceDirectory;
		DirectoryConfig.Wildcards.Emplace(InWildcard, bIsIncludeWildcard);
		DirectoryConfigs.Emplace(MoveTemp(DirectoryConfig));
	}

	return DirectoryConfigs;
}

bool CaptureSourceDirectoryConfigsAreDifferent(const TArray<FAutoReimportDirectoryConfig>& Lhs, const TArray<FAutoReimportDirectoryConfig>& Rhs)
{
	if (Lhs.Num() != Rhs.Num())
	{
		return true;
	}

	for (int32 DirectoryIndex = 0; DirectoryIndex < Lhs.Num(); ++DirectoryIndex)
	{
		const FAutoReimportDirectoryConfig& LhsConfig = Lhs[DirectoryIndex];
		const FAutoReimportDirectoryConfig& RhsConfig = Rhs[DirectoryIndex];

		if (!CaptureSourceDirectoryConfigIsEqual(LhsConfig, RhsConfig))
		{
			return true;
		}
	}

	return false;
}

} // namespace UE::MetaHuman

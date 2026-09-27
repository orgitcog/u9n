// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "MetaHumanTypesEditor.h"

enum class EMetaHumanQualityLevel : uint8;

namespace UE::MetaHuman
{
class FSourceMetaHuman;
class FInstalledMetaHuman;

// Reason for performing an update (currently only version difference, but this could be extended).
struct FAssetUpdateReason
{
	FMetaHumanAssetVersion OldVersion;
	FMetaHumanAssetVersion NewVersion;

	// Whether the update is a breaking change (change in major version number)
	bool IsBreakingChange() const
	{
		return NewVersion.Major != OldVersion.Major;
	}
};

// Description of an asset update operation
struct FAssetOperationPath
{
	FString SourceFile;
	FString DestinationFile;
	FString SourcePackage;
	FString DestinationPackage;
};

// List of relative asset paths to be Added, Replaced etc. as part of the current import action
struct FAssetOperations
{
	TArray<FAssetOperationPath> Add;
	TArray<FAssetOperationPath> Replace;
	TArray<FAssetOperationPath> Skip;
	TArray<FAssetOperationPath> Update;
	TArray<FAssetUpdateReason> UpdateReasons;
};

enum class EImportOperationUserResponse: int
{
	OK,
	Cancel,
	BulkImport
};

/** Display a warning dialog informing the user that upgrade may impact upon incompatible MetaHumans in the project
 * @param SourceMetaHuman The MetaHuman being imported
 * @param IncompatibleCharacters MetaHumans in the project that are incompatible with the proposed import
 * @param InstalledMetaHumans All MetaHumans installed in the project
 */
EImportOperationUserResponse DisplayUpgradeWarning(const FSourceMetaHuman& SourceMetaHuman, const TSet<FString>& IncompatibleCharacters, const TArray<FInstalledMetaHuman>& InstalledMetaHumans, const TSet<FString>& AvailableMetaHumans, const FAssetOperations& AssetOperations);

bool DisplayQualityLevelChangeWarning(EMetaHumanQualityLevel Source, EMetaHumanQualityLevel Target);
}

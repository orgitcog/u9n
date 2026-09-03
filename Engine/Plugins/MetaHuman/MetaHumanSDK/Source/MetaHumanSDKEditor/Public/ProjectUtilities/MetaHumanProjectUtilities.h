// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanTypesEditor.h"

#include "Containers/UnrealString.h"
#include "Misc/NotNull.h"

enum class EMetaHumanQualityLevel : uint8;
class FZipArchiveReader;

namespace UE::MetaHuman
{
struct FMetaHumanImportDescription;
class IMetaHumanBulkImportHandler;
class IMetaHumanImportAutomationHandler;

// Class that handles the layout and filenames of a MetaHuman that has been added to a project.
class METAHUMANSDKEDITOR_API FInstalledMetaHuman
{
public:
	FInstalledMetaHuman(const FString& InName, const FString& InCharacterFilePath, const FString& InCommonFilePath);

	const FString& GetName() const
	{
		return Name;
	}

	FString GetRootAsset() const;

	FName GetRootPackage() const;

	FMetaHumanVersion GetVersion() const;

	EMetaHumanQualityLevel GetQualityLevel() const;

	FString GetCommonAssetPath() const;

	// Finds MetaHumans in the destination of a given import
	static TArray<FInstalledMetaHuman> GetInstalledMetaHumans(const FString& CharactersFolder, const FString& CommonAssetsFolder);

private:
	FString Name;
	FString CharacterFilePath;
	FString CommonFilePath;
	FString CharacterAssetPath;
	FString CommonAssetPath;
};

class FMetaHumanProjectUtilities
{
public:
	// Disable UI and enable automation of user input for headless testing
	static void METAHUMANSDKEDITOR_API EnableAutomation(IMetaHumanImportAutomationHandler* Handler);

	// Disable UI and enable automation of user input for headless testing
	static void METAHUMANSDKEDITOR_API SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler);

	// Main entry-point used by Quixel Bridge
	static void METAHUMANSDKEDITOR_API ImportMetaHuman(const FMetaHumanImportDescription& AssetImportDescription);

	// Provide the Url for the versioning service to use
	static void METAHUMANSDKEDITOR_API OverrideVersionServiceUrl(const FString& BaseUrl);

	// Returns a list of all MetaHumans in the project
	static TArray<FInstalledMetaHuman> METAHUMANSDKEDITOR_API GetInstalledMetaHumans();

	// Copy the MetaHuman version metadata from the Source Object to the Destination Object
	static void METAHUMANSDKEDITOR_API CopyVersionMetadata(TNotNull<class UObject*> InSourceObject, TNotNull<class UObject*> InDestObject);
};
} // namespace UE::MetaHuman

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProjectUtilities/MetaHumanProjectUtilities.h"

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "Misc/TVariant.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FSourceMetaHuman;
class FZipArchiveReader;
class UGroomBindingAsset;
class UMetaHumanAssetReport;
class USkeletalMesh;

namespace UE::MetaHuman
{
// TODO: replace these interfaces with delegates
/**
 * Interface for classes handling automation of the MetaHuman import process for e.g. scripting and tests
 */
class IMetaHumanImportAutomationHandler
{
public:
	virtual ~IMetaHumanImportAutomationHandler() = default;

	/**
	 *
	 * @param IncompatibleMetaHumans Array of names of MetaHumans in project that are not compatible with the requested import
	 * @param UpdatedFiles Array of filenames of the assets that would be updated by the requested import
	 * @return Whether to continue with the import process
	 */
	virtual bool ShouldContinueWithBreakingMetaHumans(const TArray<FString>& IncompatibleMetaHumans, const TArray<FString>& UpdatedFiles) = 0;
};

/**
 * Interface for classes handling the processing of bulk import operations. The only supported implementation is for
 * Quixel Bridge to handle the "re-import all MetaHumans" operation.
 */
class IMetaHumanBulkImportHandler
{
public:
	virtual ~IMetaHumanBulkImportHandler() = default;

	/**
	 * This is an asynchronous operation. This function must return immediately and the import operation that called it
	 * will then immediately terminate.
	 * @param MetaHumanIds A list of the Quixel IDs of the MetaHumans to be imported
	 */
	virtual void DoBulkImport(const TArray<FString>& MetaHumanIds) = 0;
};

/**
 * Struct describing a MetaHuman item in QuixelBridge
 */
struct FQuixelAccountMetaHumanEntry
{
	FString Name; // MetaHuman name
	FString Id; // Quixel ID
	bool bIsLegacy; // Does this MetaHuman require an Upgrade before it can be used
	FString Version; // The version of MHC used to create this character
};

/**
 * Struct giving parameters for an import operation for a MetaHuman character
 */
struct FMetaHumanImportDescription
{
	inline static const FString DefaultDestinationPath = TEXT("/Game/MetaHumans");

	FString CharacterPath; // The file path to the source unique assets for this import operation
	FString CommonPath; // The file path to the source common assets for this import operation
	FString CharacterName; // The name of the MetaHuman to import (expected to match the final part of CharacterPath)
	FString QuixelId; // The ID of the character being imported
	bool bIsBatchImport; // If this is part of a batch import
	FString SourcePath = DefaultDestinationPath; // The asset path that the exporter has written the assets out to
	FString DestinationPath = DefaultDestinationPath; // The asset path to install the MetaHuman to in the project
	TArray<FQuixelAccountMetaHumanEntry> AccountMetaHumans; // All the MetaHumans that are included in the user's account. Used to show which MetaHumans can be upgraded
	bool bForceUpdate = false; // Ignore asset version metadata and update all assets
	bool bWarnOnQualityChange = false; // Warn if the user is importing a MetaHuman at a different quality level to the existing MetaHuman in the scene.
	TSharedPtr<FZipArchiveReader> Archive = nullptr; // If present, import from this archive rather than the CharacterPath
	UMetaHumanAssetReport* Report = nullptr; // If present, log messages to this report
};

/**
 * Struct describing the source of a set of files to import as a MetaHuman Asset Group
 */
struct FFileSource
{
	enum class ECopyResult: uint32
	{
		Success,
		MissingSource,
		Failure
	};

	/**
	 * Constructs a FileSource that is a local folder on disk
	 * @param FilePath The file path to use to resolve relative paths to source files
	 */
	FFileSource(const FString& FilePath);

	/**
	 * Constructs a FileSource that is a zip file
	 * @param Archive The zip archive containing files
	 */
	FFileSource(const TSharedPtr<FZipArchiveReader>& Archive, const FString& FilePath=TEXT(""));

	/**
	 * @param SourceFilePath The relative path to the source file
	 * @param DestinationFilePath The absolute path to copy the source file to
	 * @return The status of the copy operation
	 */
	ECopyResult CopySingleFile(const FString& SourceFilePath, const FString& DestinationFilePath) const;

	/**
	 *
	 * @param SourceFilePath The relative path to the JSON file
	 * @return The JsonObject if loaded correctly, nullptr otherwise
	 */
	TSharedPtr<FJsonObject> ReadJson(const FString& SourceFilePath) const;

private:
	TVariant<FString, TSharedPtr<FZipArchiveReader>> Root;

	FString SubFolder;
};

/**
 * Struct giving parameters for an import operation for an Asset Group
 */
struct FAssetGroupImportDescription
{
	FString Name; // The Name of the AssetGroup
	FString TargetRootAssetPath; // The target asset path to import to. e.g. /Game/Folder/MyGrooms
	FString SourceRootAssetPath; // The path to the assets in their source project
	FFileSource FileSource; // The FileSource containing the asset group
	UMetaHumanAssetReport* Report; // The Report to populate with the results of the import operation
};

/**
 * Utility class handling the import or MetaHumans into a project.
 */
class METAHUMANSDKEDITOR_API FMetaHumanImport
{
public:
	// Delegate called when a MetaHuman starts being imported into the project. If this delegate is bound and returns false it will skip the import process
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnImportStarted, const FSourceMetaHuman& InSourceMetaHuman);
	FOnImportStarted OnImportStartedDelegate;

	// Delegate called to check if given asset or file should be imported. This can be used to import a subset of assets of files into the project
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnShouldImportAssetOrFile, const FSourceMetaHuman& InSourceMetaHuman, const FString& InDestPath, bool bInIsFile);
	FOnShouldImportAssetOrFile OnShouldImportAssetOrFileDelegate;

	// Delegate called at the end of the import process. It can be used to perform extra processing on the assets and files that were imported
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnImportEnded, const FSourceMetaHuman& InSourceMetaHuman, const FInstalledMetaHuman& InTargetMetaHuman);
	FOnImportEnded OnImportEndedDelegate;

	/**
	 * This method imports a MetaHuman into a project, handling asset unloading and re-loading and warning the
	 * user of any asset-version mismatches for assets shared by multiple MetaHumans in the project.
	 *
	 * @param ImportDescription the parameters for the import
	 * @return The main asset for the imported item
	 */
	TOptional<UObject*> ImportMetaHuman(const FMetaHumanImportDescription& ImportDescription) const;


	/**
	 * This method imports an AssetGroup into a project, handling asset unloading and re-loading.
	 *
	 * @param ImportDescription the parameters for the import
	 * @return The main asset for the imported item
	 */
	TOptional<UObject*> ImportAssetGroup(const FAssetGroupImportDescription ImportDescription) const;

	/**
	 *
	 * @param Handler the automation handler to be used for subsequent calls to ImportMetaHuman
	 */
	void SetAutomationHandler(IMetaHumanImportAutomationHandler* Handler);

	/**
	 *
	 * @param Handler bulk import handler to be used if a version conflict requiring the bulk import of multiple
	 * MetaHumans is required.
	 */
	void SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler);

	/**
	 *
	 * @return Get the singleton instance of this class
	 */
	static TSharedPtr<FMetaHumanImport> Get();

private:
	FMetaHumanImport() = default;
	IMetaHumanImportAutomationHandler* AutomationHandler{nullptr};
	IMetaHumanBulkImportHandler* BulkImportHandler{nullptr};
	static TSharedPtr<FMetaHumanImport> MetaHumanImportInst;
};
}

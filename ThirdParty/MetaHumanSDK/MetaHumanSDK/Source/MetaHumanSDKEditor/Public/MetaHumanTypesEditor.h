// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"

enum class EMetaHumanQualityLevel : uint8;
class FZipArchiveReader;

namespace UE::MetaHuman
{
// Helper structure describing the normal file and asset structure for Legacy MetaHumans. Used during the import of
// a MetaHuman to a project
struct METAHUMANSDKEDITOR_API FImportPaths
{
	inline static const FString MetaHumansFolderName = TEXT("MetaHumans");
	inline static const FString CommonFolderName = TEXT("Common");

	// Construction
	explicit FImportPaths(const FString& InSourceRootFilePath, const FString& InSourceAssetPath, const FString& InDestinationCommonAssetPath, const FString& InDestinationCharacterAssetPath);

	/**
	 * @param Filename The name of a file containing an asset that is part of a MetaHuman asset group
	 * @return The default name of the asset contained in that file
	 */
	static FString FilenameToAssetName(const FString& Filename);

	/**
	 * @param AssetName The name an asset that is part of a MetaHuman asset group
	 * @return The default name of the file containing that asset
	 */
	static FString AssetNameToFilename(const FString& AssetName);

	/**
	 * Convert from the name of the MetaHuman to the default name for the main Blueprint Asset
	 * @param CharacterName The MetaHuman Name to convert
	 * @return The name of the default blueprint asset
	 */
	FString CharacterNameToBlueprintAssetPath(const FString& CharacterName) const;

	/**
	 * Given a relative path from the manifest, calculate the full path to the corresponding source file.
	 * @param RelativeFilePath The path relative to the root
	 * @return The full path to the source file
	 */
	FString GetSourceFile(const FString& RelativeFilePath) const;

	/**
	 * Given a relative path from the manifest, calculate the full path to the corresponding destination file.
	 * @param RelativeFilePath The path relative to the root
	 * @return The full path to the destination file
	 */
	FString GetDestinationFile(const FString& RelativeFilePath) const;

	/**
	 * Given a relative path from the manifest, calculate the full asset path to the corresponding destination package.
	 * @param RelativeFilePath The path relative to the root
	 * @return The full asset path to the destination package
	 */
	FString GetDestinationPackage(const FString& RelativeFilePath) const;

	/**
	 * Given a relative path from the manifest, calculate the full asset path to the corresponding source package.
	 * @param RelativeFilePath The path relative to the root
	 * @return The full asset path to the source package
	 */
	FString GetSourcePackage(const FString& RelativeFilePath) const;

	FString SourceRootFilePath;
	FString SourceRootAssetPath;

	FString DestinationCharacterRootFilePath;
	FString DestinationCharacterFilePath;
	FString DestinationCommonFilePath;
	FString DestinationCharacterAssetPath;
	FString DestinationCommonAssetPath;
};

// Representation of a MetaHuman Version. This is a simple semantic-versioning style version number that is stored
// in a Json file at a specific location in the directory structure that MetaHumans use.
struct METAHUMANSDKEDITOR_API FMetaHumanVersion
{
	// Currently default initialisation == 0.0.0 which is not a valid version.
	FMetaHumanVersion() = default;

	// Construction
	explicit FMetaHumanVersion(const FString& VersionString);
	explicit FMetaHumanVersion(const int InMajor, const int InMinor, const int InRevision);

	// Comparison operators
	friend METAHUMANSDKEDITOR_API bool operator <(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator >(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator <=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator >=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator ==(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator !=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right);

	// Hash function
	friend METAHUMANSDKEDITOR_API uint32 GetTypeHash(FMetaHumanVersion Version);

	/**
	 * Check for asset compatibility (major version matches) between two MetaHumans
	 * @param Other The other MetaHuman to compare against
	 */
	bool IsCompatible(const FMetaHumanVersion& Other) const;

	/**
	 * Converts the Version to a string.
	 * @return The canonical string representation of the MetaHuman version.
	 */
	FString AsString() const;

	/**
	 * Reads the MetaHuman Version from the file that comes with an exported MetaHuman
	 * @param VersionFilePath Path to the VersionInfo.txt file that is part of an exported MetaHuman
	 * @return The MetaHumanVersion contained in the file
	 */
	static FMetaHumanVersion ReadFromFile(const FString& VersionFilePath);

	/**
	 * Reads the MetaHuman Version from the file that comes with a MetaHuman that is packaged in a MetaHuman archive
	 * @param VersionFilePath Path to the VersionInfo.txt file that is part of a MetaHuman package
	 * @return The MetaHumanVersion contained in the file
	 */
	static FMetaHumanVersion ReadFromArchive(const FString& VersionFilePath, FZipArchiveReader* Archive);

	int32 Major = 0;
	int32 Minor = 0;
	int32 Revision = 0;
};

/*
 * Represents the Asset Version stored in MetaData on MetaHuman assets. Major version changes imply breaking changes.
 */
struct METAHUMANSDKEDITOR_API FMetaHumanAssetVersion
{
	// Construction
	explicit FMetaHumanAssetVersion(const FString& VersionString);
	explicit FMetaHumanAssetVersion(const int InMajor, const int InMinor);

	// Comparison operators
	friend METAHUMANSDKEDITOR_API bool operator <(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator>(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator<=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator>=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator ==(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right);
	friend METAHUMANSDKEDITOR_API bool operator!=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right);

	// Hash function
	friend METAHUMANSDKEDITOR_API uint32 GetTypeHash(FMetaHumanAssetVersion Version);

	/**
	 * Converts the Version to a string.
	 * @return The canonical string representation of the MetaHuman Asset version.
	 */
	FString AsString() const;

	int32 Major = 0;
	int32 Minor = 0;
};

// Class that handles the layout on-disk of a MetaHuman being used as the source of an Import operation
// Gives us a single place to handle simple path operations, filenames etc.
class METAHUMANSDKEDITOR_API FSourceMetaHuman
{
public:
	FSourceMetaHuman(const FString& InCharacterPath, const FString& InCommonPath, const FString& InName);
	FSourceMetaHuman(FZipArchiveReader* Reader);

	/**
	 * @return The path to the source assets (i.e. DNA files) for this MetaHuman
	 */
	const FString GetSourceAssetsPath() const;

	/**
	 * @return The name of the MetaHuman
	 */
	const FString& GetName() const;

	/**
	 * @return The MetaHuman version of this MetaHuman
	 */
	const FMetaHumanVersion& GetVersion() const;

	/**
	 * @return Is this MetaHuman exported for use in UEFN
	 */
	bool IsUEFN() const;

	/**
	 * @return The MetaHuman quality level that this MetaHuman was generated at
	 */
	EMetaHumanQualityLevel GetQualityLevel() const;

private:
	FString CharacterPath;
	FString CommonPath;
	FString Name;
	FMetaHumanVersion Version;
};
}

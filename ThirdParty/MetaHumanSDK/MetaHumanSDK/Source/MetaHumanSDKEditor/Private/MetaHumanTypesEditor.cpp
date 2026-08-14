// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanTypesEditor.h"

#include "MetaHumanTypes.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"
#include "ProjectUtilities/MetaHumanProjectUtilities.h"

#include "FileUtilities/ZipArchiveReader.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace UE::MetaHuman
{
FImportPaths::FImportPaths(const FString& InSourceRootFilePath, const FString& InSourceAssetPath, const FString& InDestinationCommonAssetPath, const FString& InDestinationCharacterAssetPath)
{
	// The locations we are importing files from
	SourceRootFilePath = FPaths::GetPath(InSourceRootFilePath);

	// The project location the assets came from
	SourceRootAssetPath = InSourceAssetPath;

	// Destination asset paths in the project for the MetaHuman
	DestinationCommonAssetPath = InDestinationCommonAssetPath;
	DestinationCharacterAssetPath = InDestinationCharacterAssetPath;

	// Corresponding file paths on disk for those assets
	DestinationCommonFilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(DestinationCommonAssetPath));
	DestinationCharacterFilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(DestinationCharacterAssetPath));

	// The folder to search for other MetaHumans
	DestinationCharacterRootFilePath = FPaths::GetPath(DestinationCharacterFilePath);
}


FString FImportPaths::FilenameToAssetName(const FString& Filename)
{
	return FString::Format(TEXT("{0}.{0}"), {FPaths::GetBaseFilename(Filename)});
}

FString FImportPaths::AssetNameToFilename(const FString& AssetName)
{
	return FString::Format(TEXT("{0}{1}"), {AssetName, FPackageName::GetAssetPackageExtension()});
}

FString FImportPaths::CharacterNameToBlueprintAssetPath(const FString& CharacterName) const
{
	return DestinationCharacterAssetPath / FString::Format(TEXT("BP_{0}.BP_{0}"), {CharacterName});
}

/** Given a relative path from the manifest, calculate the full path to the corresponding source file. */
FString FImportPaths::GetSourceFile(const FString& RelativeFilePath) const
{
	return FPaths::Combine(SourceRootFilePath, RelativeFilePath);
}

/** Given a relative path from the manifest, calculate the full path to the corresponding destination file. */
FString FImportPaths::GetDestinationFile(const FString& RelativeFilePath) const
{
	FString RootPath;
	FString ChildPath;
	RelativeFilePath.Split(TEXT("/"), &RootPath, &ChildPath);
	const FString DestinationRoot = RootPath == CommonFolderName ? DestinationCommonFilePath : DestinationCharacterFilePath;
	return DestinationRoot / ChildPath;
}

/** Given a relative path from the manifest, calculate the asset path to the corresponding destination package. */
FString FImportPaths::GetDestinationPackage(const FString& RelativeFilePath) const
{
	FString RootPath;
	FString ChildPath;
	FPaths::GetBaseFilename(RelativeFilePath, false).Split(TEXT("/"), &RootPath, &ChildPath);
	const FString DestinationRoot = RootPath == CommonFolderName ? DestinationCommonAssetPath : DestinationCharacterAssetPath;
	return DestinationRoot / ChildPath;
}

/** Given a relative path from the manifest, calculate the asset path to the corresponding destination package. */
FString FImportPaths::GetSourcePackage(const FString& RelativeFilePath) const
{
	return SourceRootAssetPath / FPaths::GetBaseFilename(RelativeFilePath, false);
}


FMetaHumanVersion::FMetaHumanVersion(const int InMajor, const int InMinor, const int InRevision)
	: Major(InMajor)
	, Minor(InMinor)
	, Revision(InRevision)
{
}

// Comparison operators
bool operator <(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
{
	return Left.Major < Right.Major || (Left.Major == Right.Major && (Left.Minor < Right.Minor || (Left.Minor == Right.Minor && Left.Revision < Right.Revision)));
}

bool operator >(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
{
	return Right < Left;
}

bool operator <=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
{
	return !(Left > Right);
}

bool operator >=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
{
	return !(Left < Right);
}

bool operator ==(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
{
	return Right.Major == Left.Major && Right.Minor == Left.Minor && Right.Revision == Left.Revision;
}

bool operator !=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
{
	return !(Left == Right);
}

// Hash function
uint32 GetTypeHash(FMetaHumanVersion Version)
{
	return (Version.Major << 20) + (Version.Minor << 10) + Version.Revision;
}

// Currently MetaHumans are compatible so long as they are from the same major version. In the future, compatibility
// between versions may be more complex or require inspecting particular assets.
bool FMetaHumanVersion::IsCompatible(const FMetaHumanVersion& Other) const
{
	return (Major || Minor) && Major == Other.Major;
}

FString FMetaHumanVersion::AsString() const
{
	return FString::Format(TEXT("{0}.{1}.{2}"), {Major, Minor, Revision});
}

FMetaHumanVersion FMetaHumanVersion::ReadFromFile(const FString& VersionFilePath)
{
	// This is the old behaviour. We can probably do better than this.
	if (!IFileManager::Get().FileExists(*VersionFilePath))
	{
		return FMetaHumanVersion(TEXT("0.5.1"));
	}
	const FString VersionTag = TEXT("MetaHumanVersion");
	FString VersionInfoString;
	if (FFileHelper::LoadFileToString(VersionInfoString, *VersionFilePath))
	{
		TSharedPtr<FJsonObject> VersionInfoObject;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VersionInfoString), VersionInfoObject))
		{
			return FMetaHumanVersion(VersionInfoObject->GetStringField(VersionTag));
		}
	}
	// Invalid file
	return {};
}

FMetaHumanVersion FMetaHumanVersion::ReadFromArchive(const FString& VersionFilePath, FZipArchiveReader* Archive)
{
	TArray<uint8> FileContents;
	// This is the old behaviour. We can probably do better than this.
	if (!Archive->TryReadFile(VersionFilePath, FileContents))
	{
		return FMetaHumanVersion(TEXT("0.5.1"));
	}
	FString VersionInfoString(FileContents.Num(), reinterpret_cast<ANSICHAR*>(FileContents.GetData()));
	TSharedPtr<FJsonObject> VersionInfoObject;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VersionInfoString), VersionInfoObject))
	{
		return FMetaHumanVersion(VersionInfoObject->GetStringField(TEXT("MetaHumanVersion")));
	}
	// Invalid file
	return {};
}

FMetaHumanAssetVersion::FMetaHumanAssetVersion(const FString& VersionString)
{
	FString MajorPart;
	FString MinorPart;
	VersionString.Split(TEXT("."), &MajorPart, &MinorPart);
	Major = FCString::Atoi(*MajorPart);
	Minor = FCString::Atoi(*MinorPart);
}

FMetaHumanAssetVersion::FMetaHumanAssetVersion(const int InMajor, const int InMinor) :
	Major(InMajor),
	Minor(InMinor)
{
}

bool operator <(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
{
	return Left.Major < Right.Major || (Left.Major == Right.Major && Left.Minor < Right.Minor);
}

bool operator>(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
{
	return Right < Left;
}

bool operator<=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
{
	return !(Left > Right);
}

bool operator>=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
{
	return !(Left < Right);
}

bool operator ==(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
{
	return Right.Major == Left.Major && Right.Minor == Left.Minor;
}

bool operator!=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
{
	return !(Left == Right);
}

uint32 GetTypeHash(FMetaHumanAssetVersion Version)
{
	return (Version.Major << 16) + Version.Minor;
}

FString FMetaHumanAssetVersion::AsString() const
{
	return FString::Format(TEXT("{0}.{1}"), {Major, Minor});
}


FSourceMetaHuman::FSourceMetaHuman(const FString& InCharacterPath, const FString& InCommonPath, const FString& InName)
	: CharacterPath(FPaths::ConvertRelativePathToFull(InCharacterPath))
	, CommonPath(FPaths::ConvertRelativePathToFull(InCommonPath))
	, Name(InName)
{
	const FString VersionFilePath = CharacterPath / TEXT("VersionInfo.txt");
	Version = FMetaHumanVersion::ReadFromFile(VersionFilePath);
}

FSourceMetaHuman::FSourceMetaHuman(FZipArchiveReader* Reader)
{
	TArray<uint8> FileContents;
	Reader->TryReadFile(TEXT("Manifest.json"), FileContents);
	FString ReadData(FileContents.Num(), reinterpret_cast<ANSICHAR*>(FileContents.GetData()));

	FMetaHumanAssetDescription SourceDescription;
	FJsonObjectConverter::JsonObjectStringToUStruct(ReadData, &SourceDescription);

	// Read manifest
	Name = SourceDescription.Name.ToString();
	CommonPath = "Common";

	EMetaHumanQualityLevel QualityLevel = SourceDescription.Details.PlatformsIncluded.Num() ? SourceDescription.Details.PlatformsIncluded[0] : EMetaHumanQualityLevel::Cinematic;

	// slightly messy to avoid API changes for a hotfix.
	// use "::" to denote an archive path and then convert the Quality Level enum into a tier value
	CharacterPath = FString::Format(TEXT("::Tier{0}"), {3 - static_cast<int>(QualityLevel)});

	const FString VersionFilePath = CharacterPath / TEXT("VersionInfo.txt");
	Version = FMetaHumanVersion::ReadFromArchive(VersionFilePath, Reader);
}


const FString FSourceMetaHuman::GetSourceAssetsPath() const
{
	if (CharacterPath.StartsWith("::"))
	{
		return Name / TEXT("SourceAssets");
	}
	return CharacterPath / TEXT("SourceAssets");
}

const FString& FSourceMetaHuman::GetName() const
{
	return Name;
}

const FMetaHumanVersion& FSourceMetaHuman::GetVersion() const
{
	return Version;
}

bool FSourceMetaHuman::IsUEFN() const
{
	return CharacterPath.Contains(TEXT("asset_uefn"));
}

EMetaHumanQualityLevel FSourceMetaHuman::GetQualityLevel() const
{
	if (CharacterPath.Contains(TEXT("Tier0")))
	{
		// For UEFN Tier0 is High, for UE Tier0 is cinematic
		if (!IsUEFN())
		{
			return EMetaHumanQualityLevel::Cinematic;
		}
		return EMetaHumanQualityLevel::High;
	}
	if (CharacterPath.Contains(TEXT("Tier1")))
	{
		// Tier 1 only exists for UE
		return EMetaHumanQualityLevel::High;
	}
	if (CharacterPath.Contains(TEXT("Tier2")))
	{
		return EMetaHumanQualityLevel::Medium;
	}
	else
	{
		return EMetaHumanQualityLevel::Low;
	}
}
}

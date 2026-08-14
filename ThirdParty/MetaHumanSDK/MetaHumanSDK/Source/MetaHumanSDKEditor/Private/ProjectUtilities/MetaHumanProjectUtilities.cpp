// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectUtilities/MetaHumanProjectUtilities.h"

#include "Import/MetaHumanImport.h"
#include "MetaHumanSDKSettings.h"
#include "MetaHumanTypes.h"
#include "MetaHumanVersionService.h"

#include "EditorAssetLibrary.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "MetaHumanProjectUtilities"

namespace UE::MetaHuman
{
FMetaHumanVersion::FMetaHumanVersion(const FString& VersionString)
{
	TArray<FString> ParsedVersionString;
	const int32 NumSections = VersionString.ParseIntoArray(ParsedVersionString, TEXT("."));
	verify(NumSections == 3);
	if (NumSections == 3)
	{
		Major = FCString::Atoi(*ParsedVersionString[0]);
		Minor = FCString::Atoi(*ParsedVersionString[1]);
		Revision = FCString::Atoi(*ParsedVersionString[2]);
	}
}

FInstalledMetaHuman::FInstalledMetaHuman(const FString& InName, const FString& InCharacterFilePath, const FString& InCommonFilePath)
	: Name(InName)
	, CharacterFilePath(InCharacterFilePath)
	, CommonFilePath(InCommonFilePath)
	, CharacterAssetPath(FPackageName::FilenameToLongPackageName(InCharacterFilePath))
	, CommonAssetPath(FPackageName::FilenameToLongPackageName(InCommonFilePath))
{
}

FString FInstalledMetaHuman::GetRootAsset() const
{
	return CharacterAssetPath / FString::Format(TEXT("BP_{0}.BP_{0}"), {Name});
}

FName FInstalledMetaHuman::GetRootPackage() const
{
	return FName(CharacterAssetPath / FString::Format(TEXT("BP_{0}"), {Name}));
}

EMetaHumanQualityLevel FInstalledMetaHuman::GetQualityLevel() const
{
	static const FName MetaHumanAssetQualityLevelKey = TEXT("MHExportQuality");
	if (const UObject* Asset = LoadObject<UObject>(nullptr, *GetRootAsset()))
	{
		if (const TMap<FName, FString>* Metadata = FMetaData::GetMapForObject(Asset))
		{
			if (const FString* AssetQualityMetaData = Metadata->Find(MetaHumanAssetQualityLevelKey))
			{
				if (*AssetQualityMetaData == TEXT("Cinematic"))
				{
					return EMetaHumanQualityLevel::Cinematic;
				}
				if (*AssetQualityMetaData == TEXT("High"))
				{
					return EMetaHumanQualityLevel::High;
				}
				if (*AssetQualityMetaData == TEXT("Medium"))
				{
					return EMetaHumanQualityLevel::Medium;
				}
			}
		}
	}
	return EMetaHumanQualityLevel::Low;
}

TArray<FInstalledMetaHuman> FInstalledMetaHuman::GetInstalledMetaHumans(const FString& CharactersFolder, const FString& CommonAssetsFolder)
{
	TArray<FInstalledMetaHuman> FoundMetaHumans;
	const FString ProjectMetaHumanPath = CharactersFolder / TEXT("*");
	TArray<FString> DirectoryList;
	IFileManager::Get().FindFiles(DirectoryList, *ProjectMetaHumanPath, false, true);

	for (const FString& Directory : DirectoryList)
	{
		const FString CharacterName = FPaths::GetCleanFilename(Directory);
		FInstalledMetaHuman FoundMetaHuman(CharacterName, CharactersFolder / CharacterName, CommonAssetsFolder);
		if (UEditorAssetLibrary::DoesAssetExist(FPackageName::ObjectPathToPackageName(FoundMetaHuman.GetRootAsset())))
		{
			FoundMetaHumans.Emplace(std::move(FoundMetaHuman));
		}
	}
	return FoundMetaHumans;
}

FMetaHumanVersion FInstalledMetaHuman::GetVersion() const
{
	const FString VersionFilePath = CharacterFilePath / TEXT("VersionInfo.txt");
	return FMetaHumanVersion::ReadFromFile(VersionFilePath);
}

FString FInstalledMetaHuman::GetCommonAssetPath() const
{
	return CommonAssetPath;
}

// External APIs
void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::EnableAutomation(IMetaHumanImportAutomationHandler* Handler)
{
	FMetaHumanImport::Get()->SetAutomationHandler(Handler);
}

void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler)
{
	FMetaHumanImport::Get()->SetBulkImportHandler(Handler);
}

void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::ImportMetaHuman(const FMetaHumanImportDescription& ImportDescription)
{
	FMetaHumanImport::Get()->ImportMetaHuman(ImportDescription);
}

void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::OverrideVersionServiceUrl(const FString& BaseUrl)
{
	SetServiceUrl(BaseUrl);
}

TArray<FInstalledMetaHuman> METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::GetInstalledMetaHumans()
{
	TArray<FInstalledMetaHuman> InstalledMetaHumans;

	const UMetaHumanSDKSettings* MetaHumanSDKSettings = GetDefault<UMetaHumanSDKSettings>();
	check(MetaHumanSDKSettings);

	// TODO: Read this reference to "Common" from the settings or FMetaHumanAssetImportDescription so we don't have hard-coded values here
	// TODO: Add error logs in case the conversion fails, which indicates the values set by the user are not valid paths in the project
	FString CommonInstallPath;
	if (FPackageName::TryConvertLongPackageNameToFilename(FMetaHumanImportDescription::DefaultDestinationPath / TEXT("Common"), CommonInstallPath))
	{
		// Convert to absolute paths here to make GetInstalledMetaHumans return absolute paths for everything
		CommonInstallPath = FPaths::ConvertRelativePathToFull(CommonInstallPath);

		FString CinematicMetaHumansInstallPath;
		if (FPackageName::TryConvertLongPackageNameToFilename(MetaHumanSDKSettings->CinematicImportPath.Path, CinematicMetaHumansInstallPath))
		{
			CinematicMetaHumansInstallPath = FPaths::ConvertRelativePathToFull(CinematicMetaHumansInstallPath);
			InstalledMetaHumans += FInstalledMetaHuman::GetInstalledMetaHumans(CinematicMetaHumansInstallPath, CommonInstallPath);
		}

		if (MetaHumanSDKSettings->CinematicImportPath.Path != MetaHumanSDKSettings->OptimizedImportPath.Path)
		{
			FString OptimizedMetaHumanInstallPath;
			if (FPackageName::TryConvertLongPackageNameToFilename(MetaHumanSDKSettings->OptimizedImportPath.Path, OptimizedMetaHumanInstallPath))
			{
				OptimizedMetaHumanInstallPath = FPaths::ConvertRelativePathToFull(OptimizedMetaHumanInstallPath);
				InstalledMetaHumans += FInstalledMetaHuman::GetInstalledMetaHumans(OptimizedMetaHumanInstallPath, CommonInstallPath);
			}
		}
	}

	return InstalledMetaHumans;
}

void FMetaHumanProjectUtilities::CopyVersionMetadata(TNotNull<UObject*> InSourceObject, TNotNull<UObject*> InDestObject)
{
	TNotNull<UPackage*> DestPackage = InDestObject->GetOutermost();
	FMetaData& DestMetadata = DestPackage->GetMetaData();

	// Get all source metadata
	TMap<FName, FString>* SourceMap = FMetaData::GetMapForObject(InSourceObject);
	if (!SourceMap)
	{
		return;
	}

	const FName VersionTag("MHAssetVersion");
	FString* Version = SourceMap->Find(VersionTag);
	if (Version != nullptr)
	{
		DestMetadata.SetValue(InDestObject, VersionTag, **Version);
	}

	const FName AlwaysUpdateTag("MHAlwaysUpdateOnImport");
	FString* AlwaysUpdate = SourceMap->Find(AlwaysUpdateTag);
	if (AlwaysUpdate != nullptr)
	{
		DestMetadata.SetValue(InDestObject, AlwaysUpdateTag, **AlwaysUpdate);
	}
}

}
#undef LOCTEXT_NAMESPACE

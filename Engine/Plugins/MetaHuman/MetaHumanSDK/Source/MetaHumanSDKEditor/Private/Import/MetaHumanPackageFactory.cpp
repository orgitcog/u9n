// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPackageFactory.h"

#include "Import/MetaHumanImport.h"
#include "MetaHumanAssetReport.h"
#include "MetaHumanSDKEditor.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"
#include "UI/SImportSummary.h"

#include "EngineAnalytics.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GroomBindingAsset.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Logging/StructuredLog.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "MetaHumanPackageFactory"

UMetaHumanPackageFactory::UMetaHumanPackageFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	bEditAfterNew = false;
	bText = false;
	SupportedClass = UObject::StaticClass();
	Formats.Add(TEXT("mhpkg;MetaHuman Package file"));
}

FText UMetaHumanPackageFactory::GetToolTip() const
{
	return LOCTEXT("MetaHumanPackageDescription", "A package containing MetaHuman assets");
}

bool UMetaHumanPackageFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename) == TEXT("mhpkg");
}

UObject* UMetaHumanPackageFactory::ImportObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, bool& OutCanceled)
{
	using namespace UE::MetaHuman;
	if (!IsValid(InOuter))
	{
		UE_LOGFMT(LogMetaHumanSDK, Error, "The import destination provided is not valid");
		return nullptr;
	}

	TStrongObjectPtr<UObject> Destination(InOuter);

	// Read the manifest from the archive
	IFileHandle* ArchiveFile = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Filename);

	if (!ArchiveFile)
	{
		UE_LOGFMT(LogMetaHumanSDK, Error, "Can not open the requested archive file {0}", Filename);
	}

	TSharedPtr<FZipArchiveReader> ZipReader = MakeShared<FZipArchiveReader>(ArchiveFile);
	TSet<FString> UnProcessedFiles(ZipReader->GetFileNames());

	TArray<uint8> FileContents;
	FMetaHumanMultiArchiveDescription ArchiveContents;

	if (ZipReader->TryReadFile(TEXT("ArchiveContents.json"), FileContents))
	{
		FString ReadData(FileContents.Num(), reinterpret_cast<ANSICHAR*>(FileContents.GetData()));
		FJsonObjectConverter::JsonObjectStringToUStruct(ReadData, &ArchiveContents);
		UnProcessedFiles.Remove(TEXT("ArchiveContents.json"));
		// The extra top-level Manifest.json included for Fab
		UnProcessedFiles.Remove(TEXT("Manifest.json"));
	}
	else
	{
		ArchiveContents.ContainedArchives.Add(TEXT(""));
	}

	FFormatNamedArguments Args;
	UObject* MainObject = nullptr;
	TArray<TSharedPtr<FImportResult>> ImportResults;

	if(ZipReader->GetFileNames().ContainsByPredicate([](const FString& PackageName){ return FPaths::GetBaseFilename(PackageName).StartsWith(TEXT("WI_"));}) && !FModuleManager::Get().IsModuleLoaded(TEXT("MetaHumanCharacterEditor")))
	{
		TStrongObjectPtr<UMetaHumanAssetReport> Report(NewObject<UMetaHumanAssetReport>());
		Report->AddError({LOCTEXT("WardrobeItemPluginsNotLoaded", "This package contains a MetaHuman Wardrobe Item and can not be imported without the MetaHuman Creator plugin enabled. Open the Plugin Editor and enable the \"MetaHuman Creator\" plugin to allow the import of this asset.")});
		UnProcessedFiles.Empty();
		ArchiveContents.ContainedArchives.Empty();
		ImportResults.Add({MakeShared<FImportResult>(Report, TStrongObjectPtr<UObject>(nullptr))});
	}

	bool bIsFirst = true;
	for (const FString& FileRoot : ArchiveContents.ContainedArchives)
	{
		UObject* CurrentObject = nullptr;
		FMetaHumanAssetDescription SourceDescription;
		TStrongObjectPtr<UMetaHumanAssetReport> Report(NewObject<UMetaHumanAssetReport>());
		ON_SCOPE_EXIT
		{
			ImportResults.Add({MakeShared<FImportResult>(Report, TStrongObjectPtr<UObject>(CurrentObject))});
		};

		TArray<uint8> ManifestFileContents;
		Args.Add(TEXT("ManifestPath"), FText::FromString(FileRoot / TEXT("Manifest.json")));
		if (!ZipReader->TryReadFile(FileRoot / TEXT("Manifest.json"), ManifestFileContents))
		{
			Report->AddError({FText::Format(LOCTEXT("MissingManifest", "The package does not contain a valid manifest at {ManifestPath}"), Args)});
			continue;
		}

		UnProcessedFiles.Remove(FileRoot / TEXT("Manifest.json"));
		FString ReadData(ManifestFileContents.Num(), reinterpret_cast<ANSICHAR*>(ManifestFileContents.GetData()));
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(ReadData, &SourceDescription))
		{
			Report->AddError({FText::Format(LOCTEXT("InvalidManifest", "The manifest at {ManifestPath} can not be parsed"), Args)});
			continue;
		}

		if (bIsFirst)
		{
			AnalyticsEvent(TEXT("ArchiveImport"), {
								{TEXT("AssetType"), UEnum::GetDisplayValueAsText(SourceDescription.AssetType).ToString()},
								{TEXT("NumAssets"), ArchiveContents.ContainedArchives.Num()},
							});
		}
		bIsFirst = false;

		// Ensure required plugins are loaded
		if (SourceDescription.AssetType == EMetaHumanAssetType::Character && !FModuleManager::Get().IsModuleLoaded(TEXT("MetaHumanCharacterEditor")))
		{
			Report->AddError({LOCTEXT("CharacterPluginsNotLoaded", "MetaHuman Characters can not be imported without the MetaHuman Creator plugin enabled. Open the Plugin Editor and enable the \"MetaHuman Creator\" plugin to allow the import of this asset.")});
			UnProcessedFiles.Empty();
			break;
		}

		if (SourceDescription.AssetType == EMetaHumanAssetType::CharacterAssembly && !FModuleManager::Get().IsModuleLoaded(TEXT("LiveLink")))
		{
			Report->AddError({LOCTEXT("AssemblyPluginsNotLoaded", "MetaHuman Assemblies can not be imported without the Live Link plugin enabled. Open the Plugin Editor and enable the \"Live Link\" plugin to allow the import of this asset.")});
			UnProcessedFiles.Empty();
			break;
		}

		if (SourceDescription.AssetType == EMetaHumanAssetType::OutfitClothing && !FModuleManager::Get().IsModuleLoaded(TEXT("ChaosOutfitAssetEditor")))
		{
			Report->AddError({LOCTEXT("OutfitPluginsNotLoaded", "Outfit Assets can not be imported without the Chaos Outfit Asset plugin enabled. Open the Plugin Editor and enable the \"Chaos Outfit Asset\" plugin to allow the import of this asset.")});
			UnProcessedFiles.Empty();
			break;
		}

		if (SourceDescription.AssetType == EMetaHumanAssetType::CharacterAssembly)
		{
			check(FileRoot.IsEmpty()); // Multi asset MH Archives not supported
			FString SourcePath = FPaths::GetPath(FPaths::GetPath(SourceDescription.DependentPackages[0].ToString()));
			FMetaHumanImportDescription ImportParams{
				SourceDescription.Name.ToString(),
				TEXT("Common"),
				SourceDescription.Name.ToString(),
				"",
				false,
				SourcePath,
				FMetaHumanImportDescription::DefaultDestinationPath,
				{},
				false,
				false,
				ZipReader,
				Report.Get()
			};

			if (TOptional<UObject*> Result = FMetaHumanImport::Get()->ImportMetaHuman(ImportParams))
			{
				CurrentObject = Result.GetValue();
			}
		}
		else
		{
			FAssetGroupImportDescription ImportParams{
				SourceDescription.Name.ToString(),
				FPaths::GetPath(Destination->GetPathName()),
				FPaths::GetPath(SourceDescription.DependentPackages[0].ToString()),
				{ZipReader, FileRoot},
				Report.Get()
			};

			if (TOptional<UObject*> Result = FMetaHumanImport::Get()->ImportAssetGroup(ImportParams))
			{
				CurrentObject = Result.GetValue();
			}
		}

		TArray<uint8> FileListFileContents;
		Args.Add(TEXT("FileListPath"), FText::FromString(FileRoot / TEXT("FileList.json")));
		if (!ZipReader->TryReadFile(FileRoot / TEXT("FileList.json"), FileListFileContents))
		{
			Report->AddError({FText::Format(LOCTEXT("MissingFileList", "The package does not contain a valid FileList at {FileListPath}"), Args)});
			continue;
		}
		UnProcessedFiles.Remove(FileRoot / TEXT("FileList.json"));

		FMetaHumanArchiveContents FilesList;
		FString FileListReadData(FileListFileContents.Num(), reinterpret_cast<ANSICHAR*>(FileListFileContents.GetData()));
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(FileListReadData, &FilesList))
		{
			Report->AddError({FText::Format(LOCTEXT("InvalidFileList", "The FileList at {FileListPath} can not be parsed"), Args)});
			continue;
		}

		// Account for all files in the FileList for this package
		for (const FMetaHumanArchiveEntry& Entry : FilesList.Files)
		{
			UnProcessedFiles.Remove(FileRoot / Entry.Path);
		}

		if (!MainObject)
		{
			MainObject = CurrentObject;
		}
	}

	if (ArchiveContents.ContainedArchives.IsEmpty())
	{
		// We didn't find anything to import
		TStrongObjectPtr<UMetaHumanAssetReport> Report(NewObject<UMetaHumanAssetReport>());
		Report->AddError({LOCTEXT("MissingItems", "The package does not contain any importable items.")});
		ImportResults.Add({MakeShared<FImportResult>(Report, TStrongObjectPtr<UObject>(nullptr))});
	}

	if (!UnProcessedFiles.IsEmpty())
	{
		TStrongObjectPtr<UMetaHumanAssetReport> Report(NewObject<UMetaHumanAssetReport>());
		for (const FString& FileName : UnProcessedFiles)
		{
			Args.Add(TEXT("FileName"), FText::FromString(FileName));
			Report->AddError({FText::Format(LOCTEXT("UnprocessedItem", "The package contains the file {FileName} which was not used during the import process."), Args)});
		}
		ImportResults.Add({MakeShared<FImportResult>(Report, TStrongObjectPtr<UObject>(nullptr))});
	}

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	TSharedRef<SImportSummary> ReportView = SNew(UE::MetaHuman::SImportSummary).ImportResults(ImportResults);
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(ReportView, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(ReportView);
	}

	if (!MainObject)
	{
		OutCanceled = true;
	}

	return MainObject;
}

#undef LOCTEXT_NAMESPACE

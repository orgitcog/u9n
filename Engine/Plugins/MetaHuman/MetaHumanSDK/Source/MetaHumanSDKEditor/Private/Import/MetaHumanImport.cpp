// Copyright Epic Games, Inc. All Rights Reserved.

#include "Import/MetaHumanImport.h"

#include "Import/MetaHumanImportUI.h"
#include "MetaHumanAssetReport.h"
#include "MetaHumanSDKEditor.h"
#include "MetaHumanSDKSettings.h"
#include "MetaHumanTypes.h"
#include "MetaHumanTypesEditor.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"
#include "ProjectUtilities/MetaHumanProjectUtilities.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "EngineAnalytics.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FileUtilities/ZipArchiveReader.h"
#include "FileUtilities/ZipArchiveWriter.h"
#include "GroomBindingAsset.h"
#include "HAL/FileManager.h"
#include "IContentBrowserSingleton.h"
#include "Internationalization/Text.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/StructuredLog.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "PackageTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Linker.h"
#include "UObject/MetaData.h"
#include "UObject/Object.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "MetaHumanImport"

extern UNREALED_API UEditorEngine* GEditor;

namespace UE::MetaHuman
{
FFileSource::FFileSource(const FString& FilePath) :
	Root(TInPlaceType<FString>(), FilePath)
{
}

FFileSource::FFileSource(const TSharedPtr<FZipArchiveReader>& Archive, const FString& InSubFolder) :
	Root(TInPlaceType<TSharedPtr<FZipArchiveReader>>(), Archive),
	SubFolder(InSubFolder)
{
}

FFileSource::ECopyResult FFileSource::CopySingleFile(const FString& SourceFilePath, const FString& DestinationFilePath) const
{
	TArray<uint8> FileContents;
	bool bFound = false;
	if (Root.IsType<TSharedPtr<FZipArchiveReader>>())
	{
		bFound = Root.Get<TSharedPtr<FZipArchiveReader>>()->TryReadFile(SubFolder / *SourceFilePath, FileContents);
	}
	else
	{
		bFound = IFileManager::Get().FileExists(*(Root.Get<FString>() / SourceFilePath));
	}
	if (!bFound)
	{
		return ECopyResult::MissingSource;
	}

	bool bSuccess = false;
	if (Root.IsType<TSharedPtr<FZipArchiveReader>>())
	{
		bSuccess = FFileHelper::SaveArrayToFile(FileContents, *DestinationFilePath, &IFileManager::Get(), FILEWRITE_EvenIfReadOnly);
	}
	else
	{
		bSuccess = IFileManager::Get().Copy(*DestinationFilePath, *(Root.Get<FString>() / SourceFilePath), true, true) == COPY_OK;
	}
	if (!bSuccess)
	{
		return ECopyResult::Failure;
	}

	return ECopyResult::Success;
}


TSharedPtr<FJsonObject> FFileSource::ReadJson(const FString& SourceFilePath) const
{
	FString FileContents;
	if (Root.IsType<TSharedPtr<FZipArchiveReader>>())
	{
		TArray<uint8> FileBytes;
		if (!Root.Get<TSharedPtr<FZipArchiveReader>>()->TryReadFile(SubFolder / SourceFilePath, FileBytes))
		{
			return {};
		}
		FFileHelper::BufferToString(FileContents, FileBytes.GetData(), FileBytes.Num());
	}
	else
	{
		const FString FullFilePath = Root.Get<FString>() / SourceFilePath;
		if (!IFileManager::Get().FileExists(*FullFilePath))
		{
			return {};
		}
		FFileHelper::LoadFileToString(FileContents, *FullFilePath);
	}

	if (FileContents.IsEmpty())
	{
		return {};
	}

	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FileContents), JsonObject))
	{
		return JsonObject;
	}

	return {};
}

/*
 * Helper classes to handle the import process. These are just containers to maintain a bundle of state for the duration of
 * a single import operation.
 */
class FImportContext
{
public:
	FImportContext(UMetaHumanAssetReport* InReport, const FFileSource& InFileSource):
		Report(InReport),
		FileSource(InFileSource)
	{
	}

	void AddMessageWithMessageBox(const ELogVerbosity::Type& VerbosityLevel, const FText& Message) const
	{
		check(VerbosityLevel == ELogVerbosity::Error || VerbosityLevel == ELogVerbosity::Warning);
		const EAppMsgCategory Category = VerbosityLevel == ELogVerbosity::Error ? EAppMsgCategory::Error : EAppMsgCategory::Warning;
		FMessageDialog::Open(Category, EAppMsgType::Ok, Message);
		AddMessage(VerbosityLevel, Message);
	}

	void AddMessage(const ELogVerbosity::Type& VerbosityLevel, const FText& Message) const
	{
		// Mainly required because UE_LOGFMT is a macro...
		switch (VerbosityLevel)
		{
		case ELogVerbosity::Type::Error:
			if (Report)
			{
				Report->AddError({Message});
			}
			else
			{
				UE_LOGFMT(LogMetaHumanSDK, Error, "{Message}", Message.ToString());
			}
			break;
		case ELogVerbosity::Type::Warning:
			if (Report)
			{
				Report->AddWarning({Message});
			}
			else
			{
				UE_LOGFMT(LogMetaHumanSDK, Warning, "{Message}", Message.ToString());
			}
			break;
		case ELogVerbosity::Type::Display:
			if (Report)
			{
				Report->AddInfo({Message});
			}
			else
			{
				UE_LOGFMT(LogMetaHumanSDK, Display, "{Message}", Message.ToString());
			}
			break;
		default:
			if (Report)
			{
				Report->AddVerbose({Message});
			}
			else
			{
				UE_LOGFMT(LogMetaHumanSDK, Verbose, "{Message}", Message.ToString());
			}
			break;
		}
	}

	bool CopySingleFile(const FString& SourceFilePath, const FString& DestinationFilePath, bool bIsOptional = false) const
	{
		switch (FileSource.CopySingleFile(SourceFilePath, DestinationFilePath))
		{
		case FFileSource::ECopyResult::Success:
			return true;
		case FFileSource::ECopyResult::MissingSource:
			if (!bIsOptional)
			{
				AddMessage(ELogVerbosity::Type::Warning, FText::Format(LOCTEXT("FileCopyNotFoundWarning", "Failed to find expected file {0}."), FText::FromString(SourceFilePath)));
			}
			return false;
		case FFileSource::ECopyResult::Failure:
			AddMessage(ELogVerbosity::Type::Error, FText::Format(LOCTEXT("FileCopyFailureError", "Failed to copy file {0} to {1}."), FText::FromString(SourceFilePath), FText::FromString(DestinationFilePath)));
			return false;
		}
		return false;
	}


	void CopyFiles(const FAssetOperations& AssetOperations, TArray<FString> DestinationAssetRoots, const FText& ProgressBarMessage) const
	{
		TArray<FAssetOperationPath> TouchedAssets;
		TouchedAssets.Reserve(AssetOperations.Update.Num() + AssetOperations.Replace.Num() + AssetOperations.Add.Num());
		TouchedAssets.Append(AssetOperations.Update);
		TouchedAssets.Append(AssetOperations.Replace);
		TouchedAssets.Append(AssetOperations.Add);

		// Even though these files were skipped - if they would go to a new location then references need to be updated.
		TArray<FAssetOperationPath> IncludedAssets(TouchedAssets);
		IncludedAssets.Append(AssetOperations.Skip);

		// If required, set up redirects
		TArray<FCoreRedirect> Redirects;
		const FString& AssetExtension = FPackageName::GetAssetPackageExtension();
		for (const FAssetOperationPath& AssetFilePath : IncludedAssets)
		{
			if (AssetFilePath.SourceFile.EndsWith(AssetExtension))
			{
				if (AssetFilePath.SourcePackage != AssetFilePath.DestinationPackage)
				{
					Redirects.Emplace(ECoreRedirectFlags::Type_Package, AssetFilePath.SourcePackage, AssetFilePath.DestinationPackage);
				}
			}
		}

		int WorkRequired = 2;
		if (Redirects.Num())
		{
			AddMessage(ELogVerbosity::Verbose, LOCTEXT("ReferenceFixupMessage", "The MetaHuman import project path differs from the imported assets' original location and so references were updated to the new asset paths."));
			FCoreRedirects::AddRedirectList(Redirects, TEXT("MetaHumanImportTool"));
			WorkRequired += 1;
		}

		FScopedSlowTask ImportProgress(WorkRequired, ProgressBarMessage, true);
		ImportProgress.MakeDialog();

		// Update assets
		ImportProgress.EnterProgressFrame();

		TArray<UPackage*> PackagesToReload;
		TArray<UPackage*> BPsToReload;

		{
			int32 CommonFilesCount = AssetOperations.Add.Num() + AssetOperations.Replace.Num() + AssetOperations.Update.Num();
			FScopedSlowTask AssetLoadProgress(CommonFilesCount, FText::FromString(TEXT("Updating assets.")), true);
			AssetLoadProgress.MakeDialog();

			IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

			for (const FAssetOperationPath& AssetToAdd : AssetOperations.Add)
			{
				AssetLoadProgress.EnterProgressFrame();
				CopySingleFile(AssetToAdd.SourceFile, AssetToAdd.DestinationFile);
				PackagesToReload.Add(FindPackage(nullptr, *AssetToAdd.DestinationPackage));
			}

			TArray<FAssetOperationPath> UpdateOperations = AssetOperations.Update;
			UpdateOperations.Append(AssetOperations.Replace);
			for (const FAssetOperationPath& AssetToUpdate : UpdateOperations)
			{
				AssetLoadProgress.EnterProgressFrame();
				if (AssetToUpdate.SourceFile.EndsWith(FPackageName::GetAssetPackageExtension()))
				{
					TArray<FAssetData> AssetData;
					AssetRegistry.GetAssetsByPackageName(FName(AssetToUpdate.DestinationPackage), AssetData);
					// If the asset is not loaded we can just overwrite the file and do not need to worry about unloading
					// and reloading the package. Just look at the first asset in the package.
					if (AssetData.Num() && AssetData[0].IsAssetLoaded())
					{
						// Finish any pending read operations
						UObject* ItemObject = AssetData[0].GetAsset();
						if (!ItemObject->GetPackage()->IsFullyLoaded())
						{
							FlushAsyncLoading();
							ItemObject->GetPackage()->FullyLoad();
						}

						// We are about to replace this object, so ignore any pending changes
						ItemObject->GetPackage()->ClearDirtyFlag();
						ResetLoaders(ItemObject->GetPackage());

						// Add to lists of things to reload
						if (Cast<UBlueprint>(ItemObject) != nullptr)
						{
							BPsToReload.Add(ItemObject->GetPackage());
						}
					}
				}
				CopySingleFile(AssetToUpdate.SourceFile, AssetToUpdate.DestinationFile);
				PackagesToReload.Add(FindPackage(nullptr, *AssetToUpdate.DestinationPackage));
			}
		}

		// Reload packages and BPs
		FScopedSlowTask PackageReloadProgress(PackagesToReload.Num() + BPsToReload.Num(), LOCTEXT("ReloadingPackagesProgress", "Reloading packages."), true);
		PackageReloadProgress.MakeDialog();

		PackageReloadProgress.EnterProgressFrame(PackagesToReload.Num());
		UPackageTools::ReloadPackages(PackagesToReload);

		for (const UPackage* Package : BPsToReload)
		{
			PackageReloadProgress.EnterProgressFrame();
			UObject* Obj = Package->FindAssetInPackage();
			if (UBlueprint* BPObject = Cast<UBlueprint>(Obj))
			{
				FKismetEditorUtilities::CompileBlueprint(BPObject, EBlueprintCompileOptions::SkipGarbageCollection);
				BPObject->PreEditChange(nullptr);
				BPObject->PostEditChange();
			}
		}

		// Refresh asset registry
		ImportProgress.EnterProgressFrame();
		IAssetRegistry::Get()->ScanPathsSynchronous(DestinationAssetRoots, true);

		// Re-save assets to bake-in new reference paths
		if (Redirects.Num())
		{
			ImportProgress.EnterProgressFrame();
			FScopedSlowTask MetaDataWriteProgress(TouchedAssets.Num(), LOCTEXT("ImportFinalizingProgress", "Finalizing imported assets"));
			MetaDataWriteProgress.MakeDialog();
			for (const FAssetOperationPath& AssetToUpdate : TouchedAssets)
			{
				MetaDataWriteProgress.EnterProgressFrame();
				if (!IFileManager::Get().FileExists(*AssetToUpdate.DestinationFile))
				{
					continue;
				}

				if (UPackage* Package = UPackageTools::LoadPackage(AssetToUpdate.DestinationFile))
				{
					FSavePackageArgs SaveArgs;
					SaveArgs.TopLevelFlags = RF_Standalone;
					UPackage::Save(Package, nullptr, *AssetToUpdate.DestinationFile, SaveArgs);
				}
			}

			// Remove Redirects
			FCoreRedirects::RemoveRedirectList(Redirects, TEXT("MetaHumanImportTool"));
		}
	}

	TMap<FString, FMetaHumanAssetVersion> GetSourceFiles()
	{
		TMap<FString, FMetaHumanAssetVersion> VersionInfo;
		// Try getting in the new format FileList.json
		if (TSharedPtr<FJsonObject> SourceData = FileSource.ReadJson(TEXT("FileList.json")))
		{
			FMetaHumanArchiveContents FilesList;
			FJsonObjectConverter::JsonObjectToUStruct(SourceData.ToSharedRef(), &FilesList);

			for (const FMetaHumanArchiveEntry& Entry : FilesList.Files)
			{
				VersionInfo.Add(Entry.Path, FMetaHumanAssetVersion(Entry.Version));
			}
		}
		// Fall back to the old MHAssetVersions.txt
		else if ((SourceData = FileSource.ReadJson(TEXT("MHAssetVersions.txt"))))
		{
			TArray<TSharedPtr<FJsonValue>> AssetsVersionInfoArray = SourceData->GetArrayField(TEXT("assets"));

			for (const TSharedPtr<FJsonValue>& AssetVersionInfoObject : AssetsVersionInfoArray)
			{
				FString AssetPath = AssetVersionInfoObject->AsObject()->GetStringField(TEXT("path"));
				// Remove leading "MetaHumans/" as this can be configured to an arbitrary value by the users
				if (const FString DefaultRoot = FImportPaths::MetaHumansFolderName + TEXT("/"); AssetPath.StartsWith(DefaultRoot))
				{
					AssetPath = AssetPath.RightChop(DefaultRoot.Len());
				}
				FMetaHumanAssetVersion AssetVersion(AssetVersionInfoObject->AsObject()->GetStringField(TEXT("version")));
				VersionInfo.Add(AssetPath, AssetVersion);
			}
		}

		if (VersionInfo.IsEmpty())
		{
			AddMessage(ELogVerbosity::Error, LOCTEXT("NoArchiveVersionInfo", "The archive does not have a valid contents listing and can not be imported"));
		}

		return VersionInfo;
	}

private:
	UMetaHumanAssetReport* Report;
	FFileSource FileSource;
};

class FAssetGroupImportContext
{
public:
	FAssetGroupImportContext(const FAssetGroupImportDescription& InImportDescription) :
		Context(InImportDescription.Report, InImportDescription.FileSource),
		ImportDescription(InImportDescription)
	{
	}

	bool Import()
	{
		// Read file list from Source.
		TMap<FString, FMetaHumanAssetVersion> SourceFiles = Context.GetSourceFiles();
		if (SourceFiles.IsEmpty())
		{
			return false;
		}

		// Get AssetOperations for the update of the downloaded Files
		FScopedSlowTask AssetScanProgress(SourceFiles.Num(), FText::FromString(TEXT("Scanning existing assets")), true);
		AssetScanProgress.MakeDialog();
		FAssetOperations AssetOperations;
		for (const TTuple<FString, FMetaHumanAssetVersion>& SourceAssetInfo : SourceFiles)
		{
			AssetScanProgress.EnterProgressFrame();
			FAssetOperationPath Paths = {
				SourceAssetInfo.Key,
				FPackageName::LongPackageNameToFilename(ImportDescription.TargetRootAssetPath) / SourceAssetInfo.Key,
				ImportDescription.SourceRootAssetPath / FPaths::GetBaseFilename(SourceAssetInfo.Key, false),
				ImportDescription.TargetRootAssetPath / FPaths::GetBaseFilename(SourceAssetInfo.Key, false)
			};
			// If there is no existing asset, we add it
			if (IFileManager::Get().FileExists(*Paths.DestinationFile))
			{
				AssetOperations.Replace.Add(Paths);
			}
			else
			{
				AssetOperations.Add.Add(Paths);
			}
		}

		TArray<FString> AssetDestinationPaths;
		AssetDestinationPaths.Add(ImportDescription.TargetRootAssetPath);
		Context.CopyFiles(AssetOperations, AssetDestinationPaths, LOCTEXT("AssetGroupImportProgressMessage", "Importing MetaHuman Assets ..."));

		return true;
	}

private:
	FImportContext Context;
	const FAssetGroupImportDescription& ImportDescription;
};

class FMetaHumanImportContext
{
	bool CopySingleFile(const FString& SourceFilePath, const FString& DestinationFilePath, bool bIsOptional = false) const
	{
		if (OnShouldImportAssetOrFileDelegate.IsBound() && !OnShouldImportAssetOrFileDelegate.Execute(SourceMetaHuman, SourceFilePath, true))
		{
			// As far as the calling code is concerned, deliberately skipping a file is a success.
			return true;
		}

		return Context.CopySingleFile(SourceFilePath, DestinationFilePath, bIsOptional);
	}

	// Calculate which assets to add to the project, which to replace, which to update and which to skip
	FAssetOperations DetermineAssetOperations(const TMap<FString, FMetaHumanAssetVersion>& SourceVersionInfo, const FImportPaths& ImportPaths) const
	{
		FScopedSlowTask AssetScanProgress(SourceVersionInfo.Num(), FText::FromString(TEXT("Scanning existing assets")), true);
		AssetScanProgress.MakeDialog();
		static const FName MetaHumanAssetVersionKey = TEXT("MHAssetVersion");
		FAssetOperations AssetOperations;

		for (const TTuple<FString, FMetaHumanAssetVersion>& SourceAssetInfo : SourceVersionInfo)
		{
			AssetScanProgress.EnterProgressFrame();
			FAssetOperationPath Paths = {
				SourceAssetInfo.Key,
				ImportPaths.GetDestinationFile(SourceAssetInfo.Key),
				ImportPaths.GetSourcePackage(SourceAssetInfo.Key),
				ImportPaths.GetDestinationPackage(SourceAssetInfo.Key)
			};
			// If there is no existing asset, we add it
			if (!IFileManager::Get().FileExists(*Paths.DestinationFile))
			{
				AssetOperations.Add.Add(Paths);
				continue;
			}

			// If we are doing a force update or the asset is unique to the MetaHuman we always replace it
			if (ImportDescription.bForceUpdate || !SourceAssetInfo.Key.StartsWith(FImportPaths::CommonFolderName + TEXT("/")))
			{
				AssetOperations.Replace.Add(Paths);
				continue;
			}

			// If the asset is part of the common assets, we only update it if the source asset has a greater version number
			// If the file has no metadata then we assume it is old and will update it.
			FString TargetVersion = TEXT("0.0");
			if (const UObject* Asset = LoadObject<UObject>(nullptr, *(FPaths::GetPath(Paths.DestinationPackage) / FImportPaths::FilenameToAssetName(SourceAssetInfo.Key))))
			{
				if (const TMap<FName, FString>* Metadata = FMetaData::GetMapForObject(Asset))
				{
					if (const FString* VersionMetaData = Metadata->Find(MetaHumanAssetVersionKey))
					{
						TargetVersion = *VersionMetaData;
					}
				}
			}

			const FMetaHumanAssetVersion OldVersion(TargetVersion);
			const FMetaHumanAssetVersion NewVersion = SourceAssetInfo.Value;
			if (NewVersion > OldVersion)
			{
				AssetOperations.Update.Add(Paths);
				AssetOperations.UpdateReasons.Add({OldVersion, NewVersion});
			}
			else
			{
				AssetOperations.Skip.Add(Paths);
			}
		}

		return AssetOperations;
	}


	// Check if the project contains any incompatible MetaHuman characters
	TSet<FString> CheckVersionCompatibility(const TArray<FInstalledMetaHuman>& InstalledMetaHumans)
	{
		TSet<FString> IncompatibleCharacters;
		const FMetaHumanVersion& SourceVersion = SourceMetaHuman.GetVersion();
		for (const FInstalledMetaHuman& InstalledMetaHuman : InstalledMetaHumans)
		{
			if (!SourceVersion.IsCompatible(InstalledMetaHuman.GetVersion()))
			{
				IncompatibleCharacters.Emplace(InstalledMetaHuman.GetName());
			}
		}
		return IncompatibleCharacters;
	}

	bool MHInLevel(const FString& CharacterBPPath)
	{
		const FString CharacterPathInLevel = CharacterBPPath + TEXT("_C");
		TArray<AActor*> FoundActors;
		check(GEngine->GetWorldContexts().Num() != 0);
		UGameplayStatics::GetAllActorsOfClass(GEngine->GetWorldContexts()[0].World(), AActor::StaticClass(), FoundActors);

		for (const AActor* FoundActor : FoundActors)
		{
			FString ActorPath = FoundActor->GetClass()->GetPathName();
			if (ActorPath.Equals(CharacterPathInLevel))
			{
				return true;
			}
		}

		return false;
	}

public:
	FMetaHumanImportContext(const FMetaHumanImportDescription& ImportDescription, const FSourceMetaHuman& SourceMetaHuman) :
		Context(ImportDescription.Report, FPaths::GetPath(ImportDescription.CharacterPath)),
		ImportDescription(ImportDescription),
		SourceMetaHuman(SourceMetaHuman)
	{
		if (ImportDescription.Archive.IsValid())
		{
			Context = FImportContext(ImportDescription.Report, ImportDescription.Archive);
		}
	}

	TOptional<UObject*> Import()
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MetaHumanName"), FText::FromString(ImportDescription.CharacterName));

		// Determine the source and destination paths. There are two ways they can be updated from the standard /Game/MetaHumans
		// location. In UEFN we can request that instead of installing to /Game we install to the content folder of the
		// project. Also, we can use project settings to override the destination paths for both cinematic and optimized
		// MetaHumans
		FString DestinationCommonAssetPath = ImportDescription.DestinationPath / FImportPaths::CommonFolderName; // At the moment this can not be changed
		FString CharactersRootImportPath = ImportDescription.DestinationPath; // This is the location we will look for other characters in the project

		// If the ImportDescription does not target a specific location (i.e. not UEFN) then look for a project-based override
		if (ImportDescription.DestinationPath == FMetaHumanImportDescription::DefaultDestinationPath)
		{
			// Get overrides from settings
			const UMetaHumanSDKSettings* ProjectSettings = GetDefault<UMetaHumanSDKSettings>();
			if (SourceMetaHuman.GetQualityLevel() == EMetaHumanQualityLevel::Cinematic)
			{
				if (!ProjectSettings->CinematicImportPath.Path.IsEmpty())
				{
					// Use the project-configured destination path for cinematic MHs
					CharactersRootImportPath = ProjectSettings->CinematicImportPath.Path;
				}
			}
			else if (!ProjectSettings->OptimizedImportPath.Path.IsEmpty())
			{
				// Use the project-configured destination path for optimized MHs
				CharactersRootImportPath = ProjectSettings->OptimizedImportPath.Path;
			}
		}

		// If OnImportStartedDelegate is bound and returns false it means the import operation was canceled
		if (OnImportStartedDelegate.IsBound() && !OnImportStartedDelegate.Execute(SourceMetaHuman))
		{
			Context.AddMessage(ELogVerbosity::Error, LOCTEXT("OperationCancelledOverride", "The import operation was cancelled"));
			return {};
		}

		Args.Add(TEXT("DestinationCommonAssetPath"), FText::FromString(DestinationCommonAssetPath));
		Args.Add(TEXT("CharactersRootImportPath"), FText::FromString(CharactersRootImportPath));
		// Check we are trying to import to a valid content root
		if (!(FPackageName::IsValidPath(DestinationCommonAssetPath) && FPackageName::IsValidPath(CharactersRootImportPath)))
		{
			FText ErrorMessage = FText::Format(LOCTEXT("InvalidImportRootError", "Attempting to import to an invalid root location. Please check your Import Paths in the MetaHuman SDK Project Settings.\n Common files import root: \"{DestinationCommonAssetPath}\", character files import root: \"{CharactersRootImportPath}\""), Args);
			Context.AddMessageWithMessageBox(ELogVerbosity::Error, ErrorMessage);
			return {};
		}

		// This is the location we are installing the character to
		FString DestinationCharacterAssetPath{CharactersRootImportPath / ImportDescription.CharacterName};
		Args.Add(TEXT("ImportCharacterPath"), FText::FromString(ImportDescription.CharacterPath));
		if (ImportDescription.Archive)
		{
			Args.Add(TEXT("ImportCharacterPath"), LOCTEXT("ArchiveFile", "Archive File"));
		}
		Context.AddMessage(ELogVerbosity::Verbose, FText::Format(LOCTEXT("ImportOperationSummary", "Importing {MetaHumanName} from {ImportCharacterPath} to \"{DestinationCommonAssetPath}\", and \"{CharactersRootImportPath}\""), Args));

		// Helpers for managing source data
		const FImportPaths ImportPaths(ImportDescription.CharacterPath, ImportDescription.SourcePath, DestinationCommonAssetPath, DestinationCharacterAssetPath);

		// sanitize our import destination
		const int MaxImportPathLength = FPlatformMisc::GetMaxPathLength() - 100; // longest asset path in a MetaHuman ~100 chars
		Args.Add(TEXT("DestinationCharacterFilePath"), FText::FromString(ImportPaths.DestinationCharacterFilePath));
		Args.Add(TEXT("MaxImportPathLength"), MaxImportPathLength);
		if (ImportPaths.DestinationCharacterFilePath.Len() > MaxImportPathLength)
		{
			FText ErrorMessage = FText::Format(LOCTEXT("ImportPathLengthError", "The requested import path {DestinationCharacterFilePath} is longer than {MaxImportPathLength} characters. Please set the Import Path in the MetaHuman SDK Project Settings to a shorter path, or move your project to a file location with a shorter path."), Args);
			Context.AddMessageWithMessageBox(ELogVerbosity::Error, ErrorMessage);
			return {};
		}

		// Determine what other MetaHumans are installed and if any are incompatible
		const TArray<FInstalledMetaHuman> InstalledMetaHumans = FInstalledMetaHuman::GetInstalledMetaHumans(ImportPaths.DestinationCharacterRootFilePath, ImportPaths.DestinationCommonFilePath);
		const TSet<FString> IncompatibleCharacters = CheckVersionCompatibility(InstalledMetaHumans);

		// Get the names of all installed MetaHumans and see if the MetaHuman we are trying to install is among them
		TSet<FString> InstalledMetaHumanNames;
		Algo::Transform(InstalledMetaHumans, InstalledMetaHumanNames, &FInstalledMetaHuman::GetName);
		bool bIsNewCharacter = !InstalledMetaHumanNames.Contains(ImportDescription.CharacterName);


		// Get AssetOperations for the update of the downloaded MetaHuman
		const FAssetOperations AssetOperations = DetermineAssetOperations(Context.GetSourceFiles(), ImportPaths);

		Args.Add(TEXT("NumAddOperations"), AssetOperations.Add.Num());
		Args.Add(TEXT("NumUpdateOperations"), AssetOperations.Replace.Num() + AssetOperations.Update.Num());
		Args.Add(TEXT("NumSkipOperations"), AssetOperations.Skip.Num());
		Context.AddMessage(ELogVerbosity::Verbose, FText::Format(LOCTEXT("AssetOperationsSummary", "Importing {MetaHumanName} with {NumAddOperations} new files added, {NumUpdateOperations} existing files updated, and {NumSkipOperations} files skipped based on the version of the assets present in the project."), Args));

		// If we are updating common files, have incompatible characters and are not updating all of them, then ask the user if they want to continue.
		if (IncompatibleCharacters.Num() > 0 && !ImportDescription.bIsBatchImport && !AssetOperations.Update.IsEmpty())
		{
			if (AutomationHandler)
			{
				TArray<FString> ToUpdate;
				for (const FAssetOperationPath& OperationPath : AssetOperations.Update)
				{
					ToUpdate.Add(OperationPath.DestinationFile);
				}
				if (!AutomationHandler->ShouldContinueWithBreakingMetaHumans(IncompatibleCharacters.Array(), ToUpdate))
				{
					return {};
				}
			}
			else
			{
				TSet<FString> AvailableMetaHumans;
				for (const FQuixelAccountMetaHumanEntry& Entry : ImportDescription.AccountMetaHumans)
				{
					if (!Entry.bIsLegacy)
					{
						AvailableMetaHumans.Add(Entry.Name);
					}
				}
				EImportOperationUserResponse Response = DisplayUpgradeWarning(SourceMetaHuman, IncompatibleCharacters, InstalledMetaHumans, AvailableMetaHumans, AssetOperations);

				AnalyticsEvent(TEXT("ImportConflictResolved"), {{TEXT("Result"), Response == EImportOperationUserResponse::Cancel ? TEXT("Cancel") : (Response == EImportOperationUserResponse::BulkImport ? TEXT("BulkImport") : TEXT("OK"))}});

				if (Response == EImportOperationUserResponse::Cancel)
				{
					Context.AddMessage(ELogVerbosity::Error, LOCTEXT("OperationCancelled", "The import operation was cancelled by the user"));
					return {};
				}

				if (Response == EImportOperationUserResponse::BulkImport && BulkImportHandler)
				{
					TArray<FString> ImportIds{ImportDescription.QuixelId};
					for (const FString& Name : IncompatibleCharacters)
					{
						for (const FQuixelAccountMetaHumanEntry& Entry : ImportDescription.AccountMetaHumans)
						{
							// TODO - this just selects the first entry that matches the MetaHuman's name. We need to handle more complex mapping between Ids and entry in the UI
							if (!Entry.bIsLegacy && Entry.Name == Name)
							{
								ImportIds.Add(Entry.Id);
								break;
							}
						}
					}
					BulkImportHandler->DoBulkImport(ImportIds);
					Context.AddMessage(ELogVerbosity::Warning, LOCTEXT("OperationReplacedWithBulkImport", "The import operation was replaced with a bulk import to update all MetaHumans in the scene"));
					return {};
				}
			}
		}

		const FInstalledMetaHuman TargetMetaHuman(ImportDescription.CharacterName, ImportPaths.DestinationCharacterFilePath, ImportPaths.DestinationCommonFilePath);

		// If the user is changing the export quality level of the MetaHuman then warn them that they are doing do
		if (!bIsNewCharacter && ImportDescription.bWarnOnQualityChange)
		{
			const EMetaHumanQualityLevel SourceQualityLevel = SourceMetaHuman.GetQualityLevel();
			const EMetaHumanQualityLevel TargetQualityLevel = TargetMetaHuman.GetQualityLevel();
			if (SourceQualityLevel != TargetQualityLevel)
			{
				const bool bContinue = DisplayQualityLevelChangeWarning(SourceQualityLevel, TargetQualityLevel);
				if (!bContinue)
				{
					Context.AddMessage(ELogVerbosity::Error, LOCTEXT("OperationCancelled", "The import operation was cancelled by the user"));
					return {};
				}
			}
		}

		// Update assets
		FText ProgressBarMessage = FText::FromString((bIsNewCharacter ? TEXT("Importing : ") : TEXT("Re-Importing : ")) + ImportDescription.CharacterName);
		TArray<FString> AssetDestinationPaths;
		AssetDestinationPaths.Add(ImportPaths.DestinationCommonAssetPath);
		AssetDestinationPaths.Add(ImportPaths.DestinationCharacterAssetPath);
		Context.CopyFiles(AssetOperations, AssetDestinationPaths, ProgressBarMessage);

		// Copy in text version files
		const FString VersionFile = TEXT("VersionInfo.txt");
		constexpr bool bIsOptional = true;
		CopySingleFile(ImportDescription.CharacterName / VersionFile, ImportPaths.DestinationCharacterFilePath / VersionFile, bIsOptional);
		CopySingleFile(TEXT("Common") / VersionFile, ImportPaths.DestinationCommonFilePath / VersionFile, bIsOptional);

		// Copy in the DNA source file if present
		const FString DnaFile = ImportDescription.CharacterName + TEXT(".dna");
		const FString SourceAssetsFolder = TEXT("SourceAssets");
		CopySingleFile(ImportDescription.CharacterName / SourceAssetsFolder / DnaFile, ImportPaths.DestinationCharacterFilePath / SourceAssetsFolder / DnaFile, bIsOptional);

		if (SourceMetaHuman.IsUEFN())
		{
			// Remove all graphs from the actor blueprint to ensure the MetaHuman blueprint can be validated
			UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();

			if (UBlueprint* Blueprint = Cast<UBlueprint>(EditorAssetSubsystem->LoadAsset(TargetMetaHuman.GetRootAsset())))
			{
				TArray<UEdGraph*> Graphs;
				Blueprint->GetAllGraphs(Graphs);

				FBlueprintEditorUtils::RemoveGraphs(Blueprint, Graphs);

				if (!Blueprint->bBeingCompiled)
				{
					FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
				}

				EditorAssetSubsystem->SaveLoadedAsset(Blueprint);
			}
		}

		if (OnImportEndedDelegate.IsBound() && !OnImportEndedDelegate.Execute(SourceMetaHuman, TargetMetaHuman))
		{
			Context.AddMessage(ELogVerbosity::Error, FText::Format(LOCTEXT("ImportEndedError", "{MetaHumanName} has not been imported successfully."), Args));
			return {};
		}

		Context.AddMessage(ELogVerbosity::Display, FText::Format(LOCTEXT("ImportEndedSuccess", "{MetaHumanName} has been imported successfully."), Args));

		return LoadObject<UObject>(nullptr, *TargetMetaHuman.GetRootAsset());
	};

	IMetaHumanImportAutomationHandler* AutomationHandler{nullptr};
	IMetaHumanBulkImportHandler* BulkImportHandler{nullptr};
	FMetaHumanImport::FOnImportStarted OnImportStartedDelegate;
	FMetaHumanImport::FOnShouldImportAssetOrFile OnShouldImportAssetOrFileDelegate;
	FMetaHumanImport::FOnImportEnded OnImportEndedDelegate;

private:
	FImportContext Context;
	const FMetaHumanImportDescription& ImportDescription;
	const FSourceMetaHuman& SourceMetaHuman;
};

// FMetaHumanImport Definition *****************************************
TSharedPtr<FMetaHumanImport> FMetaHumanImport::MetaHumanImportInst;

TSharedPtr<FMetaHumanImport> FMetaHumanImport::Get()
{
	if (!MetaHumanImportInst.IsValid())
	{
		MetaHumanImportInst = MakeShareable(new FMetaHumanImport);
	}
	return MetaHumanImportInst;
}

void FMetaHumanImport::SetAutomationHandler(IMetaHumanImportAutomationHandler* Handler)
{
	AutomationHandler = Handler;
}

void FMetaHumanImport::SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler)
{
	BulkImportHandler = Handler;
}

TOptional<UObject*> FMetaHumanImport::ImportMetaHuman(const FMetaHumanImportDescription& ImportDescription) const
{
	AnalyticsEvent(TEXT("AssemblyImport"), {{TEXT("bIsQuixel"), ImportDescription.QuixelId.IsEmpty() ? TEXT("false") : TEXT("true")}});

	const FSourceMetaHuman SourceMetaHuman = ImportDescription.Archive ? FSourceMetaHuman(ImportDescription.Archive.Get()) : FSourceMetaHuman(ImportDescription.CharacterPath, ImportDescription.CommonPath, ImportDescription.CharacterName);
	FMetaHumanImportContext Context(ImportDescription, SourceMetaHuman);
	Context.AutomationHandler = AutomationHandler;
	Context.BulkImportHandler = BulkImportHandler;
	Context.OnImportEndedDelegate = OnImportEndedDelegate;
	Context.OnImportStartedDelegate = OnImportStartedDelegate;
	Context.OnShouldImportAssetOrFileDelegate = OnShouldImportAssetOrFileDelegate;
	return Context.Import();
}

TOptional<UObject*> FMetaHumanImport::ImportAssetGroup(const FAssetGroupImportDescription ImportDescription) const
{
	AnalyticsEvent(TEXT("AssetGroupImport"));

	FAssetGroupImportContext Context(ImportDescription);
	if (!Context.Import())
	{
		return {};
	}
	FString RootAsset = ImportDescription.TargetRootAssetPath / FImportPaths::FilenameToAssetName(ImportDescription.Name);
	return LoadObject<UObject>(nullptr, *RootAsset);
}
}

#undef LOCTEXT_NAMESPACE

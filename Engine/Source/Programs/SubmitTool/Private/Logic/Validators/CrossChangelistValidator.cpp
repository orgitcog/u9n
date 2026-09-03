// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrossChangelistValidator.h"
#include "Misc/Paths.h"
#include "Logging/SubmitToolLog.h"
#include "Logic/ChangelistService.h"
#include "CommandLine/CmdLineParameters.h"
#include "Serialization/JsonSerializer.h"
#include "SubmitToolUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

bool FCrossChangelistValidator::Validate(const FString& InCLDescription, const TArray<FSourceControlStateRef>& InFilteredFilesInCL, const TArray<const FTag*>& InTags)
{
	const TArray<FSourceControlChangelistStatePtr> OtherChangelistsStates = ServiceProvider.Pin()->GetService<FChangelistService>()->GetOtherChangelistsStates();
	bool bValid = true;

	bValid &= CheckHeaderAndCppInDifferentChangelist(OtherChangelistsStates);

	TArray<FString> AssetPaths;
	const FString UAssetExt(TEXT(".uasset"));
	const FString UMapExt(TEXT(".umap"));
	const FString UPluginExt(TEXT(".uplugin"));
	for (FSourceControlStateRef FileInCL : InFilteredFilesInCL)
	{
		const FString& Filename = FileInCL->GetFilename();

		if (Filename.EndsWith(UAssetExt, ESearchCase::IgnoreCase)
			|| Filename.EndsWith(UMapExt, ESearchCase::IgnoreCase)
			|| Filename.EndsWith(UPluginExt, ESearchCase::IgnoreCase))
		{
			AssetPaths.Add(Filename);
		}
	}

	TSet<FString> UProjects = ExtractUProjectFiles(AssetPaths);
	TSet<FString> UEFNProjects = ExtractUEFNProjectFiles(AssetPaths);

	bValid &= CheckForFilesInUncontrolledCLFile(UProjects, UEFNProjects);

	ValidationFinished(bValid);
	return true;
}

bool FCrossChangelistValidator::CheckHeaderAndCppInDifferentChangelist(const TArray<FSourceControlChangelistStatePtr>& OtherChangelistsStates)
{
	const FString HeaderExt(TEXT(".h"));
	const FString CppExt(TEXT(".cpp"));
	const FString CExt(TEXT(".c"));

	const TArray<FSourceControlStateRef>& FilesInChangelist = ServiceProvider.Pin()->GetService<FChangelistService>()->GetFilesInCL();

	bool bValid = true;

	for (FSourceControlStateRef FileInCL : FilesInChangelist)
	{
		const FString& Filename = FileInCL->GetFilename();
		if (Filename.EndsWith(HeaderExt, ESearchCase::IgnoreCase)
			|| Filename.EndsWith(CExt, ESearchCase::IgnoreCase)
			|| Filename.EndsWith(CppExt, ESearchCase::IgnoreCase))
		{
			TArray<FString> FilenamesToCheck;

			if (Filename.EndsWith(HeaderExt, ESearchCase::IgnoreCase))
			{
				FilenamesToCheck.Add(FPaths::GetCleanFilename(Filename.Replace(*HeaderExt, *CppExt)));
				FilenamesToCheck.Add(FPaths::GetCleanFilename(Filename.Replace(*HeaderExt, *CExt)));
			}

			if (Filename.EndsWith(CExt, ESearchCase::IgnoreCase))
			{
				FilenamesToCheck.Add(FPaths::GetCleanFilename(Filename.Replace(*CExt, *HeaderExt)));
			}

			if (Filename.EndsWith(CppExt, ESearchCase::IgnoreCase))
			{
				FilenamesToCheck.Add(FPaths::GetCleanFilename(Filename.Replace(*CppExt, *HeaderExt)));
			}

			for (const FSourceControlChangelistStatePtr& ChangelistState : OtherChangelistsStates)
			{
				for (const FSourceControlStateRef& FileSate : ChangelistState->GetFilesStates())
				{
					FString OtherFilename = FPaths::GetCleanFilename(FileSate->GetFilename());
					if (FilenamesToCheck.Contains(OtherFilename))
					{
						bValid = false;

						FString Message = FString::Printf(TEXT("[%s] %s file '%s' is not in the current CL, it is in CL '%s'"),
							*GetValidatorName(),
							OtherFilename.EndsWith(HeaderExt) ? TEXT("Header") : TEXT("CPP | C"),
							*OtherFilename,
							*(ChangelistState->GetChangelist()->GetIdentifier()));

						LogFailure(Message);
					}
				}
			}
		}
	}

	return bValid;
}

bool FCrossChangelistValidator::CheckForFilesInUncontrolledCLFile(const TSet<FString>& InUProjects, const TSet<FString>& InUEFNProjects)
{
	bool bAllValid = true;
	bool bValid = true;
	for (const FString& ProjectFile : InUProjects)
	{
		const FString UncontrolledCLPath = FPaths::Combine(FPaths::GetPath(ProjectFile), TEXT("Saved"), TEXT("SourceControl"), TEXT("UncontrolledChangelists.json"));
		TMap<FString, TArray<FString>> UncontrolledCls = LoadUncontrolledCLs(UncontrolledCLPath);

		for (const TPair<FString, TArray<FString>>& Pair : UncontrolledCls)
		{
			if (Pair.Value.Num() > 0)
			{
				if (bValid)
				{
					LogFailure(FString::Printf(TEXT("[%s] Found Uncontrolled CLs with files in project %s (%s), please check files are not missing from your change."), *GetValidatorName(), *ProjectFile, *UncontrolledCLPath));
					bAllValid = false;
					bValid = false;
				}

				LogFailure(FString::Printf(TEXT("[%s] Uncontrolled changelist '%s' found with %d files: \n\t-\t%s"), *GetValidatorName(), *Pair.Key, Pair.Value.Num(), *FString::Join(Pair.Value, TEXT("\n\t-\t"))));
			}
		}
	}

	bValid = true;
	if (!InUEFNProjects.IsEmpty())
	{
		const FString GenericUncontrolledCLPath = FPaths::Combine(FSubmitToolUtils::GetLocalAppDataPath(), TEXT("UnrealEditorFortnite"), TEXT("SourceControl"), TEXT("UncontrolledChangelists.json"));
		TMap<FString, TArray<FString>> GenericUncontrolledCls = LoadUncontrolledCLs(GenericUncontrolledCLPath);

		for (const TPair<FString, TArray<FString>>& Pair : GenericUncontrolledCls)
		{
			if (Pair.Value.Num() > 0)
			{
				if (bValid)
				{
					LogFailure(FString::Printf(TEXT("[%s] Found Uncontrolled CLs with files in the global settings (%s), please check files are not missing from your change."), *GetValidatorName(), *GenericUncontrolledCLPath));
					bAllValid = false;
					bValid = false;
				}

				LogFailure(FString::Printf(TEXT("[%s] Uncontrolled changelist '%s' found with %d files: \n\t-\t%s"), *GetValidatorName(), *Pair.Key, Pair.Value.Num(), *FString::Join(Pair.Value, TEXT("\n\t-\t"))));
			}
		}
	}

	bValid = true;
	for (const FString& UEFNProject : InUEFNProjects)
	{
		const FString UncontrolledCLProjectFilename = FString::Printf(TEXT("UncontrolledChangelists_%s.json"), *FPaths::GetPathLeaf(FPaths::GetPath(UEFNProject)));
		const FString ProjectUncontrolledCLPath = FPaths::Combine(FSubmitToolUtils::GetLocalAppDataPath(), TEXT("UnrealEditorFortnite"), TEXT("SourceControl"), UncontrolledCLProjectFilename);
		TMap<FString, TArray<FString>> ProjectUncontrolledCls = LoadUncontrolledCLs(ProjectUncontrolledCLPath);

		for (const TPair<FString, TArray<FString>>& Pair : ProjectUncontrolledCls)
		{
			if (Pair.Value.Num() > 0)
			{
				if (bValid)
				{
					LogFailure(FString::Printf(TEXT("[%s] Found Uncontrolled CLs with files in project %s (%s), please check files are not missing from your change."), *GetValidatorName(), *UEFNProject, *ProjectUncontrolledCLPath));
					bAllValid = false;
					bValid = false;
				}

				LogFailure(FString::Printf(TEXT("[%s] Uncontrolled changelist '%s' found with %d files: \n\t-\t%s"), *GetValidatorName(), *Pair.Key, Pair.Value.Num(), *FString::Join(Pair.Value, TEXT("\n\t-\t"))));
			}
		}
	}

	return bValid;
}

static constexpr const TCHAR* VERSION_NAME = TEXT("version");
static constexpr const TCHAR* CHANGELISTS_NAME = TEXT("changelists");
static constexpr uint32 VERSION_NUMBER = 1;
static constexpr const TCHAR* GUID_NAME = TEXT("guid");
static constexpr const TCHAR* FILES_NAME = TEXT("files");
static constexpr const TCHAR* NAME_NAME = TEXT("name");
static constexpr const TCHAR* DESCRIPTION_NAME = TEXT("description");
TMap<FString, TArray<FString>> FCrossChangelistValidator::LoadUncontrolledCLs(const FString& InFile) const
{
	TMap<FString, TArray<FString>> FilesInUncontrolledCL;
	FString ImportJsonString;
	TSharedPtr<FJsonObject> RootObject;
	uint32 VersionNumber = 0;
	const TArray<TSharedPtr<FJsonValue>>* UncontrolledChangelistsArray = nullptr;

	if (IFileManager::Get().FileExists(*InFile) && FFileHelper::LoadFileToString(ImportJsonString, *InFile))
	{
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ImportJsonString);

		if (!FJsonSerializer::Deserialize(JsonReader, RootObject))
		{
			UE_LOG(LogValidators, Warning, TEXT("[%s] Cannot deserialize RootObject."), *GetValidatorName());
			return FilesInUncontrolledCL;
		}

		if (!RootObject->TryGetNumberField(VERSION_NAME, VersionNumber))
		{
			UE_LOG(LogValidators, Warning, TEXT("[%s] Cannot get field %s."), *GetValidatorName(), VERSION_NAME);
			return FilesInUncontrolledCL;
		}

		if (VersionNumber > VERSION_NUMBER)
		{
			UE_LOG(LogValidators, Warning, TEXT("[%s] Version number is invalid (file: %u, current: %u)."), *GetValidatorName(), VersionNumber, VERSION_NUMBER);
			return FilesInUncontrolledCL;
		}

		if (!RootObject->TryGetArrayField(CHANGELISTS_NAME, UncontrolledChangelistsArray))
		{
			UE_LOG(LogValidators, Warning, TEXT("[%s] Cannot get field %s."), *GetValidatorName(), CHANGELISTS_NAME);
			return FilesInUncontrolledCL;
		}

		for (const TSharedPtr<FJsonValue>& JsonValue : *UncontrolledChangelistsArray)
		{
			TSharedRef<FJsonObject> JsonObject = JsonValue->AsObject().ToSharedRef();
			const TArray<TSharedPtr<FJsonValue>>* FileValues = nullptr;

			FString CLDescription;
			if (!JsonObject->TryGetStringField(DESCRIPTION_NAME, CLDescription))
			{
				UE_LOG(LogValidators, Warning, TEXT("[%s] Cannot get field %s."), *GetValidatorName(), DESCRIPTION_NAME);
			}


			if ((!JsonObject->TryGetArrayField(FILES_NAME, FileValues)) || (FileValues == nullptr))
			{
				UE_LOG(LogValidators, Warning, TEXT("[%s] Cannot get field %s."), *GetValidatorName(), FILES_NAME);
				return FilesInUncontrolledCL;
			}

			TArray<FString> Filenames;
			Algo::Transform(*FileValues, Filenames, [](const TSharedPtr<FJsonValue>& File)
				{
					return File->AsString();
				});

			FilesInUncontrolledCL.FindOrAdd(CLDescription, Filenames);
		}

		UE_LOG(LogValidators, Display, TEXT("[%s] Uncontrolled Changelist persistency file loaded %s, %d uncontrolled CLs"), *GetValidatorName(), *InFile, FilesInUncontrolledCL.Num());
	}

	return FilesInUncontrolledCL;
}

TSet<FString> FCrossChangelistValidator::ExtractUProjectFiles(const TArray<FString>& InFiles)
{
	TSet<FString> CheckedDirectories;
	TSet<FString> ProjectFiles;

	for (const FString& File : InFiles)
	{
		FString CurrentDir = FPaths::GetPath(File);

		while (!CurrentDir.IsEmpty())
		{
			bool bIsAlreadyInSet = false;
			CheckedDirectories.Add(CurrentDir, &bIsAlreadyInSet);
			if (bIsAlreadyInSet)
			{
				break;
			}

			TArray<FString> Projects;
			IFileManager::Get().FindFiles(Projects, *(CurrentDir / TEXT("*.uproject")), true, false);

			for (const FString& Project : Projects)
			{
				ProjectFiles.Add(CurrentDir + "/" + Project);
			}

			CurrentDir = FPaths::GetPath(CurrentDir);
		}
	}

	return ProjectFiles;
}


TSet<FString> FCrossChangelistValidator::ExtractUEFNProjectFiles(const TArray<FString>& InFiles)
{
	TSet<FString> CheckedDirectories;
	TSet<FString> ProjectFiles;

	for (const FString& File : InFiles)
	{
		FString CurrentDir = FPaths::GetPath(File);

		while (!CurrentDir.IsEmpty())
		{
			bool bIsAlreadyInSet = false;
			CheckedDirectories.Add(CurrentDir, &bIsAlreadyInSet);
			if (bIsAlreadyInSet)
			{
				break;
			}

			TArray<FString> Projects;
			IFileManager::Get().FindFiles(Projects, *(CurrentDir / TEXT("*.uefnproject")), true, false);

			for (const FString& Project : Projects)
			{
				ProjectFiles.Add(CurrentDir + "/" + Project);
			}

			CurrentDir = FPaths::GetPath(CurrentDir);
		}
	}

	return ProjectFiles;
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "CheckForVirtualizedContentCommandlet.h"

#include "CommandletUtils.h"
#include "HAL/FileManager.h"
#include "Logging/StructuredLog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/PackageTrailer.h"

UCheckForVirtualizedContentCommandlet::UCheckForVirtualizedContentCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UCheckForVirtualizedContentCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UCheckForVirtualizedContentCommandlet);

	TArray<FString> Tokens;
	TArray<FString> Switches;

	ParseCommandLine(*Params, Tokens, Switches);

	bool bNoVAContentInEngine = false;
	bool bNoVAContentInProject = false;

	TArray<FString> DirectoriesToCheck;

	int32 UnknownSwitch = 0;
	for (const FString& Switch : Switches)
	{
		FString InputPath;

		if (Switch == TEXT("CheckEngine"))
		{
			bNoVAContentInEngine = true;
		}
		else if (Switch == TEXT("CheckProject"))
		{
			bNoVAContentInProject = true;
		}
		else if (FParse::Value(*Switch, TEXT("CheckDir="), InputPath))
		{
			InputPath.ParseIntoArray(DirectoriesToCheck, TEXT("+"), true);
		}
		else
		{
			UnknownSwitch++;
		}
	}

	if (UnknownSwitch == Switches.Num())
	{
		UE_LOG(LogVirtualization, Error, TEXT("No input was provided for the commandlet. Use '-CheckEngine', '-CheckProject' or '-CheckDir=...'"));
		return 2;
	}

	TArray<FString> EnginePackages;
	TArray<FString> ProjectPackages;

	if (bNoVAContentInEngine || bNoVAContentInProject)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SortAllPackages);

		TArray<FString> AllPackages = UE::Virtualization::FindPackages(UE::Virtualization::EFindPackageFlags::None);

		const FString EngineDir = FPaths::EngineDir();

		for (const FString& Path : AllPackages)
		{
			const bool bIsEngineContent = Path.StartsWith(EngineDir);
			if (bNoVAContentInEngine && bIsEngineContent)
			{
				EnginePackages.Add(Path);
			}
			else if (bNoVAContentInProject && !bIsEngineContent)
			{
				ProjectPackages.Add(Path);
			}
		}
	}

	bool bAllContentValid = true;

	if (bNoVAContentInEngine)
	{
		if (!TryValidateContent(TEXT("Engine"), EnginePackages))
		{
			bAllContentValid = false;
		}
	}

	if (bNoVAContentInProject)
	{
		if (!TryValidateContent(TEXT("Project"), ProjectPackages))
		{
			bAllContentValid = false;
		}
	}

	if (!DirectoriesToCheck.IsEmpty())
	{
		for (const FString& Directory : DirectoriesToCheck)
		{
			if (!TryValidateDirectory(Directory))
			{
				bAllContentValid = false;
			}
		}
	}

	UE_LOG(LogVirtualization, Display, TEXT("********************************************************************************"));

	return bAllContentValid ? 0 : 1;
}

TArray<FString> UCheckForVirtualizedContentCommandlet::FindVirtualizedPackages(const TArray<FString>& PackagePaths)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ParsePackageTrailers);

	TArray<FString> VirtualizedPackages;

	for (const FString& Path : PackagePaths)
	{
		UE::FPackageTrailer Trailer;
		if (UE::FPackageTrailer::TryLoadFromFile(Path, Trailer))
		{
			const int32 NumVirtualizedPayloads = Trailer.GetNumPayloads(UE::EPayloadStorageType::Virtualized);
			if (NumVirtualizedPayloads > 0)
			{
				FString PackageName;
				if (FPackageName::TryConvertFilenameToLongPackageName(Path, PackageName))
				{
					VirtualizedPackages.Emplace(MoveTemp(PackageName));
				}
				else
				{
					VirtualizedPackages.Add(Path);
				}

			}
		}
	}

	return VirtualizedPackages;
}

bool UCheckForVirtualizedContentCommandlet::TryValidateContent(const TCHAR* DebugName, const TArray<FString>& Packages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*WriteToString<128>("TryValidateContent - ", DebugName));

	check(DebugName != nullptr);

	UE_LOG(LogVirtualization, Display, TEXT("********************************************************************************"));
	UE_LOG(LogVirtualization, Display, TEXT("Looking for virtualized payloads in %s content..."), DebugName);
	UE_LOG(LogVirtualization, Display, TEXT("Found %d %s package(s)"), Packages.Num(), DebugName);

	TArray<FString> VirtualizedPackages = FindVirtualizedPackages(Packages);

	if (VirtualizedPackages.IsEmpty())
	{
		UE_LOG(LogVirtualization, Display, TEXT("No virtualized packages were found in %s content"), DebugName);
		return true;
	}
	else
	{
		for (FString& Path : VirtualizedPackages)
		{
			UE_LOGFMT(LogVirtualization, Error, "Package {PackagePath} contains virtualized payloads", Path);
		}

		UE_LOG(LogVirtualization, Error, TEXT("Found %d virtualized package(s) in %s content"), VirtualizedPackages.Num(), DebugName);
		return false;
	}
}

bool UCheckForVirtualizedContentCommandlet::TryValidateDirectory(const FString& Directory)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryValidateDirectory);

	UE_LOG(LogVirtualization, Display, TEXT("********************************************************************************"));
	UE_LOG(LogVirtualization, Display, TEXT("Searching directory '%s' for virtualized packages..."), *Directory);

	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		UE_LOG(LogVirtualization, Error, TEXT("Directory '%s' could not be found!"), *Directory);
		return false;
	}

	TArray<FString> DirectoryPackages;
	DirectoryPackages = UE::Virtualization::FindPackagesInDirectory(Directory);

	if (DirectoryPackages.IsEmpty())
	{
		UE_LOG(LogVirtualization, Display, TEXT("Found no packages under '%s'"), *Directory);
		return true;
	}

	UE_LOG(LogVirtualization, Display, TEXT("Found %d package(s) under '%s'"), DirectoryPackages.Num(), *Directory);
	UE_LOG(LogVirtualization, Display, TEXT("Looking for virtualized payloads under directory..."));

	TArray<FString> VirtualizedPackages = FindVirtualizedPackages(DirectoryPackages);

	if (VirtualizedPackages.IsEmpty())
	{
		UE_LOG(LogVirtualization, Display, TEXT("No virtualized packages were found under '%s'"), *Directory);
	}
	else
	{
		for (FString& Path : VirtualizedPackages)
		{
			UE_LOGFMT(LogVirtualization, Error, "Package {PackagePath} contains virtualized payloads", Path);
		}

		UE_LOG(LogVirtualization, Error, TEXT("Found %d virtualized package(s) under '%s'"), VirtualizedPackages.Num(), *Directory);
	}

	return true;
}

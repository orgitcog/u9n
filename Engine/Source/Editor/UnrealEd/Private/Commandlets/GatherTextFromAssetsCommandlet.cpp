// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextFromAssetsCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/ParallelFor.h"
#include "CollectionManagerModule.h"
#include "Commandlets/GatherTextFromSourceCommandlet.h"
#include "DistanceFieldAtlas.h"
#include "Editor.h"
#include "EditorWorldUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Framework/Commands/Commands.h"
#include "HAL/FileManager.h"
#include "ICollectionContainer.h"
#include "ICollectionManager.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Logging/StructuredLog.h"
#include "MeshCardBuild.h"
#include "MeshCardRepresentation.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "PackageHelperFunctions.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "ShaderCompiler.h"
#include "Sound/DialogueWave.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/GCObjectScopeGuard.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/UObjectIterator.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "WorldPartition/WorldPartitionHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromAssetsCommandlet, Log, All);
namespace GatherTextFromAssetsCommandlet
{
	static constexpr int32 LocalizationLogIdentifier = 304;
}

/** Special feedback context used to stop the commandlet to reporting failure due to a package load error */
class FLoadPackageLogOutputRedirector : public FFeedbackContext
{
public:
	struct FScopedCapture
	{
		FScopedCapture(FLoadPackageLogOutputRedirector* InLogOutputRedirector, FStringView InPackageContext)
			: LogOutputRedirector(InLogOutputRedirector)
		{
			LogOutputRedirector->BeginCapturingLogData(InPackageContext);
		}

		~FScopedCapture()
		{
			LogOutputRedirector->EndCapturingLogData();
		}

		FLoadPackageLogOutputRedirector* LogOutputRedirector;
	};

	FLoadPackageLogOutputRedirector() = default;
	virtual ~FLoadPackageLogOutputRedirector() = default;

	void BeginCapturingLogData(FStringView InPackageContext)
	{
		// Override GWarn so that we can capture any log data
		check(!OriginalWarningContext);
		OriginalWarningContext = GWarn;
		GWarn = this;

		PackageContext = InPackageContext;
	}

	void EndCapturingLogData()
	{
		// Restore the original GWarn now that we've finished capturing log data
		check(OriginalWarningContext);
		GWarn = OriginalWarningContext;
		OriginalWarningContext = nullptr;

		// Report any messages, and also report a warning if we silenced some warnings or errors when loading
		if (ErrorCount > 0 || WarningCount > 0)
		{
			static const FString LogIndentation = TEXT("    ");

			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Package '%s' produced %d error(s) and %d warning(s) while loading (see below). Please verify that your text has gathered correctly."), *PackageContext, ErrorCount, WarningCount);
			for (const FString& FormattedOutput : FormattedErrorsAndWarningsList)
			{
				GWarn->Log(NAME_None, ELogVerbosity::Display, LogIndentation + FormattedOutput);
			}
		}

		PackageContext.Reset();

		// Reset the counts and previous log output
		ErrorCount = 0;
		WarningCount = 0;
		FormattedErrorsAndWarningsList.Reset();
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		Serialize(V, Verbosity, Category, -1.0);
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time) override
	{
		if (Verbosity == ELogVerbosity::Error)
		{
			++ErrorCount;
			// Downgrade Error to Log while loading packages to avoid false positives from things searching for "Error:" tokens in the log file
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(ELogVerbosity::Log, Category, V));
		}
		else if (Verbosity == ELogVerbosity::Warning)
		{
			++WarningCount;
			// Downgrade Warning to Log while loading packages to avoid false positives from things searching for "Warning:" tokens in the log file
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(ELogVerbosity::Log, Category, V));
		}
		else if (Verbosity == ELogVerbosity::Display)
		{
			// Downgrade Display to Log while loading packages
			OriginalWarningContext->Serialize(V, ELogVerbosity::Log, Category, Time);
		}
		else
		{
			// Pass anything else on to GWarn so that it can handle them appropriately
			OriginalWarningContext->Serialize(V, Verbosity, Category, Time);
		}
	}

	virtual void SerializeRecord(const UE::FLogRecord& Record) override
	{
		const ELogVerbosity::Type Verbosity = Record.GetVerbosity();
		if (Verbosity == ELogVerbosity::Error)
		{
			++ErrorCount;
			// Downgrade Error to Log while loading packages to avoid false positives from things searching for "Error:" tokens in the log file
			UE::FLogRecord LocalRecord = Record;
			LocalRecord.SetVerbosity(ELogVerbosity::Log);
			TStringBuilder<512> Line;
			FormatRecordLine(Line, LocalRecord);
			FormattedErrorsAndWarningsList.Emplace(Line);
		}
		else if (Verbosity == ELogVerbosity::Warning)
		{
			++WarningCount;
			// Downgrade Warning to Log while loading packages to avoid false positives from things searching for "Warning:" tokens in the log file
			UE::FLogRecord LocalRecord = Record;
			LocalRecord.SetVerbosity(ELogVerbosity::Log);
			TStringBuilder<512> Line;
			FormatRecordLine(Line, LocalRecord);
			FormattedErrorsAndWarningsList.Emplace(Line);
		}
		else if (Verbosity == ELogVerbosity::Display)
		{
			// Downgrade Display to Log while loading packages
			UE::FLogRecord LocalRecord = Record;
			LocalRecord.SetVerbosity(ELogVerbosity::Log);
			OriginalWarningContext->SerializeRecord(LocalRecord);
		}
		else
		{
			// Pass anything else on to GWarn so that it can handle them appropriately
			OriginalWarningContext->SerializeRecord(Record);
		}
	}

private:
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	TArray<FString> FormattedErrorsAndWarningsList;

	FString PackageContext;
	FFeedbackContext* OriginalWarningContext = nullptr;
};

class FAssetGatherCacheMetrics
{
public:
	FAssetGatherCacheMetrics()
		: CachedAssetCount(0)
		, UncachedAssetCount(0)
	{
		FMemory::Memzero(UncachedAssetBreakdown);
	}

	void CountCachedAsset()
	{
		++CachedAssetCount;
	}

	void CountUncachedAsset(const UGatherTextFromAssetsCommandlet::EPackageLocCacheState InState)
	{
		check(InState != UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Cached);
		++UncachedAssetCount;
		++UncachedAssetBreakdown[(int32)InState];
	}

	void LogMetrics() const
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("%s"), *ToString());
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT("Asset gather cache metrics: %d cached, %d uncached (%d too old, %d no cache or contained bytecode)"), 
			CachedAssetCount, 
			UncachedAssetCount, 
			UncachedAssetBreakdown[(int32)UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Uncached_TooOld], 
			UncachedAssetBreakdown[(int32)UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Uncached_NoCache]
			);
	}

private:
	int32 CachedAssetCount;
	int32 UncachedAssetCount;
	int32 UncachedAssetBreakdown[(int32)UGatherTextFromAssetsCommandlet::EPackageLocCacheState::Cached];
};

namespace UE::Private::GatherTextFromAssetsCommandlet
{
	static FAssetGatherCacheMetrics AssetGatherCacheMetrics;
	/**
	 * Commandlets don't tick, but loading assets can queue async building work to various systems.
	 * We tick these systems periodically during a gather to prevent us from running out of memory due to the queued pending tasks.
	 * Refer to the cooker to determine if this function needs to be expanded to cover more systems.
	 */
	void TickBackgroundTasks()
	{
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->ProcessAsyncResults(true, false);
		}
		if (GDistanceFieldAsyncQueue)
		{
			GDistanceFieldAsyncQueue->ProcessAsyncTasks();
		}
		if (GCardRepresentationAsyncQueue)
		{
			GCardRepresentationAsyncQueue->ProcessAsyncTasks();
		}
	}

	static bool bParallelizeIncludeExcludePathFiltering = true;
	static FAutoConsoleVariableRef CVarParallelizeIncludeExcludePathFiltering(TEXT("Localization.GatherTextFromAssetsCommandlet.ParallelizeIncludeExcludePathFiltering"), bParallelizeIncludeExcludePathFiltering, TEXT("True to parallelize the include exclude path filtering. False to force it to be single threaded for easier debugging."));

	static bool bParallelizeProcessAndRemoveCachedPackages = true;
	static FAutoConsoleVariableRef CVarParallelizeProcessAndRemoveCachedPackages(TEXT("Localization.GatherTextFromAssetsCommandlet.ParallelizeProcessAndRemoveCachedPackages"), bParallelizeProcessAndRemoveCachedPackages, TEXT("True to parallelize the 'process and remove cached packages' step. False to force it to be single threaded for easier debugging."));

	static int32 ProcessAndRemoveCachedPackagesMaxThreads = -1;
	static FAutoConsoleVariableRef CVarProcessAndRemoveCachedPackagesMaxThreads(TEXT("Localization.GatherTextFromAssetsCommandlet.ProcessAndRemoveCachedPackagesMaxThreads"), ProcessAndRemoveCachedPackagesMaxThreads, TEXT("Max number of threads to use if parallelizing the 'process and remove cached packages' step, or <= 0 to use as many threads as possible."));
}

#define LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
//UGatherTextFromAssetsCommandlet

const FString UGatherTextFromAssetsCommandlet::UsageText
(
	TEXT("GatherTextFromAssetsCommandlet usage...\r\n")
	TEXT("    <GameName> UGatherTextFromAssetsCommandlet -root=<parsed code root folder> -exclude=<paths to exclude>\r\n")
	TEXT("    \r\n")
	TEXT("    <paths to include> Paths to include. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, everything will be included. \r\n")
	TEXT("    <paths to exclude> Paths to exclude. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, nothing will be excluded.\r\n")
);

UGatherTextFromAssetsCommandlet::UGatherTextFromAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MinFreeMemoryBytes(0)
	, MaxUsedMemoryBytes(0)
	, NumPackagesDupLocId(0)
	, bSkipGatherCache(false)
	, bSearchAllAssets(true)
	, bShouldGatherFromEditorOnlyData(false)
	, bShouldExcludeDerivedClasses(false)
	, bFixPackageLocalizationIdConflict(false)
{
}

void UGatherTextFromAssetsCommandlet::ProcessGatherableTextDataArray(const TArray<FGatherableTextData>& GatherableTextDataArray)
{
	
	for (const FGatherableTextData& GatherableTextData : GatherableTextDataArray)
	{
		for (const FTextSourceSiteContext& TextSourceSiteContext : GatherableTextData.SourceSiteContexts)
		{
			if (!TextSourceSiteContext.IsEditorOnly || bShouldGatherFromEditorOnlyData)
			{
				if (TextSourceSiteContext.KeyName.IsEmpty())
				{
					UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Detected missing key on asset '{location}'.",
						("location", *TextSourceSiteContext.SiteDescription),
						("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
					);
					continue;
				}

				static const FLocMetadataObject DefaultMetadataObject;

				FManifestContext Context;
				Context.Key = TextSourceSiteContext.KeyName;
				Context.KeyMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.KeyMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.KeyMetaData)) : nullptr;
				Context.InfoMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.InfoMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.InfoMetaData)) : nullptr;
				Context.bIsOptional = TextSourceSiteContext.IsOptional;
				Context.SourceLocation = TextSourceSiteContext.SiteDescription;
				Context.PlatformName = GetSplitPlatformNameFromPath(TextSourceSiteContext.SiteDescription);

				FLocItem Source(GatherableTextData.SourceData.SourceString);

				GatherManifestHelper->AddSourceText(GatherableTextData.NamespaceName, Source, Context);
			}
		}
	}
}

void CalculateDependenciesImpl(IAssetRegistry& InAssetRegistry, const FName& InPackageName, TSet<FName>& OutDependencies, TMap<FName, TSet<FName>>& InOutPackageNameToDependencies)
{
	const TSet<FName>* CachedDependencies = InOutPackageNameToDependencies.Find(InPackageName);

	if (!CachedDependencies)
	{
		// Add a dummy entry now to avoid any infinite recursion for this package as we build the dependencies list
		InOutPackageNameToDependencies.Add(InPackageName);

		// Build the complete list of dependencies for this package
		TSet<FName> LocalDependencies;
		{
			TArray<FName> LocalDependenciesArray;
			InAssetRegistry.GetDependencies(InPackageName, LocalDependenciesArray);

			LocalDependencies.Append(LocalDependenciesArray);
			for (const FName& LocalDependency : LocalDependenciesArray)
			{
				CalculateDependenciesImpl(InAssetRegistry, LocalDependency, LocalDependencies, InOutPackageNameToDependencies);
			}
		}

		// Add the real data now
		CachedDependencies = &InOutPackageNameToDependencies.Add(InPackageName, MoveTemp(LocalDependencies));
	}

	check(CachedDependencies);
	OutDependencies.Append(*CachedDependencies);
}

void UGatherTextFromAssetsCommandlet::CalculateDependenciesForPackagesPendingGather()
{
	UE_SCOPED_TIMER(TEXT("UGatherTextFromAssetsCommandlet::CalculateDependenciesForPackagesPendingGather"), LogGatherTextFromAssetsCommandlet, Display);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	TMap<FName, TSet<FName>> PackageNameToDependencies;

	for (FPackagePendingGather& PackagePendingGather : PackagesPendingGather)
	{
		CalculateDependenciesImpl(AssetRegistry, PackagePendingGather.PackageName, PackagePendingGather.Dependencies, PackageNameToDependencies);
	}
}

bool UGatherTextFromAssetsCommandlet::HasExceededMemoryLimit(const bool bLog)
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();

	const uint64 FreeMemoryBytes = MemStats.AvailablePhysical;
	if (MinFreeMemoryBytes > 0u && FreeMemoryBytes < MinFreeMemoryBytes)
	{
		UE_CLOG(bLog, LogGatherTextFromAssetsCommandlet, Display, TEXT("Free system memory is currently %s, which is less than the requested limit of %s; a flush will be performed."), *FText::AsMemory(FreeMemoryBytes).ToString(), *FText::AsMemory(MinFreeMemoryBytes).ToString());
		return true;
	}

	const uint64 UsedMemoryBytes = MemStats.UsedPhysical;
	if (MaxUsedMemoryBytes > 0u && UsedMemoryBytes >= MaxUsedMemoryBytes)
	{
		UE_CLOG(bLog, LogGatherTextFromAssetsCommandlet, Display, TEXT("Used process memory is currently %s, which is greater than the requested limit of %s; a flush will be performed."), *FText::AsMemory(UsedMemoryBytes).ToString(), *FText::AsMemory(MaxUsedMemoryBytes).ToString());
		return true;
	}

	return false;
}

void UGatherTextFromAssetsCommandlet::PurgeGarbage(const bool bPurgeReferencedPackages)
{
	check(ObjectsToKeepAlive.Num() == 0);

	FlushAsyncLoading();

	TSet<FName> LoadedPackageNames;
	TSet<FName> PackageNamesToKeepAlive;

	if (!bPurgeReferencedPackages)
	{
		// Build a complete list of packages that we still need to keep alive, either because we still 
		// have to process them, or because they're a dependency for something we still have to process
		for (const FPackagePendingGather& PackagePendingGather : PackagesPendingGather)
		{
			PackageNamesToKeepAlive.Add(PackagePendingGather.PackageName);
			PackageNamesToKeepAlive.Append(PackagePendingGather.Dependencies);
		}

		for (TObjectIterator<UPackage> PackageIt; PackageIt; ++PackageIt)
		{
			UPackage* Package = *PackageIt;
			if (PackageNamesToKeepAlive.Contains(Package->GetFName()))
			{
				LoadedPackageNames.Add(Package->GetFName());

				// Keep any requested packages (and their RF_Standalone inners) alive during a call to PurgeGarbage
				ObjectsToKeepAlive.Add(Package);
				ForEachObjectWithPackage(Package, [this](UObject* InPackageInner)
				{
					if (InPackageInner->HasAnyFlags(RF_Standalone | RF_HasExternalPackage))
					{
						ObjectsToKeepAlive.Add(InPackageInner);
					}
					return true;
				}, true, RF_NoFlags, EInternalObjectFlags::Garbage);
			}
		}
	}

	CollectGarbage(IsRunningCommandlet() ? RF_NoFlags : GARBAGE_COLLECTION_KEEPFLAGS);
	ObjectsToKeepAlive.Reset();

	// Fully process the shader compilation results when performing a full purge, as it's the only way to reclaim that memory
	if (bPurgeReferencedPackages && GShaderCompilingManager)
	{
		GShaderCompilingManager->ProcessAsyncResults(false, false);
	}

	if (!bPurgeReferencedPackages)
	{
		// Sort the remaining packages to gather so that currently loaded packages are processed first, followed by those with the most dependencies
		// This aims to allow packages to be GC'd as soon as possible once nothing is no longer referencing them as a dependency
		// Note: This array is processed backwards, so "first" is actually the end of the array
		PackagesPendingGather.Sort([&LoadedPackageNames](const FPackagePendingGather& PackagePendingGatherOne, const FPackagePendingGather& PackagePendingGatherTwo)
		{
			const bool bIsPackageOneLoaded = LoadedPackageNames.Contains(PackagePendingGatherOne.PackageName);
			const bool bIsPackageTwoLoaded = LoadedPackageNames.Contains(PackagePendingGatherTwo.PackageName);
			return (bIsPackageOneLoaded == bIsPackageTwoLoaded) 
				? PackagePendingGatherOne.Dependencies.Num() < PackagePendingGatherTwo.Dependencies.Num() 
				: bIsPackageTwoLoaded;
		});
	}
}

void UGatherTextFromAssetsCommandlet::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	// Keep any requested objects alive during a call to PurgeGarbage
	UGatherTextFromAssetsCommandlet* This = CastChecked<UGatherTextFromAssetsCommandlet>(InThis);
	Collector.AddReferencedObjects(This->ObjectsToKeepAlive);
}

bool IsGatherableTextDataIdentical(const TArray<FGatherableTextData>& GatherableTextDataArrayOne, const TArray<FGatherableTextData>& GatherableTextDataArrayTwo)
{
	struct FSignificantGatherableTextData
	{
		FLocKey Identity;
		FString SourceString;
	};

	auto ExtractSignificantGatherableTextData = [](const TArray<FGatherableTextData>& InGatherableTextDataArray)
	{
		TArray<FSignificantGatherableTextData> SignificantGatherableTextDataArray;

		for (const FGatherableTextData& GatherableTextData : InGatherableTextDataArray)
		{
			for (const FTextSourceSiteContext& TextSourceSiteContext : GatherableTextData.SourceSiteContexts)
			{
				SignificantGatherableTextDataArray.Add({ FString::Printf(TEXT("%s:%s"), *GatherableTextData.NamespaceName, *TextSourceSiteContext.KeyName), GatherableTextData.SourceData.SourceString });
			}
		}

		SignificantGatherableTextDataArray.Sort([](const FSignificantGatherableTextData& SignificantGatherableTextDataOne, const FSignificantGatherableTextData& SignificantGatherableTextDataTwo)
		{
			return SignificantGatherableTextDataOne.Identity < SignificantGatherableTextDataTwo.Identity;
		});

		return SignificantGatherableTextDataArray;
	};

	TArray<FSignificantGatherableTextData> SignificantGatherableTextDataArrayOne = ExtractSignificantGatherableTextData(GatherableTextDataArrayOne);
	TArray<FSignificantGatherableTextData> SignificantGatherableTextDataArrayTwo = ExtractSignificantGatherableTextData(GatherableTextDataArrayTwo);

	if (SignificantGatherableTextDataArrayOne.Num() != SignificantGatherableTextDataArrayTwo.Num())
	{
		return false;
	}

	// These arrays are sorted by identity, so everything should match as we iterate through the array
	// If it doesn't, then these caches aren't identical
	for (int32 Idx = 0; Idx < SignificantGatherableTextDataArrayOne.Num(); ++Idx)
	{
		const FSignificantGatherableTextData& SignificantGatherableTextDataOne = SignificantGatherableTextDataArrayOne[Idx];
		const FSignificantGatherableTextData& SignificantGatherableTextDataTwo = SignificantGatherableTextDataArrayTwo[Idx];

		if (SignificantGatherableTextDataOne.Identity != SignificantGatherableTextDataTwo.Identity)
		{
			return false;
		}

		if (!SignificantGatherableTextDataOne.SourceString.Equals(SignificantGatherableTextDataTwo.SourceString, ESearchCase::CaseSensitive))
		{
			return false;
		}
	}

	return true;
}

bool UGatherTextFromAssetsCommandlet::ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const
{
	const FString* GatherType = ParamVals.Find(UGatherTextCommandletBase::GatherTypeParam);
	// If the param is not specified, it is assumed that both source and assets are to be gathered 
	return !GatherType || *GatherType == TEXT("Asset") || *GatherType == TEXT("All");
}

/**
 * Builds the first pass filter which currently consists of the collection filter and the optional derived class filter.
 * See BuildCollectionFilter and BuildExcludeDerivedClassesFilter
 */
bool UGatherTextFromAssetsCommandlet::BuildFirstPassFilter(FARFilter& InOutFilter) const
{
	// Filter object paths to only those in any of the specified collections.
	if (!BuildCollectionFilter(InOutFilter, CollectionFilters))
	{
		return false;
	}

	// Filter object paths to those in IncludePathFilters, if possible
	if (!BuildPackagePathsFilter(InOutFilter))
	{
		return false;
	}

	// Filter out any objects of the specified classes and their children at this point.
	if (bShouldExcludeDerivedClasses)
	{
		if (!BuildExcludeDerivedClassesFilter(InOutFilter))
		{
			return false;
		}
	}

	InOutFilter.bIncludeOnlyOnDiskAssets = true;
	InOutFilter.WithoutPackageFlags = PKG_Cooked;

	return true;
}

/** Builds a filter based on the specified collections to be used for gathering.*/
bool UGatherTextFromAssetsCommandlet::BuildCollectionFilter(FARFilter& InOutFilter, const TArray<FString>& Collections) const
{
	bool bHasFailedToGetACollection = false;
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	ICollectionManager& CollectionManager = CollectionManagerModule.Get();
	for (const FString& Collection : Collections)
	{
		TSharedPtr<ICollectionContainer> CollectionContainer;
		FName CollectionName;
		ECollectionShareType::Type ShareType = ECollectionShareType::CST_All;
		if (!CollectionManager.TryParseCollectionPath(Collection, &CollectionContainer, &CollectionName, &ShareType) ||
			!CollectionContainer->GetObjectsInCollection(CollectionName, ShareType, InOutFilter.SoftObjectPaths, ECollectionRecursionFlags::SelfAndChildren))
		{
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "Failed get objects in specified collection: {collection}",
				("collection", *Collection),
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
			bHasFailedToGetACollection = true;
		}
	}


	return !bHasFailedToGetACollection;
}

/** Builds a filter to include assets based on the current IncludePathFilters, if those filters can be represented as an asset registry filter */
bool UGatherTextFromAssetsCommandlet::BuildPackagePathsFilter(FARFilter& InOutFilter) const
{
	TArray<FName> IncludePackagePaths;

	for (const FString& IncludePath : IncludePathFilters)
	{
		FString AbsoluteIncludePath = FPaths::ConvertRelativePathToFull(IncludePath);
		if (FFuzzyPathMatcher::CalculatePolicyForPath(AbsoluteIncludePath) != FFuzzyPathMatcher::EPathTestPolicy::StartsWith)
		{
			// Not valid to use as an asset registry filter, but not an error
			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Skipping first pass PackagePaths filter due to complex IncludePathFilters: %s"), *IncludePath);
			return true;
		}

		FNameBuilder IncludePackagePath;
		AbsoluteIncludePath.LeftChopInline(1);
		if (!FPackageName::TryConvertFilenameToLongPackageName(AbsoluteIncludePath, IncludePackagePath))
		{
			// Check if we're just missing the Content folder (eg, "Plugins/Foo" rather than "Plugins/Foo/Content")
			AbsoluteIncludePath /= TEXT("Content");
			if (!FPackageName::TryConvertFilenameToLongPackageName(AbsoluteIncludePath, IncludePackagePath))
			{
				// Not valid to use as an asset registry filter, but not an error
				UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Skipping first pass PackagePaths filter due to non-content IncludePathFilters: %s"), *IncludePath);
				return true;
			}
		}

		IncludePackagePaths.Add(FName(IncludePackagePath));
	}

	InOutFilter.bRecursivePaths = true;
	InOutFilter.PackagePaths.Append(MoveTemp(IncludePackagePaths));
	return true;
}

/** Builds a filter to remove classes and derived classes of ExactClassFilter.*/
bool UGatherTextFromAssetsCommandlet::BuildExcludeDerivedClassesFilter(FARFilter& InOutFilter) const
{
	InOutFilter.bRecursiveClasses = true;
	InOutFilter.ClassPaths.Add(UObject::StaticClass()->GetClassPathName());
	for (const FString& ExcludeClassName : ExcludeClassNames)
	{
		FTopLevelAssetPath ExcludedClassPathName = UClass::TryConvertShortTypeNameToPathName<UClass>(ExcludeClassName, ELogVerbosity::Warning, TEXT("GatherTextFromAssetsCommandlet"));
		if (!ExcludedClassPathName.IsNull())
		{
			// Note: Can't necessarily validate these class names here, as the class may be a generated blueprint class that hasn't been loaded yet.
			InOutFilter.RecursiveClassPathsExclusionSet.Add(FTopLevelAssetPath(ExcludeClassName));
		}
		else
		{
			UE_CLOG(!ExcludeClassName.IsEmpty(), LogGatherTextFromAssetsCommandlet, Error, TEXT("Unable to convert short class name \"%s\" to path name. Please use path names fo ExcludeClassNames"), *ExcludeClassName);
		}
	}
	
	return true;
}

/** Builds a filter to exclude exactly the specified classes. This will retrieve the exact assets from the asset registry to exclude. */
bool UGatherTextFromAssetsCommandlet::BuildExcludeExactClassesFilter(FARFilter& InOutFilter) const
{
	InOutFilter.bRecursiveClasses = false;
	for (const FString& ExcludeClassName : ExcludeClassNames)
	{
		FTopLevelAssetPath ExcludedClassPathName = UClass::TryConvertShortTypeNameToPathName<UClass>(ExcludeClassName, ELogVerbosity::Warning, TEXT("GatherTextFromAssetsCommandlet"));
		if (!ExcludedClassPathName.IsNull())
		{
			// Note: Can't necessarily validate these class names here, as the class may be a generated blueprint class that hasn't been loaded yet.
			InOutFilter.ClassPaths.Add(FTopLevelAssetPath(ExcludeClassName));
		}
		else
		{
			UE_CLOG(!ExcludeClassName.IsEmpty(), LogGatherTextFromAssetsCommandlet, Error, TEXT("Unable to convert short class name \"%s\" to path name. Please use path names fo ExcludeClassNames"), *ExcludeClassName);
		}
	}
	return true;
}

/** Filters out assets that fail the IncludePath and ExcludePath wildcard filters. */
void UGatherTextFromAssetsCommandlet::FilterAssetsBasedOnIncludeExcludePaths(TArray<FAssetData>& InOutAssetDataArray) const
{
	UE_SCOPED_TIMER(TEXT("UGatherTextFromAssetsCommandlet::FilterAssetsBasedOnIncludeExcludePaths"), LogGatherTextFromAssetsCommandlet, Display);
	// We pre-process the package filters into 2 sets because comparing wild cards is expensive 
	// This is the array for cases like *.uasset, *.umap 
	// We only store the extension without the wildcard for an optimization later 
	TArray<FString> PackageFileFiltersStartingWithWildcard;
	// For everything else. We will assume that we will need a wildcard match in this case 
	TArray<FString> OtherPackageFileFilters;

	for (const FString& PackageFileNameFilter : PackageFileNameFilters)
	{
		FString Extension;
		FString CleanPackageFileName;
		PackageFileNameFilter.Split(TEXT("."), &CleanPackageFileName, &Extension);
		if ((CleanPackageFileName.Len() == 1) && (CleanPackageFileName[0] == TEXT('*')) && !Extension.Contains(TEXT("*")))
		{
			// We drop the * from say *.uasset and just keep the extension 
			PackageFileFiltersStartingWithWildcard.Add(PackageFileNameFilter.RightChop(1));
		}
		else
		{
			OtherPackageFileFilters.Add(PackageFileNameFilter);
		}
	}

	const FFuzzyPathMatcher FuzzyPathMatcher = FFuzzyPathMatcher(IncludePathFilters, ExcludePathFilters);
	TArray<bool> PackagesToFilter;
	PackagesToFilter.Init( false, InOutAssetDataArray.Num());
	ParallelFor(InOutAssetDataArray.Num(), [&](int32 Index)
		{
			const FAssetData& PartiallyFilteredAssetData = InOutAssetDataArray[Index];
			if (PartiallyFilteredAssetData.IsRedirector())
			{
				// Redirectors never have localization
				PackagesToFilter[Index] = true;
				return;
			}

			FString PackageFilePathWithoutExtension;
			if (!FPackageName::TryConvertLongPackageNameToFilename(PartiallyFilteredAssetData.PackageName.ToString(), PackageFilePathWithoutExtension))
			{
				// This means the asset data is for content that isn't mounted - this can happen when using a cooked asset registry
				PackagesToFilter[Index] = true;
				return;
			}

			FString PackageFilePathWithExtension;
			if (!FPackageName::FindPackageFileWithoutExtension(PackageFilePathWithoutExtension, PackageFilePathWithExtension))
			{
				// This means the package file doesn't exist on disk, which means we cannot gather it
				PackagesToFilter[Index] = true;
				return;
			}

			PackageFilePathWithExtension = FPaths::ConvertRelativePathToFull(PackageFilePathWithExtension);
			const FString PackageFileName = FPaths::GetCleanFilename(PackageFilePathWithExtension);

			// Filter out assets whose package file names DO NOT match any of the package file name filters.
			{
				bool bHasPassedAnyFileNameFilter = false;
				// This is an optimization to process package file filters in the form *.uasset or *.umap differently
				// FString::MatchesWildcard is an expensive call so we try and minimize the call to that and we go with FString::EndsWith instead for better performance
				for (const FString& PackageFileNameFilter : PackageFileFiltersStartingWithWildcard)
				{
					if (PackageFileName.EndsWith(PackageFileNameFilter))
					{
						bHasPassedAnyFileNameFilter = true;
						break;
					}
				}

				for (const FString& PackageFileNameFilter : OtherPackageFileFilters)
				{
					if (PackageFileName.MatchesWildcard(PackageFileNameFilter))
					{
						bHasPassedAnyFileNameFilter = true;
						break;
					}
				}
				if (!bHasPassedAnyFileNameFilter)
				{
					PackagesToFilter[Index] = true;
					return;
				}
			}

			// Filter out assets whose package file paths do not pass the "fuzzy path" filters.
			if (FuzzyPathMatcher.TestPath(PackageFilePathWithExtension) != FFuzzyPathMatcher::EPathMatch::Included)
			{
				PackagesToFilter[Index] = true;
				return;
			}
		}, UE::Private::GatherTextFromAssetsCommandlet::bParallelizeIncludeExcludePathFiltering ? EParallelForFlags::None: EParallelForFlags::ForceSingleThread);

	check(PackagesToFilter.Num() == InOutAssetDataArray.Num());
	for (int32 Index = InOutAssetDataArray.Num() - 1; Index >= 0; --Index)
	{
		if (PackagesToFilter[Index])
		{
			InOutAssetDataArray.RemoveAtSwap(Index, EAllowShrinking::No);
		}
	}
}

/** Remove any external actors that currently exist in InOutAssetDataArray. OutExternalActorsWorldPackageNames is populated with the package paths of worlds using external actors.*/
void UGatherTextFromAssetsCommandlet::RemoveExistingExternalActors(TArray<FAssetData>& InOutAssetDataArray, const TSet<FName>* WorldPackageFilter, TSet<FName>& OutExternalActorsWorldPackageNames, TSet<FName>& OutGameFeatureDataPackageNames) const
{
	auto IsGameFeatureData = [](const UClass* AssetClass)
	{
		static const FTopLevelAssetPath GameFeatureDataClassPath = FTopLevelAssetPath("/Script/GameFeatures", "GameFeatureData"); // cannot include UGameFeatureData as it is inside a plugin
		for (const UClass* Class = AssetClass; Class; Class = Class->GetSuperClass())
		{
			if (Class->GetClassPathName() == GameFeatureDataClassPath)
			{
				return true;
			}
		}
		return false;
	};

	InOutAssetDataArray.RemoveAll([WorldPackageFilter, &OutExternalActorsWorldPackageNames, &OutGameFeatureDataPackageNames, &IsGameFeatureData](const FAssetData& AssetData)
		{
			const FNameBuilder PackageNameStr(AssetData.PackageName);

			if (AssetData.AssetClassPath == UWorld::StaticClass()->GetClassPathName())
			{
				if (WorldPackageFilter && !WorldPackageFilter->Contains(AssetData.PackageName))
				{
					return true;
				}
				else if (ULevel::GetIsLevelUsingExternalActorsFromAsset(AssetData))
				{
					OutExternalActorsWorldPackageNames.Add(AssetData.PackageName);
				}
			}
			else if (PackageNameStr.ToView().Contains(FPackagePath::GetExternalActorsFolderName()))
			{
				// Remove any external actors that are already in the list, as they will be re-added providing their owner world passed the gather criteria
				// It is possible for an external actor to be directly specified for gather in the configs but have their world not pass the gather criteria.
				return true;
			}
			else if (IsGameFeatureData(AssetData.GetClass()))
			{
				OutGameFeatureDataPackageNames.Add(AssetData.PackageName);
			}

			return false;
		});
}

/** Appends any external actors that also need to be gathered to the InOutAssetDataArray. */
bool UGatherTextFromAssetsCommandlet::DiscoverExternalActors(TArray<FAssetData>& InOutAssetDataArray)
{
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Discovering external actors to gather..."));
	const double DiscoveringExternalActorsStartTime = FPlatformTime::Seconds();

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	// If we have WorldCollectionFilters specified, then use those to filter the external actors we gather
	TOptional<TSet<FName>> WorldPackageFilter;
	if (WorldCollectionFilters.Num() > 0)
	{
		FARFilter Filter;
		if (!BuildCollectionFilter(Filter, WorldCollectionFilters))
		{
			return false;
		}
		Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.WithoutPackageFlags = PKG_Cooked;

		TArray<FAssetData> FilteredWorldAssets;
		AssetRegistry.GetAssets(Filter, FilteredWorldAssets);

		TSet<FName> FilteredWorldPackages;
		FilteredWorldPackages.Reserve(FilteredWorldAssets.Num());
		Algo::Transform(FilteredWorldAssets, FilteredWorldPackages, &FAssetData::PackageName);
		WorldPackageFilter.Emplace(MoveTemp(FilteredWorldPackages));
	}

	TSet<FName> ExternalActorsWorldPackageNames;
	TSet<FName> GameFeatureDataPackageNames;
	RemoveExistingExternalActors(InOutAssetDataArray, WorldPackageFilter.GetPtrOrNull(), ExternalActorsWorldPackageNames, GameFeatureDataPackageNames);

	// Append the actors that are directly known by each world (by looking for their external actors under the expected path)
	if (ExternalActorsWorldPackageNames.Num() > 0)
	{
		// Note: This doesn't add AActor to ClassPaths as that doesn't work correctly doing a partial asset scan (see bSearchAllAssets)
		FARFilter Filter;
		for (const FName ExternalActorsWorldPackageName : ExternalActorsWorldPackageNames)
		{
			FNameBuilder PackageNameStr(ExternalActorsWorldPackageName);
			Filter.PackagePaths.Add(*ULevel::GetExternalActorsPath(*PackageNameStr));
		}
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.WithoutPackageFlags = PKG_Cooked;
		
		TArray<FAssetData> PotentialExternalActors;
		AssetRegistry.GetAssets(Filter, PotentialExternalActors);
		for (FAssetData& PotentialExternalActor : PotentialExternalActors)
		{
			if (FWorldPartitionActorDescUtils::IsValidActorDescriptorFromAssetData(PotentialExternalActor))
			{
				InOutAssetDataArray.Add(MoveTemp(PotentialExternalActor));
			}
		}
	}

	// Append additional actors for each world
	// These are actors added via ExternalDataLayers or ContentBundles, and will be referenced by the GameFeatureData assets that add them (via a GameFeatureAction)
	if (GameFeatureDataPackageNames.Num() > 0)
	{
		TArray<FAssetData> GameFeatureDataDependencies;
		{
			// Note: This doesn't add AActor to ClassPaths as that doesn't work correctly doing a partial asset scan (see bSearchAllAssets)
			FARFilter Filter;
			for (const FName GameFeatureDataPackageName : GameFeatureDataPackageNames)
			{
				AssetRegistry.GetDependencies(GameFeatureDataPackageName, Filter.PackageNames);
			}
			Filter.bIncludeOnlyOnDiskAssets = true;
			Filter.WithoutPackageFlags = PKG_Cooked;

			if (Filter.PackageNames.Num() > 0)
			{
				AssetRegistry.GetAssets(Filter, GameFeatureDataDependencies);
			}
		}

		// External actors may be filtered in two ways;
		//  1. If WorldCollectionFilters were provided, then we only include actors related to those worlds
		//  2. If ExternalActorsWorldPackageNames was populated (meaning there are worlds in this gather), then we only include actors related to those worlds
		// If neither of the above is true then we include all actors related to the GFDs in this gather, as we assume this is a plugin hosting external actors for worlds owned by another localization target
		const bool bHasWorldFilter = WorldPackageFilter.IsSet() || ExternalActorsWorldPackageNames.Num() > 0;
		const TSet<FName>& GameFeatureDataActorsWorldPackageFilter = WorldPackageFilter.Get(ExternalActorsWorldPackageNames);
		for (FAssetData& GameFeatureDataDependency : GameFeatureDataDependencies)
		{
			const FName OptionalOuterPathName = GameFeatureDataDependency.GetOptionalOuterPathName();
			if (!OptionalOuterPathName.IsNone() && FWorldPartitionActorDescUtils::IsValidActorDescriptorFromAssetData(GameFeatureDataDependency))
			{
				const FName OptionalOuterPackageName = FName(FPackageName::ObjectPathToPackageName(FNameBuilder(OptionalOuterPathName).ToView()));
				if (!bHasWorldFilter || GameFeatureDataActorsWorldPackageFilter.Contains(OptionalOuterPackageName))
				{
					InOutAssetDataArray.Add(MoveTemp(GameFeatureDataDependency));
				}
				else
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, VeryVerbose, TEXT("Skipping external actor package (%s) as its associated world package (%s) is not relevant to this gather."), *GameFeatureDataDependency.GetSoftObjectPath().ToString(), *OptionalOuterPackageName.ToString());
				}
			}
		}
	}

	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Discovering external actors took %.2f seconds."), FPlatformTime::Seconds() - DiscoveringExternalActorsStartTime);
	return true;
}

/** Applies the passed in filter to the asset registry. If the filter is empty, the entire asset registry will be returned in InOutAssetDataArray. Else assets that pass the filter will be in InOutAssetDataArray.*/
void UGatherTextFromAssetsCommandlet::ApplyFirstPassFilter(const FARFilter& InFilter, TArray<FAssetData>& InOutAssetDataArray) const
{
	// Apply filter if valid to do so, get all assets otherwise.
	if (InFilter.IsEmpty())
	{
		// @TODOLocalization: Logging that the first path filter is empty resulting in all assets being gathered can confuse users who generally rely on the second pass.
		// Figure out a good way to still convey the information in a log or clog.
		const double GetAllAssetsStartTime = FPlatformTime::Seconds();
		IAssetRegistry::GetChecked().GetAllAssets(InOutAssetDataArray);
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Loading all assets from asset registry took %.2f seconds."), FPlatformTime::Seconds() - GetAllAssetsStartTime);
	}
	else
	{
		const double GetAllAssetsWithFirstPassFilterStartTime = FPlatformTime::Seconds();
		IAssetRegistry::GetChecked().GetAssets(InFilter, InOutAssetDataArray);
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Getting all assets with first pass filter from asset registry took %.2f seconds."), FPlatformTime::Seconds() - GetAllAssetsWithFirstPassFilterStartTime);
	}
}

/**
 * Builds and applies the first pass filter to the entire AssetRegistry. OutAssetDataArray will hold all the assets that pass the first pass filter.
 * See BuildFirstPassFilter and ApplyFirstPassFilter
 */
bool UGatherTextFromAssetsCommandlet::PerformFirstPassFilter(TArray<FAssetData>& OutAssetDataArray) const
{
	FARFilter FirstPassFilter;

	if (!BuildFirstPassFilter(FirstPassFilter))
	{
		return false;
	}
	ApplyFirstPassFilter(FirstPassFilter, OutAssetDataArray);
	return true;
}

void UGatherTextFromAssetsCommandlet::ApplyExcludeExactClassesFilter(const FARFilter& InFilter, TArray<FAssetData>& InOutAssetDataArray) const
{
	// NOTE: The filter applied is actually the inverse, due to API limitations, so the resultant set must be removed from the current set.
	TArray<FAssetData> AssetsToExclude = InOutAssetDataArray;
	IAssetRegistry::GetChecked().RunAssetsThroughFilter(AssetsToExclude, InFilter);
	InOutAssetDataArray.RemoveAll([&](const FAssetData& AssetData)
		{
			return AssetsToExclude.Contains(AssetData);
		});
}

/**
 * Filters out assets from the exact specified classes . Assets that pass the filter will be in InOutAssetDataArray.
 * See BuildExactClassesFilter and ApplyExactClassesFilter
 */
bool UGatherTextFromAssetsCommandlet::PerformExcludeExactClassesFilter(TArray<FAssetData>& InOutAssetDataArray) const
{
	const double ExcludeDerivedClassesStartTime = FPlatformTime::Seconds();
	// Filter out any objects of the specified classes.
	FARFilter ExcludeExactClassesFilter;
	if (!BuildExcludeExactClassesFilter(ExcludeExactClassesFilter))
	{
		return false;
	}

	// Reapply filter over the current set of assets.
	if (!ExcludeExactClassesFilter.IsEmpty())
	{
		ApplyExcludeExactClassesFilter(ExcludeExactClassesFilter, InOutAssetDataArray);
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Filtering out derived classes took %.2f seconds."), FPlatformTime::Seconds() - ExcludeDerivedClassesStartTime);
	}
	return true;
}

bool UGatherTextFromAssetsCommandlet::ParseCommandLineHelper(const FString& InCommandLine)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*InCommandLine, Tokens, Switches, ParamVals);

	FString GatherTextConfigPath;
	FString SectionName;
	if (!GetConfigurationScript(ParamVals, GatherTextConfigPath, SectionName))
	{
		return false;
	}

	if (!ConfigureFromScript(GatherTextConfigPath, SectionName))
	{
		return false;
	}

	{
		FGatherTextContext Context;
		Context.CommandletClass = GetClass()->GetClassPathName();
		Context.PreferredPathType = FGatherTextContext::EPreferredPathType::Content;

		FGatherTextDelegates::GetAdditionalGatherPathsForContext.Broadcast(GatherManifestHelper->GetTargetName(), Context, IncludePathFilters, ExcludePathFilters);
	}

	// Get destination path
	if (!GetPathFromConfig(*SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath))
	{
		UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "No destination path specified.",
			("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
		);
		return false;
	}

	// Add any manifest dependencies if they were provided
	{
		bool bHasFailedToAddManifestDependency = false;
		for (const FString& ManifestDependency : ManifestDependenciesList)
		{
			FText OutError;
			if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
			{
				UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "The GatherTextFromAssets commandlet couldn't load the specified manifest dependency: '{manifestDependency}'. {error}",
					("manifestDependency", *ManifestDependency),
					("error", *OutError.ToString()),
					("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
				);
				bHasFailedToAddManifestDependency = true;
			}
		}
		if (bHasFailedToAddManifestDependency)
		{
			return false;
		}
	}

	// Preload necessary modules.
	{
		bool bHasFailedToPreloadAnyModules = false;
		for (const FString& ModuleName : ModulesToPreload)
		{
			EModuleLoadResult ModuleLoadResult;
			FModuleManager::Get().LoadModuleWithFailureReason(*ModuleName, ModuleLoadResult);

			if (ModuleLoadResult != EModuleLoadResult::Success)
			{
				UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Failed to preload dependent module {module}. Please check if the modules have been renamed or moved to another folder.",
					("module", *ModuleName),
					("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
				);
				bHasFailedToPreloadAnyModules = true;
				continue;
			}
		}

		if (bHasFailedToPreloadAnyModules)
		{
			return false;
		}
	}

	return true;
}

UGatherTextFromAssetsCommandlet::FPackagePendingGather* UGatherTextFromAssetsCommandlet::AppendPackagePendingGather(const FName PackageNameToGather)
{
	FString PackageFilename;
	if (!FPackageName::FindPackageFileWithoutExtension(FPackageName::LongPackageNameToFilename(PackageNameToGather.ToString()), PackageFilename))
	{
		return nullptr;
	}
	PackageFilename = FPaths::ConvertRelativePathToFull(PackageFilename);

	FPackagePendingGather& PackagePendingGather = PackagesPendingGather.AddDefaulted_GetRef();
	PackagePendingGather.PackageName = PackageNameToGather;
	PackagePendingGather.PackageFilename = MoveTemp(PackageFilename);
	PackagePendingGather.PackageLocCacheState = EPackageLocCacheState::Cached;
	return &PackagePendingGather;
}

TSet<FName> UGatherTextFromAssetsCommandlet::GetPackageNamesToGather(const TArray<FAssetData>& InAssetDataArray) const
{
	// Collapse the assets down to a set of packages
	TSet<FName> PackageNamesToGather;
	PackageNamesToGather.Reserve(InAssetDataArray.Num());
	for (const FAssetData& AssetData : InAssetDataArray)
	{
		PackageNamesToGather.Add(AssetData.PackageName);
	}
	return PackageNamesToGather;
}

void UGatherTextFromAssetsCommandlet::PopulatePackagesPendingGather(TSet<FName> PackageNamesToGather)
{
	const double PopulationStartTime = FPlatformTime::Seconds();
	// Build the basic information for the packages to gather (dependencies are filled in later once we've processed cached packages)
	PackagesPendingGather.Reserve(PackageNamesToGather.Num());
	for (const FName& PackageNameToGather : PackageNamesToGather)
	{
		AppendPackagePendingGather(PackageNameToGather);
	}
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Populating pending packages took %.2f seconds."), FPlatformTime::Seconds() - PopulationStartTime);
}

/** Process packages with loc data cached in its header and removes them from the pending packages.*/
void UGatherTextFromAssetsCommandlet::ProcessAndRemoveCachedPackages(TMap<FName, TSet<FGuid>>& OutExternalActorsWithStaleOrMissingCaches)
{
	UE_SCOPED_TIMER(TEXT("UGatherTextFromAssetsCommandlet::ProcessAndRemoveCachedPackages"), LogGatherTextFromAssetsCommandlet, Display);

	const int32 InitialNumPackagesToGather = PackagesPendingGather.Num();

	// Load any cached localization data in parallel to maximize file throughput
	// We do not update any shared state during this pass, and only update the data within FPackagePendingGather
	{
		const EParallelForFlags LoadingLoopFlags = UE::Private::GatherTextFromAssetsCommandlet::bParallelizeProcessAndRemoveCachedPackages ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread;
		const int32 LoadingLoopNumElements = InitialNumPackagesToGather;
		const int32 LoadingLoopMinBatchSize = UE::Private::GatherTextFromAssetsCommandlet::ProcessAndRemoveCachedPackagesMaxThreads <= 0
			? 1 // Batch size of 1 uses as many threads as ParallelFor allows
			: (LoadingLoopNumElements / FMath::Max(UE::Private::GatherTextFromAssetsCommandlet::ProcessAndRemoveCachedPackagesMaxThreads - 1, 1)) + 1; // -1 from MaxThreads as ParallelFor will include the game thread internally
		const int32 LoadingLoopNumThreads = ParallelForImpl::GetNumberOfThreadTasks(LoadingLoopNumElements, LoadingLoopMinBatchSize, LoadingLoopFlags);

		const double LoadStartTime = FPlatformTime::Seconds();
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Loading the cached localization data for %d package(s) using %d thread(s). This may take a while..."), InitialNumPackagesToGather, LoadingLoopNumThreads);

		ParallelFor(TEXT("UGatherTextFromAssetsCommandlet::ProcessAndRemoveCachedPackages"), LoadingLoopNumElements, LoadingLoopMinBatchSize, [this](int32 Index)
		{
			FPackagePendingGather& PackagePendingGather = PackagesPendingGather[Index];

			const FNameBuilder PackageNameStr(PackagePendingGather.PackageName);
			const bool bIsExternalActorPackage = PackageNameStr.ToView().Contains(FPackagePath::GetExternalActorsFolderName());

			TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*PackagePendingGather.PackageFilename));
			if (!FileReader)
			{
				return;
			}

			// Read package file summary from the file.
			FPackageFileSummary PackageFileSummary;
			*FileReader << PackageFileSummary;

			PackagePendingGather.PackageLocalizationId = PackageFileSummary.LocalizationId;
			PackagePendingGather.PackageLocCacheState = CalculatePackageLocCacheState(PackageFileSummary, PackagePendingGather.PackageName, bIsExternalActorPackage);

			// Read the cached localization data
			if (PackagePendingGather.PackageLocCacheState == EPackageLocCacheState::Cached && PackageFileSummary.GatherableTextDataOffset > 0)
			{
				FileReader->Seek(PackageFileSummary.GatherableTextDataOffset);

				PackagePendingGather.GatherableTextDataArray.SetNum(PackageFileSummary.GatherableTextDataCount);
				for (int32 GatherableTextDataIndex = 0; GatherableTextDataIndex < PackageFileSummary.GatherableTextDataCount; ++GatherableTextDataIndex)
				{
					*FileReader << PackagePendingGather.GatherableTextDataArray[GatherableTextDataIndex];
				}
			}
		}, LoadingLoopFlags);

		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Loading the cached localization data for %d package(s) took %.2f seconds."), InitialNumPackagesToGather, FPlatformTime::Seconds() - LoadStartTime);
	}

	// Now that everything has been loaded (or not), run through the cached data, ingest it, update the metrics, and update PackagesPendingGather
	{
		const double IngestStartTime = FPlatformTime::Seconds();
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Ingesting the cached localization data for %d package(s). This may take a while..."), InitialNumPackagesToGather);

		int32 NumPackagesProcessed = 0;
		TMap<FString, FName> AssignedPackageLocalizationIds;
		PackagesPendingGather.RemoveAll([this, &OutExternalActorsWithStaleOrMissingCaches, &AssignedPackageLocalizationIds, &NumPackagesProcessed, InitialNumPackagesToGather](const FPackagePendingGather& PackagePendingGather) -> bool
		{
			FNameBuilder PackageNameStr(PackagePendingGather.PackageName);
			const int32 CurrentPackageNum = ++NumPackagesProcessed;
			const float PercentageComplete = static_cast<float>(CurrentPackageNum) / static_cast<float>(InitialNumPackagesToGather) * 100.0f;

			const bool bIsExternalActorPackage = PackageNameStr.ToView().Contains(FPackagePath::GetExternalActorsFolderName());

			// Track the package localization ID of this package (if known) and detect duplicates
			bool bThisPackageHasLocIdConflictToFix = false;
			if (!PackagePendingGather.PackageLocalizationId.IsEmpty())
			{
				// if this package's localization ID is a duplicate
				if (const FName* ExistingLongPackageName = AssignedPackageLocalizationIds.Find(PackagePendingGather.PackageLocalizationId))
				{
					UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Package '{file}' and '{conflictFile}' have the same localization ID ({locKey}). Please reset one of these (Asset Localization -> Reset Localization ID) to avoid conflicts.", ("file", *PackageNameStr), ("conflictFile", *ExistingLongPackageName->ToString()), ("locKey", *PackagePendingGather.PackageLocalizationId), ("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier));
					if (bFixPackageLocalizationIdConflict) 
					{
						bThisPackageHasLocIdConflictToFix = true;
						// add this package to the list of packages with a duplicate localization ID
						PackagesWithDuplicateLocalizationIds.Add(PackagePendingGather.PackageName);
						NumPackagesDupLocId++;
					}
				}
				else
				{
					// This package is not a duplicate so it is added to Assigned Package Localization IDs dictionary
					AssignedPackageLocalizationIds.Add(PackagePendingGather.PackageLocalizationId, PackagePendingGather.PackageName);
				}
			}

			if (PackagePendingGather.PackageLocCacheState != EPackageLocCacheState::Cached)
			{
				// External actors must be gathered via their owner world rather than via a raw LoadPackage call
				// Remove them from PackagesToGather as the owner world is merged back in below
				if (bIsExternalActorPackage)
				{
					TArray<FAssetData> ActorsInPackage;
					IAssetRegistry::GetChecked().GetAssetsByPackageName(PackagePendingGather.PackageName, ActorsInPackage);
					for (const FAssetData& ActorInPackage : ActorsInPackage)
					{
						if (TUniquePtr<FWorldPartitionActorDesc> ActorDesc = FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(ActorInPackage))
						{
							FName WorldPackageName = *FPackageName::ObjectPathToPackageName(ActorDesc->GetActorSoftPath().ToString());
							OutExternalActorsWithStaleOrMissingCaches.FindOrAdd(WorldPackageName).Add(ActorDesc->GetGuid());
						}
					}
					return true;
				}

				UE::Private::GatherTextFromAssetsCommandlet::AssetGatherCacheMetrics.CountUncachedAsset(PackagePendingGather.PackageLocCacheState);
				return false;
			}

			// Process packages that don't require loading to process.
			if (PackagePendingGather.GatherableTextDataArray.Num() > 0)
			{
				UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("[%6.2f%%] Gathering package: '%s'..."), PercentageComplete, *PackageNameStr);

				UE::Private::GatherTextFromAssetsCommandlet::AssetGatherCacheMetrics.CountCachedAsset();

				ProcessGatherableTextDataArray(PackagePendingGather.GatherableTextDataArray);
			}

			// If we're reporting or fixing assets with a stale gather cache then we still need to load this 
			// package in order to do that, but the PackageLocCacheState prevents it being gathered again
			if (bReportStaleGatherCache || bFixStaleGatherCache || bThisPackageHasLocIdConflictToFix)
			{
				check(PackagePendingGather.PackageLocCacheState == EPackageLocCacheState::Cached);
				return false;
			}

			return true;
		});

		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Ingesting the cached localization data for %d package(s) took %.2f seconds."), InitialNumPackagesToGather, FPlatformTime::Seconds() - IngestStartTime);
	}
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Number of packages with duplicate loc ids: %d"), NumPackagesDupLocId);
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Number of packages to load: %d"), PackagesPendingGather.Num());
}

void UGatherTextFromAssetsCommandlet::MergeInExternalActorsWithStaleOrMissingCaches(TMap<FName, TSet<FGuid>>& StaleExternalActors)
{
	for (TTuple<FName, TSet<FGuid>>& StaleExternalActorsPair : StaleExternalActors)
	{
		FPackagePendingGather* WorldPackagePendingGather = PackagesPendingGather.FindByPredicate([&StaleExternalActorsPair](const FPackagePendingGather& PotentialPackagePendingGather)
			{
				return PotentialPackagePendingGather.PackageName == StaleExternalActorsPair.Key;
			});
		if (!WorldPackagePendingGather)
		{
			WorldPackagePendingGather = AppendPackagePendingGather(StaleExternalActorsPair.Key);
		}

		if (WorldPackagePendingGather)
		{
			WorldPackagePendingGather->ExternalActors = MoveTemp(StaleExternalActorsPair.Value);
			WorldPackagePendingGather->PackageLocCacheState = EPackageLocCacheState::Uncached_TooOld;
		}
		else
		{
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Failed to queue world package '{package}' for {nbExternalActors} external actor(s).",
				("package", *StaleExternalActorsPair.Key.ToString()),
				("nbExternalActors", StaleExternalActorsPair.Value.Num()),
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
		}
	}
}

/** Load the remaining pending packages for gather.*/
bool UGatherTextFromAssetsCommandlet::LoadAndProcessUncachedPackages(TArray<FName>& OutPackagesWithStaleGatherCache)
{
	UE_SCOPED_TIMER(TEXT("UGatherTextFromAssetsCommandlet::LoadAndProcessUncachedPackages"), LogGatherTextFromAssetsCommandlet, Display);
	FLoadPackageLogOutputRedirector LogOutputRedirector;
	TArray<FGatherableTextData> GatherableTextDataArray;
	int32 NumPackagesProcessed = 0;
	int32 PackageCount = PackagesPendingGather.Num();
	int32 NumPackagesFailedLoading = 0;
	
	FScopedSlowTask SlowTask(PackageCount, NSLOCTEXT("GatherTextCommandlet", "LoadAndProcessUncachedPackages", "Loading and Gathering Packages..."));
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Number of packages with duplicate loc ids: %d"), NumPackagesDupLocId);
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Number of packages to load: %d"), PackagesPendingGather.Num());

	// Auxiliary lambda function for resaving packages
	auto ResavePackage = [this, &LogOutputRedirector](
		const FStringView PackageNameStr,
		UPackage* Package,
		const FString& PackageFileName
		) -> bool
	{
		FLoadPackageLogOutputRedirector::FScopedCapture ScopedCapture(&LogOutputRedirector, PackageNameStr);
		return FLocalizedAssetSCCUtil::SavePackageWithSCC(SourceControlInfo, Package, PackageFileName);
	};

	while (PackagesPendingGather.Num() > 0)
	{
		const FPackagePendingGather PackagePendingGather = PackagesPendingGather.Pop(EAllowShrinking::No);
		FNameBuilder PackageNameStr(PackagePendingGather.PackageName);

		const int32 CurrentPackageNum = ++NumPackagesProcessed;
		const float PercentageComplete = static_cast<float>(CurrentPackageNum) / static_cast<float>(PackageCount) * 100.0f;
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("[%6.2f%%] Loading package: '%s'..."), PercentageComplete, *PackageNameStr);

		SlowTask.EnterProgressFrame();

		if (SlowTask.ShouldCancel() || (EmbeddedContext && EmbeddedContext->ShouldAbort()))
		{
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "GatherText aborted!",
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
			return false;
		}

		UPackage* Package = nullptr;
		{
			FLoadPackageLogOutputRedirector::FScopedCapture ScopedCapture(&LogOutputRedirector, *PackageNameStr);
			if (PackagePendingGather.ExternalActors.Num() > 0)
			{
				Package = LoadWorldPackageForEditor(*PackageNameStr, EWorldType::Editor, LOAD_NoWarn | LOAD_Quiet);
			}
			else
			{
				Package = LoadPackage(nullptr, *PackageNameStr, LOAD_NoWarn | LOAD_Quiet);
			}
		}

		if (!Package)
		{
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Failed to load package: '{package}'.",
				("package", *PackageNameStr),
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
			++NumPackagesFailedLoading;
			continue;
		}

		// if fixing duplicate localization package IDs mode is enabled, 
		// and the package is in the list of packages with duplicate localization IDs,
		// reset the package's localization ID
		bool bPackageLocIdWasReset = false;
		if (bFixPackageLocalizationIdConflict)
		{
			if (PackagesWithDuplicateLocalizationIds.Contains(PackagePendingGather.PackageName))
			{
				// This package's localization ID and name have been found in the duplicates dictionary, and the flag for fixing duplicates is on
				// So resetting the localization ID for that package
				UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("About to reset localization id for package: '%s'."), *PackageNameStr);
				TextNamespaceUtil::ClearPackageNamespace(Package);
				TextNamespaceUtil::EnsurePackageNamespace(Package);
				bPackageLocIdWasReset = true;
				UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Localization ID has been reset for package: '%s'."), *PackageNameStr);
				
				// Now resaving to ensure the in-memory IDs are updated in the package before the data gatherer runs over it
				UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Resaving package: '%s'..."), *PackageNameStr);
				if (!ResavePackage(PackageNameStr, Package, PackagePendingGather.PackageFilename))
				{
					UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Failed to resave package: '{package}'.",
						("package", *PackageNameStr),
						("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
					);
				}
			}
		}

		// Tick background tasks
		UE::Private::GatherTextFromAssetsCommandlet::TickBackgroundTasks();
		if (EmbeddedContext)
		{
			EmbeddedContext->RunTick();
		}

		// Because packages may not have been resaved after this flagging was implemented, we may have added packages to load that weren't flagged - potential false positives.
		// The loading process should have reflagged said packages so that only true positives will have this flag.
		if (Package->RequiresLocalizationGather() || PackagePendingGather.ExternalActors.Num() > 0)
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("[%6.2f%%] Gathering package: '%s'..."), PercentageComplete, *PackageNameStr);

			// Gathers from the given package
			EPropertyLocalizationGathererResultFlags GatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;
			FPropertyLocalizationDataGatherer(GatherableTextDataArray, Package, GatherableTextResultFlags);

			bool bSavePackage = false;

			// Optionally check to see whether the clean gather we did is in-sync with the gather cache and deal with it accordingly
			if ((bReportStaleGatherCache || bFixStaleGatherCache) && PackagePendingGather.PackageLocCacheState == EPackageLocCacheState::Cached)
			{
				// Look for any structurally significant changes (missing, added, or changed texts) in the cache
				// Ignore insignificant things (like source changes caused by assets moving or being renamed)
				if (EnumHasAnyFlags(GatherableTextResultFlags, EPropertyLocalizationGathererResultFlags::HasTextWithInvalidPackageLocalizationID)
					|| !IsGatherableTextDataIdentical(GatherableTextDataArray, PackagePendingGather.GatherableTextDataArray))
				{
					OutPackagesWithStaleGatherCache.Add(PackagePendingGather.PackageName);

					if (bFixStaleGatherCache)
					{
						bSavePackage = true;
					}
				}
			}

			// Optionally save the package if it is missing a gather cache
			if (bFixMissingGatherCache && PackagePendingGather.PackageLocCacheState == EPackageLocCacheState::Uncached_TooOld)
			{
				bSavePackage = true;
			}

			// if the package localization ID was reset in duplicate localization ID fixing mode, it needs to be resaved
			if(bPackageLocIdWasReset)
			{ 
				bSavePackage = true;
			}

			// Re-save the package to attempt to fix it?
			if (bSavePackage)
			{
				UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Resaving package: '%s'..."), *PackageNameStr);
				if (!ResavePackage(PackageNameStr, Package, PackagePendingGather.PackageFilename))
				{
					UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Failed to resave package: '{package}'.",
						("package", *PackageNameStr),
						("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
					);
				}
			}

			// If this is a WP world then query the localization for any external actors actors that were determined to be stale
			if (PackagePendingGather.ExternalActors.Num() > 0)
			{
				if (UWorld* World = UWorld::FindWorldInPackage(Package))
				{
					UWorld::InitializationValues IVS;
					IVS.InitializeScenes(false);
					IVS.AllowAudioPlayback(false);
					IVS.RequiresHitProxies(false);
					IVS.CreatePhysicsScene(false);
					IVS.CreateNavigation(false);
					IVS.CreateAISystem(false);
					IVS.ShouldSimulatePhysics(false);
					IVS.EnableTraceCollision(false);
					IVS.SetTransactional(false);
					IVS.CreateFXSystem(false);
					IVS.CreateWorldPartition(true);

					TOptional<FScopedEditorWorld> ScopeEditorWorld;
					if (!World->IsInitialized())
					{
						FLoadPackageLogOutputRedirector::FScopedCapture ScopedCapture(&LogOutputRedirector, *PackageNameStr);
						ScopeEditorWorld.Emplace(World, IVS); // Initializing FScopedEditorWorld can log warnings, so capture those like we do with loading errors
					}

					if (UWorldPartition* WorldPartition = World->GetWorldPartition())
					{
						// ForEachActorWithLoading may GC while running, so keep the world partition (and indirectly the world and its package) alive
						TGCObjectScopeGuard<UWorldPartition> WorldPartitionGCGuard(WorldPartition);

						FWorldPartitionHelpers::FForEachActorWithLoadingParams ForEachActorParams;
						ForEachActorParams.ActorGuids = PackagePendingGather.ExternalActors.Array();

						FWorldPartitionHelpers::ForEachActorWithLoading(WorldPartition, [&GatherableTextDataArray](const FWorldPartitionActorDescInstance* ActorDescInstance)
							{
								if (const AActor* Actor = ActorDescInstance->GetActor())
								{
									EPropertyLocalizationGathererResultFlags ActorGatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;
									FPropertyLocalizationDataGatherer(GatherableTextDataArray, Actor->GetExternalPackage(), ActorGatherableTextResultFlags);
								}
								return true;
							}, ForEachActorParams);
					}
				}
			}

			// This package may have already been cached in cases where we're reporting or fixing assets with a stale gather cache
			// This check prevents it being gathered a second time
			if (PackagePendingGather.PackageLocCacheState != EPackageLocCacheState::Cached)
			{
				ProcessGatherableTextDataArray(GatherableTextDataArray);
			}

			GatherableTextDataArray.Reset();
		}
		else if (bPackageLocIdWasReset)
		{
			// Resaving a second time in case of package localization ID reset
			// to fix the on-disk cache to match the new in-memory IDs
			UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Resaving package: '%s'..."), *PackageNameStr);
			if (!ResavePackage(PackageNameStr, Package, PackagePendingGather.PackageFilename))
			{
				UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "Failed to resave package: '{package}'.",
					("package", *PackageNameStr),
					("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
				);
			}
		}

		if (HasExceededMemoryLimit(/*bLog*/true))
		{
			// First try a minimal purge to only remove things that are no longer referenced or needed by other packages pending gather
			PurgeGarbage(/*bPurgeReferencedPackages*/false);

			if (HasExceededMemoryLimit(/*bLog*/false))
			{
				// If we're still over the memory limit after a minimal purge, then attempt a full purge
				PurgeGarbage(/*bPurgeReferencedPackages*/true);

				// If we're still over the memory limit after both purges, then log a warning as we may be about to OOM
				UE_CLOG(HasExceededMemoryLimit(/*bLog*/false), LogGatherTextFromAssetsCommandlet, Warning, TEXT("Flushing failed to reduce process memory to within the requested limits; this process may OOM!"));
			}
		}
	}

	// Clear list of packages with duplicate localization IDs
	PackagesWithDuplicateLocalizationIds.Empty();

	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Loaded %d packages. %d failed."), NumPackagesProcessed, NumPackagesFailedLoading);
	return true;
}

void UGatherTextFromAssetsCommandlet::ReportStaleGatherCache(TArray<FName>& InPackagesWithStaleGatherCache) const
{
	InPackagesWithStaleGatherCache.Sort(FNameLexicalLess());

	FString StaleGatherCacheReport;
	for (const FName& PackageWithStaleGatherCache : InPackagesWithStaleGatherCache)
	{
		StaleGatherCacheReport += PackageWithStaleGatherCache.ToString();
		StaleGatherCacheReport += TEXT("\n");
	}

	const FString StaleGatherCacheReportFilename = DestinationPath / TEXT("StaleGatherCacheReport.txt");
	const bool bStaleGatherCacheReportSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, StaleGatherCacheReportFilename, [&StaleGatherCacheReport](const FString& InSaveFileName) -> bool
		{
			return FFileHelper::SaveStringToFile(StaleGatherCacheReport, *InSaveFileName, FFileHelper::EEncodingOptions::ForceUTF8);
		});

	if (!bStaleGatherCacheReportSaved)
	{
		UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "Failed to save report: '{report}'.",
			("report", *StaleGatherCacheReportFilename),
			("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
		);
	}
}

UGatherTextFromAssetsCommandlet::EPackageLocCacheState UGatherTextFromAssetsCommandlet::CalculatePackageLocCacheState(const FPackageFileSummary& PackageFileSummary, const FName PackageName, bool bIsExternalActorPackage) const
{
	// Have we been asked to skip the cache of text that exists in the header of newer packages?
	if (bSkipGatherCache && PackageFileSummary.GetFileVersionUE() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
	{
		// Fallback on the old package flag check.
		if (PackageFileSummary.GetPackageFlags() & PKG_RequiresLocalizationGather)
		{
			return EPackageLocCacheState::Uncached_NoCache;
		}
	}

	const FCustomVersion* const EditorVersion = PackageFileSummary.GetCustomVersionContainer().GetVersion(FEditorObjectVersion::GUID);
	const FCustomVersion* const FNMainVersion = PackageFileSummary.GetCustomVersionContainer().GetVersion(FFortniteMainBranchObjectVersion::GUID);

	// Packages not resaved since localization gathering flagging was added to packages must be loaded.
	if (PackageFileSummary.GetFileVersionUE() < VER_UE4_PACKAGE_REQUIRES_LOCALIZATION_GATHER_FLAGGING)
	{
		return EPackageLocCacheState::Uncached_TooOld;
	}
	// Package not resaved since gatherable text data was added to package headers must be loaded, since their package header won't contain pregathered text data.
	else if (PackageFileSummary.GetFileVersionUE() < VER_UE4_SERIALIZE_TEXT_IN_PACKAGES || (!EditorVersion || EditorVersion->Version < FEditorObjectVersion::GatheredTextEditorOnlyPackageLocId))
	{
		// Fallback on the old package flag check.
		if (PackageFileSummary.GetPackageFlags() & PKG_RequiresLocalizationGather)
		{
			return EPackageLocCacheState::Uncached_TooOld;
		}
	}
	else if (PackageFileSummary.GetFileVersionUE() < VER_UE4_DIALOGUE_WAVE_NAMESPACE_AND_CONTEXT_CHANGES)
	{
		TArray<FAssetData> AllAssetDataInSamePackage;
		IAssetRegistry::GetChecked().GetAssetsByPackageName(PackageName, AllAssetDataInSamePackage);
		for (const FAssetData& AssetData : AllAssetDataInSamePackage)
		{
			if (AssetData.AssetClassPath == UDialogueWave::StaticClass()->GetClassPathName())
			{
				return EPackageLocCacheState::Uncached_TooOld;
			}
		}
	}
	else if (bIsExternalActorPackage && (!FNMainVersion || FNMainVersion->Version < FFortniteMainBranchObjectVersion::FixedLocalizationGatherForExternalActorPackage))
	{
		// Fallback on the old package flag check.
		if (PackageFileSummary.GetPackageFlags() & PKG_RequiresLocalizationGather)
		{
			return EPackageLocCacheState::Uncached_TooOld;
		}
	}

	// If this package doesn't have any cached data, then we have to load it for gather
	if (PackageFileSummary.GetFileVersionUE() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES && PackageFileSummary.GatherableTextDataOffset == 0 && (PackageFileSummary.GetPackageFlags() & PKG_RequiresLocalizationGather))
	{
		return EPackageLocCacheState::Uncached_NoCache;
	}
	return EPackageLocCacheState::Cached;
}

int32 UGatherTextFromAssetsCommandlet::Main(const FString& Params)
{
	UE_SCOPED_TIMER(TEXT("UGatherTextFromAssetsCommandlet::Main"), LogGatherTextFromAssetsCommandlet, Display);
	// Parse command line.
	if (!ParseCommandLineHelper(Params))
	{
		return -1;
	}

	// If the editor has loaded a persistent world then create an empty world prior to starting the asset gather
	// This avoids any issues when loading and initializing worlds during the gather, as WP needs to re-initialize the world
	// Note: We can skip this when running embedded within a normal editor (ie, not a commandlet) as editor worlds are already fully initialized
	if (IsRunningCommandlet() && GEditor)
	{
		if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
			EditorWorld && !FPackageName::IsTempPackage(FNameBuilder(EditorWorld->GetPackage()->GetFName()).ToView()))
		{
			GEditor->CreateNewMapForEditing(/*bPromptForSave*/false);
		}
	}

	FARFilter FirstPassFilter;
	if (!BuildFirstPassFilter(FirstPassFilter))
	{
		return -1;
	}
	
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Discovering assets to gather..."));
	const double DiscoveringAssetsStartTime = FPlatformTime::Seconds();
	{
		UE_SCOPED_TIMER(TEXT("UGatherTextFromAssetsCommandlet::SearchAssetRegistryForAllAssets"), LogGatherTextFromAssetsCommandlet, Display);
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		if (bSearchAllAssets || FirstPassFilter.PackagePaths.IsEmpty())
		{
			AssetRegistry.SearchAllAssets(true);
		}
		else
		{
			TArray<FString> ScanPaths;
			ScanPaths.Reserve(FirstPassFilter.PackagePaths.Num());
			Algo::Transform(FirstPassFilter.PackagePaths, ScanPaths, [](FName PackagePath) { return PackagePath.ToString(); });

			// Note: We don't use FirstPassFilter.SoftObjectPaths as the set of files to scan, as ScanSynchronous can perform poorly when given large numbers of files to scan
			AssetRegistry.ScanSynchronous(ScanPaths, TArray<FString>(), UE::AssetRegistry::EScanFlags::IgnoreInvalidPathWarning);
		}
	}

	TArray<FAssetData> AssetDataArray;
	ApplyFirstPassFilter(FirstPassFilter, AssetDataArray);

	if (!bShouldExcludeDerivedClasses)
	{
		if (!PerformExcludeExactClassesFilter(AssetDataArray))
		{
			return -1;
		}
	}

	// Note: AssetDataArray now contains all assets in the specified collections that are not instances of the specified excluded classes.
	FilterAssetsBasedOnIncludeExcludePaths(AssetDataArray);

	if (AssetDataArray.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("No assets matched the specified criteria."));
		return 0;
	}

	// Discover the external actors for any worlds that are pending gather
	if (!DiscoverExternalActors(AssetDataArray))
	{
		return -1;
	}

	// Collect the basic information about the packages that we're going to gather from
	TSet<FName> PackageNamesToGather = GetPackageNamesToGather(AssetDataArray);
	AssetDataArray.Empty();
	PopulatePackagesPendingGather(MoveTemp(PackageNamesToGather));

	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Discovering assets to gather took %.2f seconds."), FPlatformTime::Seconds() - DiscoveringAssetsStartTime);

	// These are external actor packages that are stale or are missing a gather cache from their package
	// Map of world package name -> external actor Ids in the world 
	TMap<FName, TSet<FGuid>> ExternalActorsWithStaleOrMissingCaches;
	// Process all packages that do not need to be loaded. Remove processed packages from the list.
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Processing assets to gather..."));
	ProcessAndRemoveCachedPackages(ExternalActorsWithStaleOrMissingCaches);

	UE::Private::GatherTextFromAssetsCommandlet::AssetGatherCacheMetrics.LogMetrics();

	// Merge any pending WP map requests back into PackagesPendingGather
	MergeInExternalActorsWithStaleOrMissingCaches(ExternalActorsWithStaleOrMissingCaches);
	ExternalActorsWithStaleOrMissingCaches.Reset();

	// All packages left in PackagesPendingGather should now have to be loaded 
	if (PackagesPendingGather.Num() == 0)
	{
		// Nothing more to do!
		return 0;
	}

	const double PackageLoadingStartTime = FPlatformTime::Seconds();
	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Preparing to load %d packages..."), PackagesPendingGather.Num());

	CalculateDependenciesForPackagesPendingGather();

	// Collect garbage before beginning to load packages
	// This also sorts the list of packages into the best processing order
	PurgeGarbage(/*bPurgeReferencedPackages*/false);

	// We don't need to have compiled shaders to gather text
	bool bWasShaderCompilationEnabled = false;
	if (GShaderCompilingManager)
	{
		bWasShaderCompilationEnabled = !GShaderCompilingManager->IsShaderCompilationSkipped();
		GShaderCompilingManager->SkipShaderCompilation(true);
	}

	TArray<FName> PackagesWithStaleGatherCache;
	if (!LoadAndProcessUncachedPackages(PackagesWithStaleGatherCache))
	{
		return -1;
	}

	UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("Loading packages took %.2f seconds."), FPlatformTime::Seconds() - PackageLoadingStartTime);

	// Collect garbage after loading all packages
	// This reclaims as much memory as possible for the rest of the gather pipeline
	PurgeGarbage(/*bPurgeReferencedPackages*/true);
	
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->SkipShaderCompilation(!bWasShaderCompilationEnabled);
	}
	
	if (bReportStaleGatherCache)
	{
		ReportStaleGatherCache(PackagesWithStaleGatherCache);
	}

	return 0;
}

bool UGatherTextFromAssetsCommandlet::GetConfigurationScript(const TMap<FString, FString>& InCommandLineParameters, FString& OutFilePath, FString& OutStepSectionName)
{
	//Set config file
	const FString* ParamVal = InCommandLineParameters.Find(FString(TEXT("Config")));
	if (ParamVal)
	{
		OutFilePath = *ParamVal;
	}
	else
	{
		UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "No config specified.",
			("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
		);
		return false;
	}

	//Set config section
	ParamVal = InCommandLineParameters.Find(FString(TEXT("Section")));
	if (ParamVal)
	{
		OutStepSectionName = *ParamVal;
	}
	else
	{
		UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "No config section specified.",
			("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
		);
		return false;
	}

	return true;
}

bool UGatherTextFromAssetsCommandlet::ConfigureFromScript(const FString& GatherTextConfigPath, const FString& SectionName)
{
	bool bHasFatalError = false;

	// Modules to Preload
	GetStringArrayFromConfig(*SectionName, TEXT("ModulesToPreload"), ModulesToPreload, GatherTextConfigPath);

	// IncludePathFilters
	GetPathArrayFromConfig(*SectionName, TEXT("IncludePathFilters"), IncludePathFilters, GatherTextConfigPath);

	// IncludePaths (DEPRECATED)
	{
		TArray<FString> IncludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);
		if (IncludePaths.Num())
		{
			IncludePathFilters.Append(IncludePaths);
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "IncludePaths detected in section {section}. IncludePaths is deprecated, please use IncludePathFilters.",
				("section", *SectionName),
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
		}
	}

	if (IncludePathFilters.Num() == 0)
	{
		UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "No include path filters in section {section}.",
			("section", *SectionName),
			("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
		);
		bHasFatalError = true;
	}

	// Collections
	{
		auto GetAndValidateCollections = [this, &bHasFatalError, &SectionName, &GatherTextConfigPath](const TCHAR* KeyName, TArray<FString>& OutCollections)
		{
			GetStringArrayFromConfig(*SectionName, KeyName, OutCollections, GatherTextConfigPath);
			if (OutCollections.Num() > 0)
			{
				FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
				ICollectionManager& CollectionManager = CollectionManagerModule.Get();
				for (const FString& Collection : OutCollections)
				{
					TSharedPtr<ICollectionContainer> CollectionContainer;
					FName CollectionName;
					ECollectionShareType::Type ShareType = ECollectionShareType::CST_All;
					const bool bDoesCollectionExist = CollectionManager.TryParseCollectionPath(Collection, &CollectionContainer, &CollectionName, &ShareType) &&
						CollectionContainer->CollectionExists(CollectionName, ShareType);
					if (!bDoesCollectionExist)
					{
						UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "Failed to find collection '{collection}', collection does not exist.",
							("collection", *Collection),
							("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
						);
						bHasFatalError = true;
					}
				}
			}
		};

		GetAndValidateCollections(TEXT("CollectionFilters"), CollectionFilters);
		GetAndValidateCollections(TEXT("WorldCollectionFilters"), WorldCollectionFilters);
	}

	// ExcludePathFilters
	GetPathArrayFromConfig(*SectionName, TEXT("ExcludePathFilters"), ExcludePathFilters, GatherTextConfigPath);

	// ExcludePaths (DEPRECATED)
	{
		TArray<FString> ExcludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);
		if (ExcludePaths.Num())
		{
			ExcludePathFilters.Append(ExcludePaths);
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "ExcludePaths detected in section {section}. ExcludePaths is deprecated, please use ExcludePathFilters.",
				("section", *SectionName),
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
		}
	}

	// PackageNameFilters
	GetStringArrayFromConfig(*SectionName, TEXT("PackageFileNameFilters"), PackageFileNameFilters, GatherTextConfigPath);

	// PackageExtensions (DEPRECATED)
	{
		TArray<FString> PackageExtensions;
		GetStringArrayFromConfig(*SectionName, TEXT("PackageExtensions"), PackageExtensions, GatherTextConfigPath);
		if (PackageExtensions.Num())
		{
			PackageFileNameFilters.Append(PackageExtensions);
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "PackageExtensions detected in section {section}. PackageExtensions is deprecated, please use PackageFileNameFilters.",
				("section", *SectionName),
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
		}
	}

	if (PackageFileNameFilters.Num() == 0)
	{
		UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Error, "No package file name filters in section {section}.",
			("section", *SectionName),
			("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
		);
		bHasFatalError = true;
	}

	// Recursive asset class exclusion
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldExcludeDerivedClasses"), bShouldExcludeDerivedClasses, GatherTextConfigPath))
	{
		bShouldExcludeDerivedClasses = false;
	}

	// Asset class exclude
	GetStringArrayFromConfig(*SectionName, TEXT("ExcludeClasses"), ExcludeClassNames, GatherTextConfigPath);

	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);

	if (!GetBoolFromConfig(*SectionName, TEXT("SearchAllAssets"), bSearchAllAssets, GatherTextConfigPath))
	{
		bSearchAllAssets = true;
	}

	// Get whether we should gather editor-only data. Typically only useful for the localization of UE itself.
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldGatherFromEditorOnlyData"), bShouldGatherFromEditorOnlyData, GatherTextConfigPath))
	{
		bShouldGatherFromEditorOnlyData = false;
	}

	auto ReadBoolFlagWithFallback = [this, &SectionName, &GatherTextConfigPath](const TCHAR* FlagName, bool& OutValue)
	{
		OutValue = FParse::Param(FCommandLine::Get(), FlagName);
		if (!OutValue)
		{
			GetBoolFromConfig(*SectionName, FlagName, OutValue, GatherTextConfigPath);
		}
		UE_LOG(LogGatherTextFromAssetsCommandlet, Display, TEXT("%s: %s"), FlagName, OutValue ? TEXT("true") : TEXT("false"));
	};

	ReadBoolFlagWithFallback(TEXT("SkipGatherCache"), bSkipGatherCache);
	ReadBoolFlagWithFallback(TEXT("ReportStaleGatherCache"), bReportStaleGatherCache);
	ReadBoolFlagWithFallback(TEXT("FixStaleGatherCache"), bFixStaleGatherCache);
	ReadBoolFlagWithFallback(TEXT("FixMissingGatherCache"), bFixMissingGatherCache);
	ReadBoolFlagWithFallback(TEXT("FixPackageLocalizationIdConflict"), bFixPackageLocalizationIdConflict);

	// Read some settings from the editor config
	{
		int32 MinFreeMemoryMB = 0;
		GConfig->GetInt(TEXT("GatherTextFromAssets"), TEXT("MinFreeMemory"), MinFreeMemoryMB, GEditorIni);
		MinFreeMemoryMB = FMath::Max(MinFreeMemoryMB, 0);
		MinFreeMemoryBytes = MinFreeMemoryMB * 1024LL * 1024LL;

		int32 MaxUsedMemoryMB = 0;
		if (GConfig->GetInt(TEXT("GatherTextFromAssets"), TEXT("MaxMemoryAllowance"), MaxUsedMemoryMB, GEditorIni))
		{
			UE_LOGFMT(LogGatherTextFromAssetsCommandlet, Warning, "The MaxMemoryAllowance config option is deprecated, please use MaxUsedMemory.",
				("id", GatherTextFromAssetsCommandlet::LocalizationLogIdentifier)
			);
		}
		else
		{
			GConfig->GetInt(TEXT("GatherTextFromAssets"), TEXT("MaxUsedMemory"), MaxUsedMemoryMB, GEditorIni);
		}
		MaxUsedMemoryMB = FMath::Max(MaxUsedMemoryMB, 0);
		MaxUsedMemoryBytes = MaxUsedMemoryMB * 1024LL * 1024LL;
	}

	return !bHasFatalError;
}

#undef LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////

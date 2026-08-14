// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoStoreOnDemand.h"

#include "Containers/StringView.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "IO/IoStatus.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "String/Numeric.h"

#if !UE_BUILD_SHIPPING
#include "Misc/PackageName.h"
#endif //!UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY(LogIoStoreOnDemand);
DEFINE_LOG_CATEGORY(LogIas);

namespace UE::IoStore
{

IOnDemandIoStore* GOnDemandIoStore = nullptr;

///////////////////////////////////////////////////////////////////////////////
#if !UE_BUILD_SHIPPING
namespace Commands
{

////////////////////////////////////////////////////////////////////////////////
static TIoStatusOr<FOnDemandHostGroup> SplitHostAndTocUrl(FStringView Url, FStringView& OutTocRelativUrl)
{
	if (Url.StartsWith(TEXTVIEW("http")) == false)
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid URL protocol"));
	}

	int32 Delim = INDEX_NONE;
	if (Url.FindChar(':', Delim) == false)
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid URL"));
	}

	const int32 ProtocolDelim = Delim + 3;
	if (Url.RightChop(ProtocolDelim).FindChar('/', Delim) == false)
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Failed to find host and TOC path delimiter"));
	}

	const FStringView Host	= Url.Left(ProtocolDelim + Delim);
	OutTocRelativUrl		= Url.RightChop(Host.Len());

	return FOnDemandHostGroup::Create(Host);
}

////////////////////////////////////////////////////////////////////////////////
static void PurgeInstallCache(const TArray<FString>& Args)
{
	if (UE::IoStore::IOnDemandIoStore* IoStore = UE::IoStore::TryGetOnDemandIoStore())
	{
		FOnDemandPurgeArgs PurgeArgs;
		for (const FString& Arg : Args)
		{
			if (Arg == TEXTVIEW("defrag"))
			{
				PurgeArgs.Options |= EOnDemandPurgeOptions::Defrag;
			}
			else if (!PurgeArgs.BytesToPurge && UE::String::IsNumericOnlyDigits(Arg))
			{
				uint64 BytesToPurge = 0;
				if (LexTryParseString(BytesToPurge, *Arg))
				{
					PurgeArgs.BytesToPurge = BytesToPurge;
				}
			}
		}

		UE_LOG(LogIoStoreOnDemand, Display, TEXT("Purging on demand install cache"));
		IoStore->Purge(MoveTemp(PurgeArgs), [](const FOnDemandPurgeResult& Result)
		{
			if (Result.Status.IsOk())
			{
				UE_LOG(LogIoStoreOnDemand, Display, TEXT("Purged on demand install cache"));
			}
			else
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed Purged on demand install cache: %s"), *Result.Status.ToString());
			}
		});
	}
	else
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Purge install cache failed, reason 'I/O store on-demand module not initialized'"));
	}
}

////////////////////////////////////////////////////////////////////////////////
static void DefragInstallCache(const TArray<FString>& Args)
{
	if (UE::IoStore::IOnDemandIoStore* IoStore = UE::IoStore::TryGetOnDemandIoStore())
	{
		FOnDemandDefragArgs DefragArgs;
		for (const FString& Arg : Args)
		{
			if (UE::String::IsNumericOnlyDigits(Arg))
			{
				uint64 BytesToFree = 0;
				if (LexTryParseString(BytesToFree, *Arg))
				{
					DefragArgs.BytesToFree = BytesToFree;
					break;
				}
			}
		}

		UE_LOG(LogIoStoreOnDemand, Display, TEXT("Defragging on demand install cache"));
		IoStore->Defrag(MoveTemp(DefragArgs), [](const FOnDemandDefragResult& Result)
		{
			if (Result.Status.IsOk())
			{
				UE_LOG(LogIoStoreOnDemand, Display, TEXT("Defragmented on demand install cache"));
			}
			else
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to defragment on demand install cache: %s"), *Result.Status.ToString());
			}
		});
	}
	else
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Defrag install cache failed, reason 'I/O store on-demand module not initialized'"));
	}
}

////////////////////////////////////////////////////////////////////////////////
static void PrintCacheUsage()
{
	if (UE::IoStore::IOnDemandIoStore* IoStore = UE::IoStore::TryGetOnDemandIoStore())
	{
		FOnDemandGetCacheUsageArgs Args;
		Args.Options = EOnDemandGetCacheUsageOptions::DumpHandlesToLog;

		TIoStatusOr<FOnDemandCacheUsage> MaybeUsage = IoStore->GetCacheUsage(Args);
		if (!MaybeUsage.IsOk())
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("iostore.CacheUsage failed: %s"), *MaybeUsage.Status().ToString());
			return;
		}

		const FOnDemandCacheUsage CacheUsage = MaybeUsage.ConsumeValueOrDie();
		UE_LOG(LogIoStoreOnDemand, Display, TEXT("iostore.CacheUsage"));
		TStringBuilder<512> Sb;
		Sb << TEXTVIEW("InstallCache: ") << CacheUsage.InstallCache << TEXTVIEW(", StreamingCache: ") << CacheUsage.StreamingCache;
		UE_LOG(LogIoStoreOnDemand, Display, TEXT("%s"), Sb.ToString());
	}
	else
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Print cache usage failed, reason 'I/O store on-demand module not initialized'"));
	}
}

////////////////////////////////////////////////////////////////////////////////
static void MountUrl(const TArray<FString>& Args)
{
	UE::IoStore::IOnDemandIoStore* IoStore = UE::IoStore::TryGetOnDemandIoStore();
	if (IoStore == nullptr)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to load I/O store on-demand module."));
		return;
	}

	if (Args.Num() < 2)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Not enough arguments."));
		return;
	}

	FString Url = Args[0];
	Url.TrimQuotesInline();

	FStringView TocRelativeUrl;
	TIoStatusOr<FOnDemandHostGroup> HostGroup = SplitHostAndTocUrl(Url, TocRelativeUrl);
	if (HostGroup.IsOk() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("%s"), *HostGroup.Status().ToString());
		return;
	}

	EOnDemandMountOptions Options = EOnDemandMountOptions::StreamOnDemand;
	const FString& Arg = Args[1];
	if (Arg.ToLower().Contains(TEXT("install")))
	{
		Options = EOnDemandMountOptions::InstallOnDemand; 
	}
	const FString& MountId = Args.Num() > 2 ? Args[2] : Url; 

	IoStore->Mount(
		FOnDemandMountArgs
		{
			.MountId = MountId,
			.TocRelativeUrl = FString(TocRelativeUrl),
			.HostGroup = HostGroup.ConsumeValueOrDie(),
			.Options = Options
		},
		[](FOnDemandMountResult MountResult)
		{
			UE_CLOG(!MountResult.Status.IsOk(), LogIas, Error,
				TEXT("Failed to mount TOC for '%s', reason '%s'"), *MountResult.MountId, *MountResult.Status.ToString());
		});
}

////////////////////////////////////////////////////////////////////////////////
static void MountFile(const TArray<FString>& Args)
{
	UE::IoStore::IOnDemandIoStore* IoStore = UE::IoStore::TryGetOnDemandIoStore();
	if (IoStore == nullptr)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to load I/O store on-demand module."));
		return;
	}

	if (Args.Num() < 3)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Not enough arguments."));
		return;
	}

	TArray<FOnDemandMountArgs> AllMountArgs;
	FString FilenameOrWildcard = Args[0];
	FilenameOrWildcard.TrimQuotesInline();
	TArray<FString> FilePaths;

	const FString ContentDir = FString::Printf(TEXT("%sPaks/"), *FPaths::ProjectContentDir());
	IFileManager& Ifm = IFileManager::Get();
	Ifm.IterateDirectoryRecursively(
		*ContentDir,
		[&FilenameOrWildcard, &FilePaths, &ContentDir](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory == false)
			{
				FString FilePath(FilenameOrDirectory);
				if (FPathViews::GetExtension(FilePath) == TEXT("uondemandtoc"))
				{
					FStringView Filename = FPathViews::GetBaseFilename(FilePath);
					if (Filename == FilenameOrWildcard || FString(Filename).MatchesWildcard(FilenameOrWildcard))
					{
						FilePaths.Add(FilenameOrDirectory);
					}
				}
			}
			return true;
		});

	if (FilePaths.IsEmpty())
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to find any on-demand TOC file(s) matching '%s'"), *FilenameOrWildcard);
		return;
	}

	EOnDemandMountOptions Options = EOnDemandMountOptions::StreamOnDemand;
	if (Args[1].ToLower().Contains(TEXT("install")))
	{
		Options = EOnDemandMountOptions::InstallOnDemand; 
	}

	FString HostPath = Args[2];
	HostPath.TrimQuotesInline();
	FStringView TocRelativeUrl;
	TIoStatusOr<FOnDemandHostGroup> HostGroup = SplitHostAndTocUrl(HostPath, TocRelativeUrl);
	if (HostGroup.IsOk() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("%s"), *HostGroup.Status().ToString());
		return;
	}

	const FString& MountId = Args.Num() > 3 ? Args[3] : FilePaths[0];
	for (const FString& FilePath : FilePaths)
	{
		FOnDemandMountArgs& MountArgs	= AllMountArgs.AddDefaulted_GetRef();
		MountArgs.FilePath				= FilePath;
		MountArgs.HostGroup				= HostGroup.ConsumeValueOrDie();
		MountArgs.TocRelativeUrl		= TocRelativeUrl;
		MountArgs.MountId				= MountId;
	}

	if (AllMountArgs.IsEmpty())
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to parse mount arguments"));
		return;
	}

	for (FOnDemandMountArgs& MountArgs : AllMountArgs)
	{
		MountArgs.Options = Options;
		IoStore->Mount(
			MoveTemp(MountArgs),
			[](FOnDemandMountResult MountResult)
			{
				UE_CLOG(!MountResult.Status.IsOk(), LogIas, Error,
					TEXT("Failed to mount TOC for '%s', reason '%s'"), *MountResult.MountId, *MountResult.Status.ToString());
			});
	}
}

////////////////////////////////////////////////////////////////////////////////
static void InstallPackage(const TArray<FString>& Args)
{
	UE::IoStore::IOnDemandIoStore* IoStore = UE::IoStore::TryGetOnDemandIoStore();
	if (IoStore == nullptr)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to load I/O store on-demand module."));
		return;
	}

	if (Args.Num() < 1)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Not enough arguments."));
		return;
	}

	FString PackageName = Args[0];
	PackageName.TrimQuotesInline();
	FPackageId PackageId;
	if (FPackageName::IsValidLongPackageName(PackageName))
	{
		PackageId = FPackageId::FromName(FName(*PackageName));
	}

	if (PackageId.IsValid() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Invalid package name '%s'"), *PackageName);
		return;
	}

	static FOnDemandContentHandle DefaultContentHandle = FOnDemandContentHandle::Create(TEXT("ConsoleCommand"));

	FOnDemandInstallArgs InstallArgs;
	InstallArgs.PackageIds.Add(PackageId);
	InstallArgs.ContentHandle = DefaultContentHandle;
	InstallArgs.Options = EOnDemandInstallOptions::InstallSoftReferences;

	IoStore->Install(
		MoveTemp(InstallArgs),
		[PackageName = MoveTemp(PackageName)](FOnDemandInstallResult InstallResult)
		{
			if (InstallResult.Status.IsOk())
			{
				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Successfully installed package '%s'"), *PackageName);
			}
			else
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to install package '%s', reason '%s'"),
					*PackageName, *InstallResult.Status.ToString());
			}
		});
}

////////////////////////////////////////////////////////////////////////////////
static void VerifyCache()
{
	UE::IoStore::IOnDemandIoStore* IoStore = UE::IoStore::TryGetOnDemandIoStore();
	if (IoStore == nullptr)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to load I/O store on-demand module."));
		return;
	}

	IoStore->Verify([](FOnDemandVerifyCacheResult&& VerifyResult)
	{
		if (VerifyResult.Status.IsOk())
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Install cache verified OK!"));
		}
		else
		{
			if (VerifyResult.Status.GetErrorCode() == EIoErrorCode::NotFound)
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Verify install cache failed, reason '%s'"),
					*VerifyResult.Status.ToString());
			}
			else
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Verify install cache failed, reason '%s'"),
					*VerifyResult.Status.ToString());
			}
		}
	});
}

////////////////////////////////////////////////////////////////////////////////
static FAutoConsoleCommand PurgeCacheCommand(
	TEXT("iostore.PurgeInstallCache"),
	TEXT("Purge On Demand Install Cache"),
	FConsoleCommandWithArgsDelegate::CreateStatic(PurgeInstallCache),
	ECVF_Cheat);

static FAutoConsoleCommand DefragCacheCommand(
	TEXT("iostore.DefragInstallCache"),
	TEXT("Defragment On Demand Install Cache"),
	FConsoleCommandWithArgsDelegate::CreateStatic(DefragInstallCache),
	ECVF_Cheat);

static FAutoConsoleCommand PrintCacheUsageCommand(
	TEXT("iostore.CacheUsage"),
	TEXT("print cache usage"),
	FConsoleCommandDelegate::CreateStatic(PrintCacheUsage),
	ECVF_Cheat);

static FAutoConsoleCommand MountUrlCommand(
	TEXT("iostore.MountUrl"),
	TEXT("<URL> <Install|Stream> <MountId>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(MountUrl));

static FAutoConsoleCommand MountFileCommand(
	TEXT("iostore.MountFile"),
	TEXT("<Filename|Wildcard> <Install|Stream> <HostPath> <MountId>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(MountFile));

static FAutoConsoleCommand InstallPackageCommand(
	TEXT("iostore.InstallPackage"),
	TEXT("<PackageName>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(InstallPackage));

static FAutoConsoleCommand VerifyCacheCommand(
	TEXT("iostore.VerifyCache"),
	TEXT("Verifies the cache chunks against the mounted container TOCs"),
	FConsoleCommandDelegate::CreateStatic(VerifyCache),
	ECVF_Cheat);

} // namespace Commands
#endif // !UE_BUILD_SHIPPING

static const TCHAR* NotInitializedError = TEXT("I/O store on-demand not initialized");

////////////////////////////////////////////////////////////////////////////////
FStringBuilderBase& operator<<(FStringBuilderBase& Sb, const FOnDemandInstallCacheUsage& CacheUsage)
{
	Sb.Appendf(TEXT("MaxSize=%.2lf MiB, TotalSize=%.2lf MiB, ReferencedBlockSize=%.2lf MiB, ReferencedSize=%.2lf MiB, FragmentedChunksSize=%.2lf MiB"),
		double(CacheUsage.MaxSize) / 1024.0 / 1024.0,
		double(CacheUsage.TotalSize) / 1024.0 / 1024.0,
		double(CacheUsage.ReferencedBlockSize) / 1024.0 / 1024.0,
		double(CacheUsage.ReferencedSize) / 1024.0 / 1024.0,
		double(CacheUsage.FragmentedChunksSize) / 1024.0 / 1024.0);

	return Sb;
}

////////////////////////////////////////////////////////////////////////////////
FStringBuilderBase& operator<<(FStringBuilderBase& Sb, const FOnDemandStreamingCacheUsage& CacheUsage)
{
	Sb.Appendf(TEXT("MaxSize=%.2lf MiB, TotalSize=%.2lf MiB"),
		double(CacheUsage.MaxSize) / 1024.0 / 1024.0, double(CacheUsage.TotalSize) / 1024.0 / 1024.0);
	return Sb;
}

////////////////////////////////////////////////////////////////////////////////
class FIoStoreOnDemandCoreModule final
	: public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandleModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature);
};
IMPLEMENT_MODULE(UE::IoStore::FIoStoreOnDemandCoreModule, IoStoreOnDemandCore);

////////////////////////////////////////////////////////////////////////////////
void FIoStoreOnDemandCoreModule::HandleModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Type != IOnDemandIoStoreFactory::FeatureName || GOnDemandIoStore != nullptr)
	{
		return;
	}

	IOnDemandIoStoreFactory* Factory	= static_cast<IOnDemandIoStoreFactory*>(ModularFeature);
	IOnDemandIoStore* IoStore			= Factory->CreateInstance();

	if (IoStore == nullptr)
	{
		UE_LOG(LogIoStoreOnDemand, Warning, TEXT("I/O store on-demand disabled, reason '%s'"), TEXT("Failed to create I/O store"));
		return;
	}

	if (FIoStatus Status = IoStore->Initialize(); !Status.IsOk())
	{
		Factory->DestroyInstance(IoStore);

		if (Status.GetErrorCode() == EIoErrorCode::Disabled || Status.GetErrorCode() == EIoErrorCode::NotFound)
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("I/O store on-demand disabled, reason '%s'"), *Status.ToString());
		}
		else
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("I/O store on-demand disabled, reason '%s'"), *Status.ToString());
		}
		return;
	}
	#if !UE_IAS_CUSTOM_INITIALIZATION
	if (FIoStatus Status = IoStore->InitializePostHotfix(); !Status.IsOk())
	{
		if (Status.GetErrorCode() == EIoErrorCode::Disabled || Status.GetErrorCode() == EIoErrorCode::NotFound)
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("I/O store post hotfix initialization failed, reason '%s'"), *Status.ToString());
		}
		else
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("I/O store post hotfix initialization failed, reason '%s'"), *Status.ToString());
		}
	}
	#endif
	GOnDemandIoStore = IoStore;
	IModularFeatures::Get().OnModularFeatureRegistered().RemoveAll(this);
}

void FIoStoreOnDemandCoreModule::StartupModule()
{
	IModularFeatures& Features = IModularFeatures::Get();
	if (Features.GetModularFeatureImplementationCount(IOnDemandIoStoreFactory::FeatureName) > 0)
	{
		IModularFeature* Feature = Features.GetModularFeatureImplementation(IOnDemandIoStoreFactory::FeatureName, 0);
		HandleModularFeatureRegistered(IOnDemandIoStoreFactory::FeatureName, Feature);
	}
	else
	{
		Features.OnModularFeatureRegistered().AddRaw(this, &FIoStoreOnDemandCoreModule::HandleModularFeatureRegistered);
	}
}

void FIoStoreOnDemandCoreModule::ShutdownModule()
{
	IOnDemandIoStore* ToDestroy = nullptr;
	Swap(ToDestroy, GOnDemandIoStore);

	if (ToDestroy == nullptr)
	{
		return;
	}

	IModularFeatures& Features = IModularFeatures::Get();
	if (Features.GetModularFeatureImplementationCount(IOnDemandIoStoreFactory::FeatureName) > 0)
	{
		IModularFeature* Feature			= Features.GetModularFeatureImplementation(IOnDemandIoStoreFactory::FeatureName, 0);
		IOnDemandIoStoreFactory* Factory	= static_cast<IOnDemandIoStoreFactory*>(Feature);

		Factory->DestroyInstance(ToDestroy);
	}
	IModularFeatures::Get().OnModularFeatureRegistered().RemoveAll(this);
}

////////////////////////////////////////////////////////////////////////////////
IOnDemandIoStore* TryGetOnDemandIoStore()
{
	return GOnDemandIoStore;
}

////////////////////////////////////////////////////////////////////////////////
IOnDemandIoStore& GetOnDemandIoStore()
{
	check(GOnDemandIoStore != nullptr);
	return *GOnDemandIoStore;
}

FName IOnDemandIoStoreFactory::FeatureName = FName(TEXT("OnDemandIoStoreFactory"));

} // namespace UE::IoStore

////////////////////////////////////////////////////////////////////////////////

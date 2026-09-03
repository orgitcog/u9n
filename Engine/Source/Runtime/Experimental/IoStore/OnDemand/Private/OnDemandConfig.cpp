// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandConfig.h"
#include "IO/IoStatus.h"
#include "OnDemandIoDispatcherBackend.h"
#include "OnDemandInstallCache.h"

#include "HAL/PlatformProcess.h"
#include "IasCache.h"
#include "IO/IoStoreOnDemand.h"
#include "Misc/Base64.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreMisc.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/Fork.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "String/LexFromString.h"

namespace UE::IoStore::Config
{

////////////////////////////////////////////////////////////////////////////////
bool GIoStoreOnDemandInstallCacheEnabled = true;
static FAutoConsoleVariableRef CVar_IoStoreOnDemandInstallCacheEnabled(
	TEXT("iostore.OnDemandInstallCacheEnabled"),
	GIoStoreOnDemandInstallCacheEnabled,
	TEXT("Whether the on-demand install cache is enabled."),
	ECVF_ReadOnly
);

/** Temp cvar to allow the fallback url to be hotfixed in case of problems */
static FString GDistributedEndpointFallbackUrl;
static FAutoConsoleVariableRef CVar_DistributedEndpointFallbackUrl(
	TEXT("ias.DistributedEndpointFallbackUrl"),
	GDistributedEndpointFallbackUrl,
	TEXT("CDN url to be used if a distributed endpoint cannot be reached (overrides IoStoreOnDemand.ini)")
);

////////////////////////////////////////////////////////////////////////////////
static bool ParseEncryptionKeyParam(const FString& Param, FGuid& OutKeyGuid, FAES::FAESKey& OutKey)
{
	TArray<FString> Tokens;
	Param.ParseIntoArray(Tokens, TEXT(":"), true);

	if (Tokens.Num() == 2)
	{
		TArray<uint8> KeyBytes;
		if (FGuid::Parse(Tokens[0], OutKeyGuid) && FBase64::Decode(Tokens[1], KeyBytes))
		{
			if (OutKeyGuid != FGuid() && KeyBytes.Num() == FAES::FAESKey::KeySize)
			{
				FMemory::Memcpy(OutKey.Key, KeyBytes.GetData(), FAES::FAESKey::KeySize);
				return true;
			}
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////
static bool ApplyEncryptionKeyFromString(const FString& GuidKeyPair)
{
	FGuid KeyGuid;
	FAES::FAESKey Key;

	if (ParseEncryptionKeyParam(GuidKeyPair, KeyGuid, Key))
	{
		// TODO: PAK and I/O store should share key manager
		FEncryptionKeyManager::Get().AddKey(KeyGuid, Key);
		FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(KeyGuid, Key);

		return true;
	}
	else
	{
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
static bool TryParseConfigContent(const FString& ConfigContent, const FString& ConfigFileName, FOnDemandEndpointConfig& OutEndpoint)
{
	if (ConfigContent.IsEmpty())
	{
		return false;
	}

	FConfigFile ConfigFile;
	ConfigFile.ProcessInputFileContents(ConfigContent, ConfigFileName);

	ConfigFile.GetString(TEXT("Endpoint"), TEXT("DistributionUrl"), OutEndpoint.DistributionUrl);
	if (!OutEndpoint.DistributionUrl.IsEmpty())
	{
		ConfigFile.GetString(TEXT("Endpoint"), TEXT("FallbackUrl"), OutEndpoint.FallbackUrl);

		if (!GDistributedEndpointFallbackUrl.IsEmpty())
		{
			OutEndpoint.FallbackUrl = GDistributedEndpointFallbackUrl;
		}
	}
	
	ConfigFile.GetArray(TEXT("Endpoint"), TEXT("ServiceUrl"), OutEndpoint.ServiceUrls);
	ConfigFile.GetString(TEXT("Endpoint"), TEXT("TocPath"), OutEndpoint.TocPath);

	if (OutEndpoint.DistributionUrl.EndsWith(TEXT("/")))
	{
		OutEndpoint.DistributionUrl = OutEndpoint.DistributionUrl.Left(OutEndpoint.DistributionUrl.Len() - 1);
	}

	for (FString& ServiceUrl : OutEndpoint.ServiceUrls)
	{
		if (ServiceUrl.EndsWith(TEXT("/")))
		{
			ServiceUrl.LeftInline(ServiceUrl.Len() - 1);
		}
	}

	if (OutEndpoint.TocPath.StartsWith(TEXT("/")))
	{
		OutEndpoint.TocPath.RightChopInline(1);
	}

	FString ContentKey;
	if (ConfigFile.GetString(TEXT("Endpoint"), TEXT("ContentKey"), ContentKey))
	{
		ApplyEncryptionKeyFromString(ContentKey);
	}

	return OutEndpoint.IsValid();
}

////////////////////////////////////////////////////////////////////////////////
int64 ParseSizeParam(FStringView Value)
{
	Value = Value.TrimStartAndEnd();

	int64 Size = -1;
	LexFromString(Size, Value);
	if (Size >= 0)
	{
		if (Value.EndsWith(TEXT("GB"))) return Size << 30;
		if (Value.EndsWith(TEXT("MB"))) return Size << 20;
		if (Value.EndsWith(TEXT("KB"))) return Size << 10;
	}
	return Size;
}

////////////////////////////////////////////////////////////////////////////////
int64 ParseSizeParam(const TCHAR* CommandLine, const TCHAR* Param)
{
	FString ParamValue;
	if (!FParse::Value(CommandLine, Param, ParamValue))
	{
		return -1;
	}

	return ParseSizeParam(ParamValue);
}

////////////////////////////////////////////////////////////////////////////////
FIasCacheConfig GetStreamingCacheConfig(const TCHAR* CommandLine)
{
	FIasCacheConfig Ret;

	// Fetch values from .ini files
	auto GetConfigIntImpl = [CommandLine] (const TCHAR* ConfigKey, const TCHAR* ParamName, auto& Out)
	{
		int64 Value = -1;
		if (FString Temp; GConfig->GetString(TEXT("Ias"), ConfigKey, Temp, GEngineIni))
		{
			Value = ParseSizeParam(Temp);
		}
#if !UE_BUILD_SHIPPING
		if (int64 Override = ParseSizeParam(CommandLine, ParamName); Override >= 0)
		{
			Value = Override;
		}
#endif

		if (Value >= 0)
		{
			Out = decltype(Out)(Value);
		}

		return true;
	};

#define GetConfigInt(Name, Dest) \
	do { GetConfigIntImpl(TEXT("FileCache.") Name, TEXT("Ias.FileCache.") Name TEXT("="), Dest); } while (false)
	GetConfigInt(TEXT("WritePeriodSeconds"),	Ret.WriteRate.Seconds);
	GetConfigInt(TEXT("WriteOpsPerPeriod"),		Ret.WriteRate.Ops);
	GetConfigInt(TEXT("WriteBytesPerPeriod"),	Ret.WriteRate.Allowance);
	GetConfigInt(TEXT("DiskQuota"),				Ret.DiskQuota);
	GetConfigInt(TEXT("MemoryQuota"),			Ret.MemoryQuota);
	GetConfigInt(TEXT("JournalQuota"),			Ret.JournalQuota);
	GetConfigInt(TEXT("JournalMagic"),			Ret.JournalMagic);
	GetConfigInt(TEXT("DemandThreshold"),		Ret.Demand.Threshold);
	GetConfigInt(TEXT("DemandBoost"),			Ret.Demand.Boost);
	GetConfigInt(TEXT("DemandSuperBoost"),		Ret.Demand.SuperBoost);
#undef GetConfigInt

#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("Ias.DropCache")))
	{
		Ret.DropCache = true;
	}
	if (FParse::Param(CommandLine, TEXT("Ias.NoCache")))
	{
		Ret.DiskQuota = 0;
	}
#endif

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FOnDemandEndpointConfig> TryParseEndpointConfig(const TCHAR* CommandLine)
{
	{
		FString EncryptionKey;
		if (FParse::Value(CommandLine, TEXT("Ias.EncryptionKey="), EncryptionKey))
		{
			ApplyEncryptionKeyFromString(EncryptionKey);
		}
	}

#if WITH_EDITOR
	bool bEnabledInEditor = false;
	GConfig->GetBool(TEXT("Ias"), TEXT("EnableInEditor"), bEnabledInEditor, GEngineIni);

	if (!bEnabledInEditor)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::Disabled) << TEXT("Disabled in Editor");
		return Status;
	}
#endif //WITH_EDITOR

#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("NoIas")))
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::Disabled) << TEXT("Disabled by '-NoIas'");
		return Status;
	}
#endif

	FOnDemandEndpointConfig OutConfig;
#if !UE_BUILD_SHIPPING
	FString UrlParam;
	if (FParse::Value(CommandLine, TEXT("Ias.TocUrl="), UrlParam))
	{
		FStringView UrlView(UrlParam);
		if (UrlView.StartsWith(TEXTVIEW("http://")) && UrlView.EndsWith(TEXTVIEW(".iochunktoc")))
		{
			int32 Delim = INDEX_NONE;
			if (UrlView.RightChop(7).FindChar(TEXT('/'), Delim))
			{
				OutConfig.ServiceUrls.Add(FString(UrlView.Left(7 +  Delim)));
				OutConfig.TocPath = UrlView.RightChop(OutConfig.ServiceUrls[0].Len() + 1);
			}
		}

		if (OutConfig.IsValid() == false)
		{
			FIoStatus Status = FIoStatusBuilder(EIoErrorCode::InvalidParameter) << TEXT("Failed to parse '-Ias.TocUrl='");
			return Status;
		}

		return OutConfig;
	}
#endif

	const FString ConfigFileName = TEXT("IoStoreOnDemand.ini");
	const FString ConfigPath = FPaths::Combine(TEXT("Cloud"), ConfigFileName);

	if (FPlatformMisc::FileExistsInPlatformPackage(ConfigPath) == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::NotFound)
			<< TEXT("Failed to find config file '")
			<< ConfigPath
			<< TEXT("'");
		return Status;
	}

	const FString ConfigContent = FPlatformMisc::LoadTextFileFromPlatformPackage(ConfigPath);
	if (TryParseConfigContent(ConfigContent, ConfigFileName, OutConfig) == false || OutConfig.IsValid() == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::ReadError)
			<< TEXT("Failed to read config file '")
			<< ConfigPath
			<< TEXT("'");
		return Status;
	}

	{
		TStringBuilder<256> TocFilePath;
		FPathViews::Append(TocFilePath, TEXT("Cloud"), FPaths::GetBaseFilename(OutConfig.TocPath));
		TocFilePath.Append(TEXT(".iochunktoc"));

		if (FPlatformMisc::FileExistsInPlatformPackage(*TocFilePath))
		{
			OutConfig.TocFilePath = TocFilePath;
		}
	}

	return OutConfig;
}

///////////////////////////////////////////////////////////////////////////////
static FString GetInstallCacheDirectory(const TCHAR* CommandLine)
{
	FString DirName;

	if (IsRunningDedicatedServer())
	{
		if (!FForkProcessHelper::IsForkRequested())
		{
			DirName = TEXT("InstallCacheServer");
		}
		else
		{
			if (!FForkProcessHelper::IsForkedChildProcess())
			{
				UE_LOG(LogIoStoreOnDemand, Fatal, TEXT("Attempting to create install cache before forking!"));
			}

			FString CommandLineDir;
			bool bUsePathFromCommandLine = FParse::Value(CommandLine, TEXT("ServerIOInstallCacheDir="), CommandLineDir);
			if (bUsePathFromCommandLine)
			{
				if (!FPaths::ValidatePath(CommandLineDir))
				{
					bUsePathFromCommandLine = false;
					UE_LOG(LogIoStoreOnDemand, Error, TEXT("Invalid ServerIOInstallCacheDir from command line: %s"), *CommandLineDir);
				}
				else if (!FPaths::IsRelative(CommandLineDir))
				{
					bUsePathFromCommandLine = false;
					UE_LOG(LogIoStoreOnDemand, Error, TEXT("ServerIOInstallCacheDir from command line is not relative: %s"), *CommandLineDir);
				}

				if (bUsePathFromCommandLine)
				{
					return FPaths::ProjectPersistentDownloadDir() / CommandLineDir;
				}
			}

			DirName = FString::Printf(TEXT("InstallCacheServer-%u"), FPlatformProcess::GetCurrentProcessId());
		}
	}
#if WITH_EDITOR
	else if (GIsEditor)
	{
		DirName = TEXT("InstallCacheEditor");
	}
#endif //if WITH_EDITOR
	else
	{
		DirName = TEXT("InstallCache");
	}

	return FPaths::ProjectPersistentDownloadDir() / TEXT("IoStore") / DirName;
}

///////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FOnDemandInstallCacheConfig> TryParseInstallCacheConfig(const TCHAR* CommandLine)
{
	bool bUseInstallCache = GIoStoreOnDemandInstallCacheEnabled;
#if !UE_BUILD_SHIPPING
	bUseInstallCache = FParse::Param(CommandLine, TEXT("NoIAD")) == false;
#endif
	if (bUseInstallCache == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::Disabled) << TEXT("Disabled");
		return Status;
	}

	if (FPaths::HasProjectPersistentDownloadDir() == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::Disabled) << TEXT("Persistent storage not configured");
		return Status;
	}

	FOnDemandInstallCacheConfig OutConfig;

	const int64 DiskQuota = [CommandLine]
	{
		if (FString ParamValue; FParse::Value(CommandLine, TEXT("-Iad.FileCache.DiskQuota="), ParamValue))
		{
			return Config::ParseSizeParam(ParamValue);
		}
		else if (FString ValueStr; GConfig->GetString(TEXT("OnDemandInstall"), TEXT("FileCache.DiskQuota"), ValueStr, GEngineIni))
		{
			return Config::ParseSizeParam(ValueStr);
		}
		return int64(-1);
	}();

	if (DiskQuota > 0)
	{
		OutConfig.DiskQuota = DiskQuota;
	}

	const int64 JournalMaxSize = [CommandLine]
	{
		if (FString ParamValue; FParse::Value(CommandLine, TEXT("-Iad.FileCache.JournalMaxSize="), ParamValue))
		{
			return Config::ParseSizeParam(ParamValue);
		}
		else if (FString ValueStr; GConfig->GetString(TEXT("OnDemandInstall"), TEXT("FileCache.JournalMaxSize"), ValueStr, GEngineIni))
		{
			return Config::ParseSizeParam(ValueStr);
		}
		return int64(-1);
	}();

	if (JournalMaxSize > 0)
	{
		OutConfig.JournalMaxSize = JournalMaxSize;
	}

	OutConfig.RootDirectory = Config::GetInstallCacheDirectory(CommandLine);
#if !UE_BUILD_SHIPPING
	OutConfig.bDropCache = FParse::Param(CommandLine, TEXT("Iad.DropCache"));
#endif

	return OutConfig;
}

} // namespace UE::IoStore

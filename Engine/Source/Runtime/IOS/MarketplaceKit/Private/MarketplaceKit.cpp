// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarketplaceKit.h"

#include "CoreGlobals.h"
#include "MarketplaceKitWrapper.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogMarketplaceKit, Log, All);

const TCHAR* LexToString(EMarketplaceType Value)
{
	switch (Value)
	{
		case EMarketplaceType::AppStore: return TEXT("AppStore");
		case EMarketplaceType::TestFlight: return TEXT("TestFlight");
		case EMarketplaceType::Marketplace: return TEXT("Marketplace");
		case EMarketplaceType::Web: return TEXT("Web");
		case EMarketplaceType::Other: return TEXT("Other");
		default: checkNoEntry(); [[fallthrough]];
		case EMarketplaceType::NotAvailable: return TEXT("NotAvailable");
	}
}

void LexFromString(EMarketplaceType& OutValue, const TCHAR* InValue)
{
	if (FCString::Stricmp(InValue, TEXT("AppStore")) == 0)
	{
		OutValue = EMarketplaceType::AppStore;
	}
	else if (FCString::Stricmp(InValue, TEXT("TestFlight")) == 0)
	{
		OutValue = EMarketplaceType::TestFlight;
	}
	else if (FCString::Stricmp(InValue, TEXT("Marketplace")) == 0)
	{
		OutValue = EMarketplaceType::Marketplace;
	}
	else if (FCString::Stricmp(InValue, TEXT("Web")) == 0)
	{
		OutValue = EMarketplaceType::Web;
	}
	else if (FCString::Stricmp(InValue, TEXT("Other")) == 0)
	{
		OutValue = EMarketplaceType::Other;
	}
	else if (FCString::Stricmp(InValue, TEXT("NotAvailable")) == 0)
	{
		OutValue = EMarketplaceType::NotAvailable;
	}
	else
	{
		checkNoEntry();
		OutValue = EMarketplaceType::NotAvailable;
	}
}

void FMarketplaceKitModule::StartupModule()
{
	FString TestFlightMarketplaceTypeString;
	GConfig->GetString(TEXT("MarketplaceKit"), TEXT("TestFlightMarketplaceType"), TestFlightMarketplaceTypeString, GEngineIni);
	FParse::Value(FCommandLine::Get(), TEXT("TestFlightMarketplaceType="), TestFlightMarketplaceTypeString);
	if (!TestFlightMarketplaceTypeString.IsEmpty())
	{
		LexFromString(TestFlightMarketplaceType, *TestFlightMarketplaceTypeString);
	}

	GConfig->GetString(TEXT("MarketplaceKit"), TEXT("TestFlightMarketplaceBundleId"), TestFlightMarketplaceBundleId, GEngineIni);
	FParse::Value(FCommandLine::Get(), TEXT("TestFlightMarketplaceBundleId="), TestFlightMarketplaceBundleId);

	CacheValue();
}

void FMarketplaceKitModule::ShutdownModule()
{
}

bool FMarketplaceKitModule::SupportsDynamicReloading()
{
	return true;
}

static constexpr EMarketplaceType ConvertMarketplaceType(const AppDistributorType Type)
{
	switch (Type)
	{
		case AppDistributorTypeAppStore:		return EMarketplaceType::AppStore;
		case AppDistributorTypeTestFlight:		return EMarketplaceType::TestFlight;
		case AppDistributorTypeMarketplace:		return EMarketplaceType::Marketplace;
		case AppDistributorTypeWeb:				return EMarketplaceType::Web;
		case AppDistributorTypeOther:			return EMarketplaceType::Other;
		case AppDistributorTypeNotAvailable:	return EMarketplaceType::AppStore; // Pre 17.4, hardcode to AppStore
		default:								return EMarketplaceType::Other;
	}
}

void FMarketplaceKitModule::GetCurrentTypeAsync(TFunction<void(EMarketplaceType Type, const FString& Name)> Callback)
{
	[AppDistributorWrapper getCurrentWithCompletionHandler:^(enum AppDistributorType Type, NSString* _Nonnull Name)
	{
		{
			const EMarketplaceType ConvertedType = ConvertMarketplaceType(Type);
			const FString ConvertedName(Name);

			UE_LOG(LogMarketplaceKit, Log, TEXT("AppDistributorWrapper getCurrentWithCompletionHandler %s %s"), LexToString(ConvertedType), *ConvertedName);

			CachedType = ConvertedType;
			CachedName = ConvertedName;
			bCachedTypeValid = true;
		}

		{
			EMarketplaceType EffectiveType;
			FString EffectiveName;
			GetEffectiveType(EffectiveType, EffectiveName);
			Callback(EffectiveType, EffectiveName);
		}
	}];
}

void FMarketplaceKitModule::GetCurrentType(EMarketplaceType& OutType, FString& OutName)
{
	CacheValue();
	GetEffectiveType(OutType, OutName);
}

EMarketplaceType FMarketplaceKitModule::GetCurrentTypeStatic()
{
	EMarketplaceType Result = EMarketplaceType::NotAvailable;
	if (FMarketplaceKitModule* MarketplaceKitModule = FModuleManager::LoadModulePtr<FMarketplaceKitModule>(TEXT("MarketplaceKit")))
	{
		FString Unused;
		MarketplaceKitModule->GetCurrentType(Result, Unused);
	}
	return Result;
}

FString FMarketplaceKitModule::GetCurrentTypeAsString()
{
	CacheValue();
	
	TStringBuilder<256> Result;

	EMarketplaceType EffectiveType;
	FString EffectiveName;
	GetEffectiveType(EffectiveType, EffectiveName);

	switch (EffectiveType)
	{
		case EMarketplaceType::AppStore:		Result.Append(TEXT("AppStore")); break;
		case EMarketplaceType::TestFlight:		Result.Append(TEXT("TestFlight")); break;
		case EMarketplaceType::Marketplace:		Result.Append(TEXT("Marketplace")); break;
		case EMarketplaceType::Web:				Result.Append(TEXT("Web")); break;
		case EMarketplaceType::NotAvailable:	Result.Append(TEXT("NotAvailable")); break;
		case EMarketplaceType::Other:			[[fallthrough]];
		default:								Result.Append(TEXT("Other")); break;
	}
	
	if (EffectiveType == EMarketplaceType::Marketplace && !EffectiveName.IsEmpty())
	{
		Result.Append(TEXT("-"));
		Result.Append(MoveTemp(EffectiveName));
	}

	return *Result;
}

FString FMarketplaceKitModule::GetCurrentTypeAsStringStatic()
{
	if (FMarketplaceKitModule* MarketplaceKitModule = FModuleManager::LoadModulePtr<FMarketplaceKitModule>(TEXT("MarketplaceKit")))
	{
		return MarketplaceKitModule->GetCurrentTypeAsString();
	}
	return FString();
}

FString FMarketplaceKitModule::GetMarketplaceBundleIdStatic()
{
	if (FMarketplaceKitModule* MarketplaceKitModule = FModuleManager::LoadModulePtr<FMarketplaceKitModule>(TEXT("MarketplaceKit")))
	{
		EMarketplaceType Type;
		FString Name;
		MarketplaceKitModule->GetCurrentType(Type, Name);
		if (Type == EMarketplaceType::Marketplace)
		{
			return Name;
		}
	}
	return FString();
}

bool FMarketplaceKitModule::IsTestFlight()
{
	CacheValue();
	return CachedType == EMarketplaceType::TestFlight;
}

bool FMarketplaceKitModule::IsTestFlightStatic()
{
	if (FMarketplaceKitModule* MarketplaceKitModule = FModuleManager::LoadModulePtr<FMarketplaceKitModule>(TEXT("MarketplaceKit")))
	{
		return MarketplaceKitModule->IsTestFlight();
	}
	return false;
}

void FMarketplaceKitModule::CacheValue()
{
	if (bCachedTypeValid)
	{
		return;
	}

	// TODO avoid scheduling multiple requests in case this path is hit from multiple threads

	dispatch_semaphore_t Semaphore = dispatch_semaphore_create(0);

	[AppDistributorWrapper getCurrentWithCompletionHandler:^(enum AppDistributorType Type, NSString* _Nonnull Name)
	{
		CachedType = ConvertMarketplaceType(Type);
		CachedName = Name;
		bCachedTypeValid = true;

		UE_LOG(LogMarketplaceKit, Log, TEXT("AppDistributorWrapper getCurrentWithCompletionHandler %s %s"), LexToString(CachedType), *CachedName);

		dispatch_semaphore_signal(Semaphore);
	}];

	// wait for a result, but timeout after 1s
	dispatch_semaphore_wait(Semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(Semaphore);
}

void FMarketplaceKitModule::GetEffectiveType(EMarketplaceType& OutType, FString& OutName)
{
	if (CachedType == EMarketplaceType::TestFlight
		// Other == run from XCode
		|| CachedType == EMarketplaceType::Other)
	{
		OutType = TestFlightMarketplaceType;
		OutName = TestFlightMarketplaceBundleId;
		return;
	}
	OutType = CachedType;
	OutName = CachedName;
}

IMPLEMENT_MODULE(FMarketplaceKitModule, MarketplaceKit);

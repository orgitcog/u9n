// Copyright Epic Games, Inc. All Rights Reserved.

#include "Integration.h"
#include "Misc/OutputDeviceRedirector.h"
#include "FMemoryResource.h"
#include "HAL/Platform.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "MetaHumanCoreStyle.h"
#include "MetaHumanSupportedRHI.h"
#include "MetaHumanCoreLog.h"
#include "MetaHumanIdentityCustomVersion.h"
#include "UObject/PerPlatformProperties.h"

#include "Misc/Guid.h"
#include "Serialization/CustomVersion.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <dlfcn.h>
#endif

#define LOCTEXT_NAMESPACE "MetaHumanCore"

DEFINE_LOG_CATEGORY_STATIC(CoreLib, Log, All);

// Register the custom version with core
const FGuid FMetaHumanIdentityCustomVersion::GUID{ 0x3AC07C3C, 0x19FF4731, 0x98C5432B, 0x43B241B5 };
FCustomVersionRegistration GRegisterMetaHumanIdentityCustomVersion(FMetaHumanIdentityCustomVersion::GUID, FMetaHumanIdentityCustomVersion::LatestVersion, TEXT("MetaHumanIdentityCustomVersion"));


class FMetaHumanCoreModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		// Register the style
		FMetaHumanCoreStyle::Register();

		// Add exemption to FName::NameToDisplayString formatting to ensure "MetaHuman" is displayed without a space
		FName::AddNameToDisplayStringExemption(TEXT("MetaHuman"));
		
		FString Run;
		FParse::Value(FCommandLine::Get(), TEXT("run="), Run);

		// only flag a warning if we are not running packaging or cooking, as there is no RHI present in this case
		if (Run != TEXT("cook") && Run != TEXT("package")&& !IsRunningCommandlet() && !FMetaHumanSupportedRHI::IsSupported())
		{
			UE_LOG(LogMetaHumanCore, Warning, TEXT("Unsupported RHI. Set RHI to %s"), *FMetaHumanSupportedRHI::GetSupportedRHINames().ToString());
		}
	}

	virtual void ShutdownModule() override
	{
		FMetaHumanCoreStyle::Unregister();
		MemoryResource.Reset();
	}

private:
	TUniquePtr<class FMemoryResource> MemoryResource;
};

IMPLEMENT_MODULE(FMetaHumanCoreModule, MetaHumanCore)

#undef LOCTEXT_NAMESPACE

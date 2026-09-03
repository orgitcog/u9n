// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMetaHumanCoreTechLib.h"

#include "Misc/OutputDeviceRedirector.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

#include "carbon/common/External.h"
#include "carbon/common/Logger.h"
#include "Logging/LogMacros.h"
#include "HAL/UnrealMemory.h"
#include "FMemoryResource.h"

// don't display verbose log messages from the core tech
DEFINE_LOG_CATEGORY_STATIC(Autorigger, Log, Log);


void AutoriggerLogFunction(TITAN_NAMESPACE::LogLevel logLevel, const char* format, ...)
{
	using namespace TITAN_NAMESPACE;

#if !NO_LOGGING
	va_list(Args);
	va_start(Args, format);
	char Buffer[1024];
#ifdef _MSC_VER
	vsnprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, format, Args);
#else
	vsnprintf(Buffer, sizeof(Buffer), format, Args);
#endif
	va_end(Args);

	FString MessageW(Buffer);

	switch (logLevel)
	{
	case LogLevel::DEBUG: break; // we won't display Debugging messages from titan
	case LogLevel::INFO: break; // we won't display Info messages from titan
	case LogLevel::ERR: UE_LOG(Autorigger, Error, TEXT("%s"), *MessageW); break;
	case LogLevel::FATAL: UE_LOG(Autorigger, Error, TEXT("%s"), *MessageW); break;
	case LogLevel::CRITICAL: UE_LOG(Autorigger, Error, TEXT("%s"), *MessageW); break;
	case LogLevel::WARNING: UE_LOG(Autorigger, Warning, TEXT("%s"), *MessageW); break;
	case LogLevel::VERBOSE: break; // we won't display Verbose messages from titan
	default: UE_LOG(Autorigger, Error, TEXT("%s"), *MessageW);
	}

#endif
}

class FMetaHumanCoreTechLib : public IMetaHumanCoreTechLib
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		MemoryResource = MakeUnique<FMemoryResource>();
		TITAN_NAMESPACE::GetIntegrationParams() = { TITAN_NAMESPACE::Logger(AutoriggerLogFunction), MemoryResource.Get() };
	}

	virtual void ShutdownModule() override
	{
		MemoryResource.Reset();
	}

private:
	TUniquePtr<class FMemoryResource> MemoryResource = nullptr;
};

IMPLEMENT_MODULE( FMetaHumanCoreTechLib, MetaHumanCoreTechLib )


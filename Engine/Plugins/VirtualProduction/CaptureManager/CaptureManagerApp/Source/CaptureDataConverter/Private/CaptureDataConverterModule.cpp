// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureDataConverterModule.h"

#include "HAL/PlatformProcess.h"

#include "Settings/CaptureManagerSettings.h"

#include "Nodes/ThirdPartyEncoder/CaptureThirdPartyNodeParams.h"

namespace UE::CaptureManager::Private
{

bool ThirdPartyAvailabilityCheck(const FString& InThirdPartyEncoder)
{
	return FPlatformProcess::ExecProcess(*InThirdPartyEncoder, TEXT("-version"), nullptr, nullptr, nullptr);
}

}

bool FCaptureDataConverterModule::IsThirdPartyEncoderAvailable()
{
	return CheckThirdPartyEncoderAvailability();
}

FString FCaptureDataConverterModule::GetThirdPartyEncoder() const
{
	const UCaptureManagerSettings* Settings = GetDefault<UCaptureManagerSettings>();

	if (!Settings->bEnableThirdPartyEncoder)
	{
		return FString();
	}

	return Settings->ThirdPartyEncoder.FilePath;
}

FString FCaptureDataConverterModule::GetThirdPartyEncoderVideoCommandArguments() const
{
	const UCaptureManagerSettings* Settings = GetDefault<UCaptureManagerSettings>();

	if (!Settings->bEnableThirdPartyEncoder)
	{
		return FString();
	}

	return Settings->CustomVideoCommandArguments;
}

FString FCaptureDataConverterModule::GetThirdPartyEncoderAudioCommandArguments() const
{
	const UCaptureManagerSettings* Settings = GetDefault<UCaptureManagerSettings>();

	if (!Settings->bEnableThirdPartyEncoder)
	{
		return FString();
	}

	return Settings->CustomAudioCommandArguments;
}

void FCaptureDataConverterModule::StartupModule()
{
	UCaptureManagerSettings* Settings = GetMutableDefault<UCaptureManagerSettings>();

	if (Settings->CustomAudioCommandArguments.IsEmpty())
	{
		Settings->CustomVideoCommandArguments = UE::CaptureManager::VideoCommandArgumentTemplate;
	}
	
	if (Settings->CustomAudioCommandArguments.IsEmpty())
	{
		Settings->CustomAudioCommandArguments = UE::CaptureManager::AudioCommandArgumentTemplate;
	}
}

void FCaptureDataConverterModule::ShutdownModule()
{
}

bool FCaptureDataConverterModule::CheckThirdPartyEncoderAvailability() const
{
	const UCaptureManagerSettings* Settings = GetDefault<UCaptureManagerSettings>();

	if (Settings->bEnableThirdPartyEncoder && !Settings->ThirdPartyEncoder.FilePath.IsEmpty())
	{
		return UE::CaptureManager::Private::ThirdPartyAvailabilityCheck(Settings->ThirdPartyEncoder.FilePath);
	}

	return false;
}

IMPLEMENT_MODULE(FCaptureDataConverterModule, CaptureDataConverter)
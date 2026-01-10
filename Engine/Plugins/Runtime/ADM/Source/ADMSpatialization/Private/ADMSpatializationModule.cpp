// Copyright Epic Games, Inc. All Rights Reserved.

#include "ADMSpatializationModule.h"

#include "ADMSpatializationSettings.h"
#include "ADMSpatializationLog.h"

DEFINE_LOG_CATEGORY(LogADMSpatialization);


namespace UE::ADM::Spatialization
{
	void FModule::StartupModule()
	{
		IModularFeatures::Get().RegisterModularFeature(IAudioSpatializationFactory::GetModularFeatureName(), &SpatializationFactory);

		if (const UADMSpatializationSettings* Settings = GetDefault<UADMSpatializationSettings>())
		{
			FIPv4Address IPAddress;
			if (FIPv4Address::Parse(Settings->IPAddress, IPAddress))
			{
				SpatializationFactory.SetSendIPEndpoint(FIPv4Endpoint(IPAddress, Settings->IPPort));
			}
			else
			{
				UE_LOG(LogADMSpatialization, Display, TEXT("Failed to parse specified default ADM Spatialization client endpoint in developer settings. Default IP not set for ADM Spatialization."));
			}
		}
		else
		{
			UE_LOG(LogADMSpatialization, Error, TEXT("Failed to find ADM Spatialization Developer Settings. Default IP not set for ADM Spatialization."));
		}

		// Enable aggregate devices so we can support large channel counts
		constexpr bool bEnable = true;
		FAudioDeviceManager::EnableAggregateDeviceSupport(bEnable);
	}

	void FModule::ShutdownModule()
	{
		IModularFeatures::Get().UnregisterModularFeature(IAudioSpatializationFactory::GetModularFeatureName(), &SpatializationFactory);
	};

	FADMSpatializationFactory& FModule::GetFactory()
	{
		return SpatializationFactory;
	}
} // namespace UE::ADM::Spatialization

IMPLEMENT_MODULE(UE::ADM::Spatialization::FModule, ADMSpatialization)

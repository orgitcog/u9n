// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesEpicCommon.h"

#include "Online/OnlineServicesEpicCommonPlatformFactory.h"
#include "Online/OnlineServicesLog.h"

namespace UE::Online {

#define UE_ONLINE_EOS_KEY_NAME_FAST_TICK_LOCK TEXT("FastTickLock")

struct FOnlineServicesEpicCommonConfig
{
	bool bEnableAsyncOpFastTick = true;
};

namespace Meta {
	BEGIN_ONLINE_STRUCT_META(FOnlineServicesEpicCommonConfig)
	ONLINE_STRUCT_FIELD(FOnlineServicesEpicCommonConfig, bEnableAsyncOpFastTick)
	END_ONLINE_STRUCT_META()
}

FOnlineServicesEpicCommon::FOnlineServicesEpicCommon(const FString& InServiceConfigName, FName InInstanceName, FName InInstanceConfigName)
	: Super(InServiceConfigName, InInstanceName, InInstanceConfigName)
{
}

bool FOnlineServicesEpicCommon::PreInit()
{
	FOnlineServicesEpicCommonPlatformFactory& PlatformFactory = FOnlineServicesEpicCommonPlatformFactory::Get();
	EOSPlatformHandle = PlatformFactory.CreatePlatform(InstanceName, InstanceConfigName);
	
	if (!EOSPlatformHandle)
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("[%hs] InstanceName=%s InstanceConfigName=%s EOSPlatformHandle=nullptr."), __FUNCTION__, *InstanceName.ToString(), *InstanceConfigName.ToString());
		return false;
	}

	return true;
}

void FOnlineServicesEpicCommon::UpdateConfig()
{
	Super::UpdateConfig();

	FOnlineServicesEpicCommonConfig Config;
	LoadConfig(Config);
	bEnableAsyncOpFastTick = Config.bEnableAsyncOpFastTick;
}

void FOnlineServicesEpicCommon::AddEOSSDKFastTick(FOnlineAsyncOp& InAsyncOp)
{
	if (bEnableAsyncOpFastTick)
	{
		if (EOSPlatformHandle)
		{
			TSharedPtr<IEOSFastTickLock> FastTickLock = EOSPlatformHandle->GetFastTickLock();
			InAsyncOp.Data.Set<TSharedPtr<IEOSFastTickLock>>(UE_ONLINE_EOS_KEY_NAME_FAST_TICK_LOCK, FastTickLock);
		}
	}
}

void FOnlineServicesEpicCommon::RemoveEOSSDKFastTick(FOnlineAsyncOp& InAsyncOp)
{
	if (TSharedPtr<IEOSFastTickLock>* FastTickLock = InAsyncOp.Data.Get<TSharedPtr<IEOSFastTickLock>>(UE_ONLINE_EOS_KEY_NAME_FAST_TICK_LOCK))
	{
		// Simply reset the shared pointer. A null shared pointer may remain on the async op.
		FastTickLock->Reset();
	}
}

void FOnlineServicesEpicCommon::WarnIfEncryptionKeyMissing(const FString& InterfaceName) const
{
	if (IEOSSDKManager* Manager = IEOSSDKManager::Get())
	{
		const FString& PlatformConfigName = GetEOSPlatformHandle()->GetConfigName();
		if (const FEOSSDKPlatformConfig* Config = Manager->GetPlatformConfig(PlatformConfigName))
		{
			const FString& EncryptionKey = Config->EncryptionKey;
			if (EncryptionKey.IsEmpty())
			{
				UE_LOG(LogOnlineServices, Verbose, TEXT("%s interface not available due to missing ClientEncryptionKey in config."), *InterfaceName);
			}
			else
			{
				// If we have an encryption key and still can't get the interface, something weird is going on.
				UE_LOG(LogOnlineServices, Warning, TEXT("%s interface not available despite encryption key being present."), *InterfaceName);
			}
		}
	}
}

void FOnlineServicesEpicCommon::FlushTick(float DeltaSeconds)
{
	Super::FlushTick(DeltaSeconds);

	GetEOSPlatformHandle()->Tick();
}

/* UE::Online */ }

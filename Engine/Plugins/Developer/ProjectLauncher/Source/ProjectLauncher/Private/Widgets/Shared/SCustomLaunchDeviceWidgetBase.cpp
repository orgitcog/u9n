// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Shared/SCustomLaunchDeviceWidgetBase.h"

#include "ITargetDeviceProxy.h"
#include "ITargetDeviceProxyManager.h"
#include "ITargetDeviceServicesModule.h"
#include "PlatformInfo.h"
#include "Modules/ModuleManager.h"

void SCustomLaunchDeviceWidgetBase::Construct()
{
	const TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = GetDeviceProxyManager();
	DeviceProxyManager->OnProxyAdded().AddSP(this, &SCustomLaunchDeviceWidgetBase::OnDeviceProxyAdded);
	DeviceProxyManager->OnProxyRemoved().AddSP(this, &SCustomLaunchDeviceWidgetBase::OnDeviceProxyRemoved);

	RefreshDeviceList();
}


SCustomLaunchDeviceWidgetBase::~SCustomLaunchDeviceWidgetBase()
{
	const TSharedRef<ITargetDeviceProxyManager>& DeviceProxyManager = GetDeviceProxyManager();
	DeviceProxyManager->OnProxyAdded().RemoveAll(this);
	DeviceProxyManager->OnProxyRemoved().RemoveAll(this);
}


void SCustomLaunchDeviceWidgetBase::OnDeviceProxyAdded(const TSharedRef<ITargetDeviceProxy>& DeviceProxy)
{
	RefreshDeviceList();
}



void SCustomLaunchDeviceWidgetBase::OnDeviceProxyRemoved(const TSharedRef<ITargetDeviceProxy>& DeviceProxy)
{
	FString DeviceID = DeviceProxy->GetTargetDeviceId(NAME_None);
	OnDeviceRemoved.ExecuteIfBound(DeviceID);

	RefreshDeviceList();
}



void SCustomLaunchDeviceWidgetBase::RefreshDeviceList()
{
	DeviceProxyList.Reset();

	if (bAllPlatforms)
	{
		for (const PlatformInfo::FTargetPlatformInfo* PlatformInfo : PlatformInfo::GetVanillaPlatformInfoArray())
		{
			TArray<TSharedPtr<ITargetDeviceProxy>> PlatformDeviceProxyList;
			GetDeviceProxyManager()->GetProxies(PlatformInfo->Name, false, PlatformDeviceProxyList);

			for (TSharedPtr<ITargetDeviceProxy> DeviceProxy : PlatformDeviceProxyList)
			{
				DeviceProxyList.AddUnique(DeviceProxy);
			}
		}
	}
	else
	{
		for (const FString& Platform : Platforms.Get())
		{
			const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(Platform));
			if (PlatformInfo != nullptr)
			{
				TArray<TSharedPtr<ITargetDeviceProxy>> PlatformDeviceProxyList;
				GetDeviceProxyManager()->GetProxies(PlatformInfo->Name, false, PlatformDeviceProxyList);

				for (TSharedPtr<ITargetDeviceProxy> DeviceProxy : PlatformDeviceProxyList)
				{
					DeviceProxyList.AddUnique(DeviceProxy);
				}
			}
		}
	}

	OnDeviceListRefreshed();
}


void SCustomLaunchDeviceWidgetBase::OnSelectedPlatformChanged()
{
	// collect all device ids for the new platforms
	TArray<FString> ValidPlatformDevices;
	for (const FString& Platform : Platforms.Get())
	{
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(Platform));
		if (PlatformInfo != nullptr)
		{
			TArray<TSharedPtr<ITargetDeviceProxy>> PlatformDeviceProxyList;
			GetDeviceProxyManager()->GetProxies(PlatformInfo->Name, false, PlatformDeviceProxyList);

			for (TSharedPtr<ITargetDeviceProxy> PlatformDeviceProxy : PlatformDeviceProxyList)
			{
				ValidPlatformDevices.Add(PlatformDeviceProxy->GetTargetDeviceId(NAME_None));
			}
		}
	}

	// remove any devices that belong to other platforms
	TArray<FString> ValidSelectedDevices = SelectedDevices.Get();
	int32 InvalidCount = ValidSelectedDevices.RemoveAll( [ValidPlatformDevices]( const FString& Device )
	{
		return !ValidPlatformDevices.Contains(Device);
	});

	if (InvalidCount > 0)
	{
		OnSelectionChanged.ExecuteIfBound(ValidSelectedDevices);
	}

	// update the list
	RefreshDeviceList();
}


const TSharedRef<ITargetDeviceProxyManager> SCustomLaunchDeviceWidgetBase::GetDeviceProxyManager() const
{
	ITargetDeviceServicesModule& TargetDeviceServicesModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>("TargetDeviceServices");
	return TargetDeviceServicesModule.GetDeviceProxyManager();
}



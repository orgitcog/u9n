// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkDeviceModule.h"

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"


class FSpawnTabArgs;
class IDetailsView;
class SDockTab;
class SLiveLinkDeviceTable;
class SWidget;


class FLiveLinkDeviceModule : public ILiveLinkDeviceModule
{
public:
	static const FName DevicesTabName;
	static const FName DeviceDetailsTabName;

	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	FOnDeviceSelectionChangedDelegate& OnSelectionChanged() override { return OnDeviceSelectionChangedDelegate; }

private:
	TSharedRef<SWidget> OnGenerateAddDeviceMenu();
	TSharedRef<SDockTab> OnSpawnDevicesTab(const FSpawnTabArgs& InSpawnTabArgs);
	TSharedRef<SDockTab> OnSpawnDeviceDetailsTab(const FSpawnTabArgs& InSpawnTabArgs);

private:
	void DeviceSelectionChanged(ULiveLinkDevice* InSelectedDevice);

	TSharedPtr<SLiveLinkDeviceTable> DeviceTable;
	TSharedPtr<IDetailsView> DetailsView;
	TWeakObjectPtr<ULiveLinkDevice> WeakSelectedDevice;

	FOnDeviceSelectionChangedDelegate OnDeviceSelectionChangedDelegate;
};

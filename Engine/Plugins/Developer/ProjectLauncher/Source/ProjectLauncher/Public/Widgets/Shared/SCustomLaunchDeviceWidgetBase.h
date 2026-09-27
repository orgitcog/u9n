// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ITargetDeviceProxy;
class ITargetDeviceProxyManager;

class PROJECTLAUNCHER_API SCustomLaunchDeviceWidgetBase
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, TArray<FString> );
	DECLARE_DELEGATE_OneParam(FOnDeviceRemoved, FString );

public:
	void Construct();
	~SCustomLaunchDeviceWidgetBase();

	void RefreshDeviceList();
	void OnSelectedPlatformChanged();

protected:
	virtual void OnDeviceListRefreshed() {};

	TAttribute<TArray<FString>> Platforms;
	TAttribute<TArray<FString>> SelectedDevices;
	FOnSelectionChanged OnSelectionChanged;
	FOnDeviceRemoved OnDeviceRemoved;
	bool bAllPlatforms = false;

	void OnDeviceProxyAdded(const TSharedRef<ITargetDeviceProxy>& DeviceProxy);
	void OnDeviceProxyRemoved(const TSharedRef<ITargetDeviceProxy>& DeviceProxy);

	const TSharedRef<ITargetDeviceProxyManager> GetDeviceProxyManager() const;

	TArray<TSharedPtr<ITargetDeviceProxy>> DeviceProxyList;
};

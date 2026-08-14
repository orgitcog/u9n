// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Shared/SCustomLaunchDeviceWidgetBase.h"

template<typename ItemType> class SComboBox;

class PROJECTLAUNCHER_API SCustomLaunchDeviceCombo
	: public SCustomLaunchDeviceWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SCustomLaunchDeviceCombo)
		: _AllPlatforms(false)
		{}
		SLATE_EVENT(FOnDeviceRemoved, OnDeviceRemoved)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged);
		SLATE_ATTRIBUTE(TArray<FString>, SelectedDevices);
		SLATE_ATTRIBUTE(TArray<FString>, Platforms);
		SLATE_ARGUMENT(bool, AllPlatforms)
	SLATE_END_ARGS()

public:
	void Construct(	const FArguments& InArgs );

protected:

	TSharedRef<SWidget> GenerateDeviceProxyListWidget(TSharedPtr<ITargetDeviceProxy> DeviceProxy) const;
	void OnDeviceProxySelectionChangedChanged(TSharedPtr<ITargetDeviceProxy> DeviceProxy, ESelectInfo::Type InSelectInfo);
	const FSlateBrush* GetSelectedDeviceProxyBrush() const;
	FText GetSelectedDeviceProxyName() const;

private:
	TSharedPtr<SComboBox<TSharedPtr<ITargetDeviceProxy>> > DeviceProxyComboBox;

};

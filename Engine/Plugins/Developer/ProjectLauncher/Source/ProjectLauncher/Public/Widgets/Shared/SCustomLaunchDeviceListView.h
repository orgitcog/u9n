// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Shared/SCustomLaunchDeviceWidgetBase.h"

class ITableRow;
class STableViewBase;
template <typename ItemType> class SListView;

class PROJECTLAUNCHER_API SCustomLaunchDeviceListView
	: public SCustomLaunchDeviceWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SCustomLaunchDeviceListView)
		: _AllPlatforms(false)
		, _SingleSelect(false)
		{}
		SLATE_EVENT(FOnDeviceRemoved, OnDeviceRemoved)
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged);
		SLATE_ATTRIBUTE(TArray<FString>, SelectedDevices);
		SLATE_ATTRIBUTE(TArray<FString>, Platforms);
		SLATE_ARGUMENT(bool, AllPlatforms)
		SLATE_ARGUMENT(bool, SingleSelect)
	SLATE_END_ARGS()

public:
	void Construct(	const FArguments& InArgs );

protected:
	bool bSingleSelect;

	TSharedRef<ITableRow> GenerateDeviceProxyRow(TSharedPtr<ITargetDeviceProxy> DeviceProxy, const TSharedRef<STableViewBase>& OwnerTable);
	ECheckBoxState IsDeviceProxyChecked(TSharedPtr<ITargetDeviceProxy> DeviceProxy) const;
	void OnDeviceProxyCheckStateChanged(ECheckBoxState NewState, TSharedPtr<ITargetDeviceProxy> DeviceProxy);
	virtual void OnDeviceListRefreshed() override;

private:
	TSharedPtr<SListView<TSharedPtr<ITargetDeviceProxy>> > DeviceProxyListView;

};

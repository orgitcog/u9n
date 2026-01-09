// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Shared/SCustomLaunchDeviceListView.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/STableRow.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/ProjectLauncherStyle.h"
#include "ITargetDeviceProxy.h"
#include "PlatformInfo.h"

#define LOCTEXT_NAMESPACE "SCustomLaunchDeviceListView"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCustomLaunchDeviceListView::Construct(const FArguments& InArgs)
{
	OnDeviceRemoved = InArgs._OnDeviceRemoved;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	SelectedDevices = InArgs._SelectedDevices;
	Platforms = InArgs._Platforms;
	bAllPlatforms = InArgs._AllPlatforms;
	bSingleSelect = InArgs._SingleSelect;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(2)
		.BorderImage(FAppStyle::GetBrush("Brushes.Background"))
		[
			SAssignNew(DeviceProxyListView, SListView<TSharedPtr<ITargetDeviceProxy>>)
			.ListItemsSource(&DeviceProxyList)
			.OnGenerateRow(this, &SCustomLaunchDeviceListView::GenerateDeviceProxyRow)
		]
	];

	SCustomLaunchDeviceWidgetBase::Construct();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION




BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<ITableRow> SCustomLaunchDeviceListView::GenerateDeviceProxyRow(TSharedPtr<ITargetDeviceProxy> DeviceProxy, const TSharedRef<STableViewBase>& OwnerTable)
{
	const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(*DeviceProxy->GetTargetPlatformName(NAME_None));

	TSharedPtr<SWidget> DeviceSelectorCheckbox = SNew(SCheckBox)
	.IsChecked(this, &SCustomLaunchDeviceListView::IsDeviceProxyChecked, DeviceProxy)
	.OnCheckStateChanged(this, &SCustomLaunchDeviceListView::OnDeviceProxyCheckStateChanged, DeviceProxy)
	.Style(FAppStyle::Get(), bSingleSelect ? "RadioButton" : "Checkbox")
	[
		SNew(SHorizontalBox)

		// platform icon
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4,0)
		[
			SNew(SImage)
			.DesiredSizeOverride(FVector2D(16,16))
			.Image(PlatformInfo ? FAppStyle::GetBrush(PlatformInfo->GetIconStyleName(EPlatformIconSize::Normal)) : FStyleDefaults::GetNoBrush())
		]

		// device name
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4,0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(*DeviceProxy->GetName()))
		]
	];

	return SNew(STableRow<TSharedPtr<ITargetDeviceProxy>>, OwnerTable)
	.Padding(FMargin(4,1))
	[
		DeviceSelectorCheckbox.ToSharedRef()
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


ECheckBoxState SCustomLaunchDeviceListView::IsDeviceProxyChecked(TSharedPtr<ITargetDeviceProxy> DeviceProxy) const
{
	FString DeviceID = DeviceProxy->GetTargetDeviceId(NAME_None);

	if (SelectedDevices.Get().Contains(DeviceID))
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}



void SCustomLaunchDeviceListView::OnDeviceProxyCheckStateChanged(ECheckBoxState NewState, TSharedPtr<ITargetDeviceProxy> DeviceProxy)
{
	FString DeviceID = DeviceProxy->GetTargetDeviceId(NAME_None);

	TArray<FString> Devices;
	if (!bSingleSelect)
	{
		Devices = SelectedDevices.Get();
	}
	else if (NewState != ECheckBoxState::Checked)
	{
		// do not allow the current item to be deselected in single-select mode
		return;
	}

	if (NewState == ECheckBoxState::Checked)
	{
		Devices.Add(DeviceID);
	}
	else
	{
		Devices.Remove(DeviceID);
	}

	OnSelectionChanged.ExecuteIfBound(Devices);
}


void SCustomLaunchDeviceListView::OnDeviceListRefreshed()
{
	DeviceProxyListView->RequestListRefresh();
}





#undef LOCTEXT_NAMESPACE

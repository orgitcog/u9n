// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Shared/SCustomLaunchDeviceCombo.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/ProjectLauncherStyle.h"
#include "ITargetDeviceProxy.h"
#include "ITargetDeviceProxyManager.h"
#include "PlatformInfo.h"

#define LOCTEXT_NAMESPACE "SCustomLaunchDeviceCombo"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCustomLaunchDeviceCombo::Construct(const FArguments& InArgs)
{
	OnDeviceRemoved = InArgs._OnDeviceRemoved;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	SelectedDevices = InArgs._SelectedDevices;
	Platforms = InArgs._Platforms;
	bAllPlatforms = InArgs._AllPlatforms;

	ChildSlot
	[
		SNew(SHorizontalBox)
		
		+SHorizontalBox::Slot()
		.AutoWidth()		
		[
			SAssignNew(DeviceProxyComboBox, SComboBox<TSharedPtr<ITargetDeviceProxy>>)
			.OptionsSource(&DeviceProxyList)
			.OnGenerateWidget(this, &SCustomLaunchDeviceCombo::GenerateDeviceProxyListWidget)
			.OnSelectionChanged( this, &SCustomLaunchDeviceCombo::OnDeviceProxySelectionChangedChanged )
			[
				SNew(SHorizontalBox)
					
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4,0)
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(16,16))
					.Image(this, &SCustomLaunchDeviceCombo::GetSelectedDeviceProxyBrush)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4,0)
				[
					SNew(STextBlock)
					//.TextStyle( FAppStyle::Get(), "SmallText")
					.Text(this, &SCustomLaunchDeviceCombo::GetSelectedDeviceProxyName)
				]
			]
		]
	];

	SCustomLaunchDeviceWidgetBase::Construct();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION






BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SCustomLaunchDeviceCombo::GenerateDeviceProxyListWidget( TSharedPtr<ITargetDeviceProxy> DeviceProxy ) const
{
	const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(*DeviceProxy->GetTargetPlatformName(NAME_None));

	return SNew(SHorizontalBox)

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
	;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


const FSlateBrush* SCustomLaunchDeviceCombo::GetSelectedDeviceProxyBrush() const
{
	TArray<FString> DeviceIDs = SelectedDevices.Get();

	if (DeviceIDs.Num() == 1)
	{
		FString DeviceID = DeviceIDs[0];
		const TSharedPtr<ITargetDeviceProxy> DeviceProxy = GetDeviceProxyManager()->FindProxyDeviceForTargetDevice(DeviceID);
		if (DeviceProxy.IsValid())
		{
			const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(*DeviceProxy->GetTargetPlatformName(NAME_None));
			if (PlatformInfo != nullptr)
			{
				return FAppStyle::GetBrush(PlatformInfo->GetIconStyleName(EPlatformIconSize::Normal));
			}
		}
	}
	else if (DeviceIDs.Num() > 1)
	{
		return FAppStyle::Get().GetBrush("Icons.WarningWithColor");
	}

	return FStyleDefaults::GetNoBrush();
}

FText SCustomLaunchDeviceCombo::GetSelectedDeviceProxyName() const
{
	TArray<FString> DeviceIDs = SelectedDevices.Get();

	if (DeviceIDs.Num() == 1)
	{
		FString DeviceID = DeviceIDs[0];
		const TSharedPtr<ITargetDeviceProxy> DeviceProxy = GetDeviceProxyManager()->FindProxyDeviceForTargetDevice(DeviceID);
		if (DeviceProxy.IsValid())
		{
			return FText::FromString(*DeviceProxy->GetName());
		}
	}
	else if (DeviceIDs.Num() > 1)
	{
		return LOCTEXT("TooManyDevices", "Multiple devices (unsupported)");
	}

	return LOCTEXT("NoDevice", "(no device)");
}






void SCustomLaunchDeviceCombo::OnDeviceProxySelectionChangedChanged(TSharedPtr<ITargetDeviceProxy> DeviceProxy, ESelectInfo::Type InSelectInfo)
{
	FString DeviceID = DeviceProxy->GetTargetDeviceId(NAME_None);

	TArray<FString> DeviceIDs;
	DeviceIDs.Add( DeviceID );

	OnSelectionChanged.ExecuteIfBound(DeviceIDs);
}



#undef LOCTEXT_NAMESPACE

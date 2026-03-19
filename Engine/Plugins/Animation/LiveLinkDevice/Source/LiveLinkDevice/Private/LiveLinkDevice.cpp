// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkDevice.h"
#include "Engine/Engine.h"
#include "JsonObjectConverter.h"
#include "LiveLinkDeviceCapability.h"
#include "LiveLinkDeviceModule.h"
#include "LiveLinkDeviceSubsystem.h"
#include "Logging/StructuredLog.h"
#include "Widgets/SNullWidget.h"



TSharedRef<SWidget> ULiveLinkDevice::GenerateWidgetForColumn(const FName InColumnId, const FLiveLinkDeviceWidgetArguments& InArgs)
{
	// Check if the capability provides a default widget implementation.
	ULiveLinkDeviceSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiveLinkDeviceSubsystem>();
	const TMap<FName, TSubclassOf<ULiveLinkDeviceCapability>>& ColumnIdToCapability = Subsystem->GetTableColumnIdToCapability();
	const TSubclassOf<ULiveLinkDeviceCapability>* MaybeCapabilityClass = ColumnIdToCapability.Find(InColumnId);
	if (ensure(MaybeCapabilityClass))
	{
		ULiveLinkDeviceCapability* CapabilityCDO = MaybeCapabilityClass->GetDefaultObject();
		if (TSharedPtr<SWidget> CapabilityWidget = CapabilityCDO->GenerateWidgetForColumn(InColumnId, InArgs, this))
		{
			return CapabilityWidget.ToSharedRef();
		}
	}

	// Neither your device class nor the capability created a widget for this column.
	ensure(false);
	return SNullWidget::NullWidget;
}


void ULiveLinkDevice::InternalDeviceAdded(const FGuid InDeviceGuid, ULiveLinkDeviceSettings* InSettings)
{
	DeviceGuid = InDeviceGuid;
	Settings = InSettings;

	OnDeviceAdded();
}

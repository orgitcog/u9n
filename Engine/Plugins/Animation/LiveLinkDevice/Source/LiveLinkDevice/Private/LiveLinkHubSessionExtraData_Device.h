// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubSessionExtraData.h"
#include "Templates/SubclassOf.h"
#include "LiveLinkHubSessionExtraData_Device.generated.h"


class ULiveLinkDevice;
class ULiveLinkDeviceSettings;


USTRUCT()
struct FLiveLinkDevicePreset
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid DeviceGuid;

	UPROPERTY()
	TSubclassOf<ULiveLinkDevice> DeviceClass;

	UPROPERTY(Instanced)
	TObjectPtr<ULiveLinkDeviceSettings> DeviceSettings;
};


UCLASS()
class ULiveLinkHubSessionExtraData_Device : public ULiveLinkHubSessionExtraData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FLiveLinkDevicePreset> Devices;
};

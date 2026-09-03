// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HomeAssistantBridge.generated.h"

/**
 * IoT Device
 */
USTRUCT(BlueprintType)
struct FIoTDevice
{
	GENERATED_BODY()

	/** Device ID */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString DeviceID;

	/** Device name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString DeviceName;

	/** Device type (light, sensor, switch, etc.) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString DeviceType;

	/** Device state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString State;

	/** Device attributes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	TMap<FString, FString> Attributes;

	FIoTDevice()
		: State(TEXT("unknown"))
	{
	}
};

/**
 * Home Assistant Bridge - Integration with Home Assistant IoT platform
 */
UCLASS(Blueprintable, BlueprintType)
class MARDUKLAB_API UHomeAssistantBridge : public UObject
{
	GENERATED_BODY()

public:
	UHomeAssistantBridge();

	/** Home Assistant server URL */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString HomeAssistantURL;

	/** Access token */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString AccessToken;

	/** Registered IoT devices */
	UPROPERTY(BlueprintReadOnly, Category = "MardukLab")
	TMap<FString, FIoTDevice> RegisteredDevices;

	/** Initialize Home Assistant bridge */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void Initialize(const FString& InURL, const FString& InAccessToken);

	/** Connect to Home Assistant */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	bool Connect();

	/** Disconnect from Home Assistant */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void Disconnect();

	/** Discover devices */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void DiscoverDevices();

	/** Get device state */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	bool GetDeviceState(const FString& DeviceID, FString& OutState);

	/** Set device state */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void SetDeviceState(const FString& DeviceID, const FString& NewState);

	/** Call Home Assistant service */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void CallService(const FString& Domain, const FString& Service, const FString& EntityID);

	/** Register device */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	bool RegisterDevice(const FIoTDevice& Device);

protected:
	/** Send API request to Home Assistant */
	void SendAPIRequest(const FString& Endpoint, const FString& Method, const FString& Payload);

	/** Process device states */
	void ProcessDeviceStates();

	bool bIsConnected;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HomeAssistantBridge.h"

UHomeAssistantBridge::UHomeAssistantBridge()
	: HomeAssistantURL(TEXT("http://localhost:8123"))
	, bIsConnected(false)
{
}

void UHomeAssistantBridge::Initialize(const FString& InURL, const FString& InAccessToken)
{
	HomeAssistantURL = InURL;
	AccessToken = InAccessToken;
	UE_LOG(LogTemp, Log, TEXT("HomeAssistantBridge: Initialized with server at %s"), *HomeAssistantURL);
}

bool UHomeAssistantBridge::Connect()
{
	bIsConnected = true;
	UE_LOG(LogTemp, Log, TEXT("HomeAssistantBridge: Connected to Home Assistant at %s"), *HomeAssistantURL);
	DiscoverDevices();
	return true;
}

void UHomeAssistantBridge::Disconnect()
{
	bIsConnected = false;
	RegisteredDevices.Empty();
	UE_LOG(LogTemp, Log, TEXT("HomeAssistantBridge: Disconnected from Home Assistant"));
}

void UHomeAssistantBridge::DiscoverDevices()
{
	if (!bIsConnected)
	{
		UE_LOG(LogTemp, Warning, TEXT("HomeAssistantBridge: Not connected, cannot discover devices"));
		return;
	}

	// Simulate device discovery
	FIoTDevice LightDevice;
	LightDevice.DeviceID = TEXT("light.living_room");
	LightDevice.DeviceName = TEXT("Living Room Light");
	LightDevice.DeviceType = TEXT("light");
	LightDevice.State = TEXT("off");
	RegisterDevice(LightDevice);

	FIoTDevice SensorDevice;
	SensorDevice.DeviceID = TEXT("sensor.temperature");
	SensorDevice.DeviceName = TEXT("Temperature Sensor");
	SensorDevice.DeviceType = TEXT("sensor");
	SensorDevice.State = TEXT("22.5");
	RegisterDevice(SensorDevice);

	UE_LOG(LogTemp, Log, TEXT("HomeAssistantBridge: Discovered %d devices"), RegisteredDevices.Num());
}

bool UHomeAssistantBridge::GetDeviceState(const FString& DeviceID, FString& OutState)
{
	FIoTDevice* Device = RegisteredDevices.Find(DeviceID);
	if (Device)
	{
		OutState = Device->State;
		return true;
	}
	return false;
}

void UHomeAssistantBridge::SetDeviceState(const FString& DeviceID, const FString& NewState)
{
	FIoTDevice* Device = RegisteredDevices.Find(DeviceID);
	if (Device)
	{
		Device->State = NewState;
		UE_LOG(LogTemp, Log, TEXT("HomeAssistantBridge: Set device %s state to %s"), *DeviceID, *NewState);
		
		// Send state update to Home Assistant
		FString Payload = FString::Printf(TEXT("{\"entity_id\":\"%s\",\"state\":\"%s\"}"), *DeviceID, *NewState);
		SendAPIRequest(TEXT("/api/states/") + DeviceID, TEXT("POST"), Payload);
	}
}

void UHomeAssistantBridge::CallService(const FString& Domain, const FString& Service, const FString& EntityID)
{
	if (!bIsConnected)
	{
		UE_LOG(LogTemp, Warning, TEXT("HomeAssistantBridge: Not connected, cannot call service"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("HomeAssistantBridge: Calling service %s.%s for entity %s"), 
		*Domain, *Service, *EntityID);

	FString Payload = FString::Printf(TEXT("{\"entity_id\":\"%s\"}"), *EntityID);
	SendAPIRequest(FString::Printf(TEXT("/api/services/%s/%s"), *Domain, *Service), TEXT("POST"), Payload);
}

bool UHomeAssistantBridge::RegisterDevice(const FIoTDevice& Device)
{
	RegisteredDevices.Add(Device.DeviceID, Device);
	UE_LOG(LogTemp, Log, TEXT("HomeAssistantBridge: Registered device %s (%s)"), 
		*Device.DeviceName, *Device.DeviceID);
	return true;
}

void UHomeAssistantBridge::SendAPIRequest(const FString& Endpoint, const FString& Method, const FString& Payload)
{
	UE_LOG(LogTemp, Verbose, TEXT("HomeAssistantBridge: %s request to %s"), *Method, *Endpoint);
	// In production, this would use HTTP module with authentication
}

void UHomeAssistantBridge::ProcessDeviceStates()
{
	// Process device state updates
}

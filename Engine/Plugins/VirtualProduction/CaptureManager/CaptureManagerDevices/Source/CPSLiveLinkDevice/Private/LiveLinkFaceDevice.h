// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkDevice.h"

#include "Ingest/LiveLinkDeviceCapability_Ingest.h"
#include "LiveLinkDeviceCapability_Connection.h"
#include "LiveLinkDeviceCapability_Recording.h"
#include "Misc/Optional.h"

#include "Async/TaskProgress.h"

#include "Protocol/CPSDevice.h"
#include "Customizations/ToggleConnectActionCustomization.h"
#include "Customizations/DeviceIpAddressCustomization.h"

#include "BaseIngestLiveLinkDevice.h"

#include "LiveLinkFaceDevice.generated.h"


UCLASS(BlueprintType)
class ULiveLinkFaceDeviceSettings : public ULiveLinkDeviceSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Face")
	FString DisplayName = TEXT("Live Link Face");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Face")
	FDeviceIpAddress IpAddress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Link Face")
	int32 Port = 14785;

	UPROPERTY(VisibleAnywhere, Category = "Live Link Face")
	FToggleConnectAction ConnectAction;
};


UCLASS(BlueprintType, meta = (DisplayName = "Live Link Face", ToolTip = "Use for ingest from the Live Link Face app and control recording"))
class ULiveLinkFaceDevice final : public UBaseIngestLiveLinkDevice
	, public ILiveLinkDeviceCapability_Connection
	, public ILiveLinkDeviceCapability_Recording
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LiveLinkFaceDeviceSettings")
	const ULiveLinkFaceDeviceSettings* GetSettings() const;

	//~ Begin ULiveLinkDevice interface
	virtual TSubclassOf<ULiveLinkDeviceSettings> GetSettingsClass() const override;
	virtual FText GetDisplayName() const override;
	virtual EDeviceHealth GetDeviceHealth() const override;
	virtual FText GetHealthText() const override;

	virtual void OnDeviceAdded() override;
	virtual void OnDeviceRemoved() override;
	//~ End ULiveLinkDevice interface

private:
	virtual FString GetFullTakePath(UE::CaptureManager::FTakeId InTakeId) const override;

	//~ Begin ULiveLinkDeviceCapability_Ingest interface
	virtual void UpdateTakeList_Implementation(UIngestCapability_UpdateTakeListCallback* InCallback) override;
	virtual void RunDownloadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions) override;
	virtual void RunConvertAndUploadTake(const UIngestCapability_ProcessHandle* InProcessHandle, const UIngestCapability_Options* InIngestOptions) override;
	virtual void CancelIngestProcess_Implementation(const UIngestCapability_ProcessHandle* InProcessHandle) override;
	//~ End ULiveLinkDeviceCapability_Ingest interface

	//~ Begin ILiveLinkDeviceCapability_Connection interface
	virtual ELiveLinkDeviceConnectionStatus GetConnectionStatus_Implementation() const override;
	virtual FString GetHardwareId_Implementation() const override;
	virtual bool SetHardwareId_Implementation(const FString& HardwareID) override;
	virtual bool Connect_Implementation() override;
	virtual bool Disconnect_Implementation() override;
	//~ End ILiveLinkDeviceCapability_Connection interface

	//~ Begin ILiveLinkDeviceCapability_Recording interface
	virtual bool StartRecording_Implementation() override;
	virtual bool StopRecording_Implementation() override;
	virtual bool IsRecording_Implementation() const override;
	//~ End ILiveLinkDeviceCapability_Recording interface

	void HandleConnectionChanged(TSharedPtr<const UE::CaptureManager::FCaptureEvent> InEvent);
	void HandleCPSStateUpdate(TSharedPtr<const UE::CaptureManager::FCaptureEvent> InEvent);
	void HandleCPSEvent(TSharedPtr<const UE::CaptureManager::FCaptureEvent> InEvent);

	void OnExportProgressReport(float InProgress, TStrongObjectPtr<const UIngestCapability_ProcessHandle> InProcessHandle);
	void OnExportFinished(UE::CaptureManager::TProtocolResult<void> InResult,
						  FString InTakeName,
						  TStrongObjectPtr<const UIngestCapability_ProcessHandle> InProcessHandle,
						  TStrongObjectPtr<const UIngestCapability_Options> InIngestOptions);

	TOptional<FTakeMetadata> ParseTake(const FString& InTakeDirectory, const FString& InTakeName);
	FTakeMetadata ParseTakeMetadata(const UE::CaptureManager::FGetTakeMetadataResponse::FTakeObject& InTake);

	void FetchPreIngestFiles(TMap<FString, UE::CaptureManager::FTakeId> InNameToIdMap);

	void RemoveDownloadedTakeData(const UE::CaptureManager::FTakeId InTakeId);

	void ExtractTimecodeIfNotSet(FTakeMetadata& InOutTakeMetadata);

	TSharedPtr<UE::CaptureManager::FCPSDevice> Device;

	mutable FCriticalSection DownloadedTakesMutex;
	TMap<UE::CaptureManager::FTakeId, FString> DownloadedTakes;

	std::atomic_bool bIsRecording = false;
	std::atomic_bool bIsConnecting = false;
};

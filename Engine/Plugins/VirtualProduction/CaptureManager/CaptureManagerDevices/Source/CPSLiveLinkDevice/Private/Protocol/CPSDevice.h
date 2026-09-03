// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Control/ControlMessenger.h"

#include "ExportClient/ExportClient.h"

#include "Async/EventSourceUtils.h"
#include "Misc/Optional.h"

#include "Async/CaptureTimerManager.h"

#include "Ingest/IngestCapability_TakeInformation.h"

namespace UE::CaptureManager
{

struct FConnectionStateChangedEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("ConnectionStateChanged");

	enum class EState
	{
		Unknown = 0,
		Connecting,
		Connected,
		Disconnected
	};

	FConnectionStateChangedEvent(EState InConnectionState);

	EState ConnectionState;
};

struct FCPSEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("CPSEvent");

	FCPSEvent(TSharedPtr<FControlUpdate> InUpdateMessage);

	TSharedPtr<FControlUpdate> UpdateMessage;
};

struct FCPSStateEvent : public FCaptureEvent
{
	inline static const FString Name = TEXT("CPSStateEvent");

	FCPSStateEvent(FGetStateResponse InGetStateResponse);

	FGetStateResponse GetStateResponse;
};

class FCPSDevice :
	public FCaptureEventSource,
	public TSharedFromThis<FCPSDevice>
{
private:

	struct FPrivateToken
	{
		explicit FPrivateToken() = default;
	};

public:

	static TSharedPtr<FCPSDevice> MakeCPSDevice(FString InDeviceIpAddress, uint16 InDevicePort);

	FCPSDevice(FPrivateToken,
			   FString InDeviceIpAddress,
			   uint16 InDevicePort);

	virtual ~FCPSDevice() override;

	void InitiateConnect();
	void Stop();

	bool IsConnected() const;

	TProtocolResult<void> StartRecording(FString SlateName,
										 uint16 TakeNumber,
										 TOptional<FString> Subject = TOptional<FString>(),
										 TOptional<FString> Scenario = TOptional<FString>(),
										 TOptional<TArray<FString>> Tags = TOptional<TArray<FString>>());

	TProtocolResult<void> StopRecording();

	TProtocolResult<TArray<FGetTakeMetadataResponse::FTakeObject>> FetchTakeList();
	TProtocolResult<FGetTakeMetadataResponse::FTakeObject> FetchTake(const FString& InTakeName);

	void AddTakeMetadata(FTakeId InId, FGetTakeMetadataResponse::FTakeObject InTake);
	void RemoveTakeMetadata(FTakeId InId);
	FGetTakeMetadataResponse::FTakeObject GetTake(FTakeId InId);
	FTakeId GetTakeId(const FString& InTakeName);

	void StartExport(FTakeId InTakeId, TUniquePtr<FBaseStream> InStream);
	void CancelExport(FTakeId InTakeId);

	/** Fetches the thumbnail for a single take. */
	void FetchThumbnailForTake(FTakeId InTakeId, TUniquePtr<FBaseStream> InStream);
	/** Fetches the thumbnails for all takes. */
	void FetchThumbnails(TUniquePtr<FBaseStream> InStream);

	/** Fetches the sepcified file for a single take. */
	void FetchFileForTake(FTakeId InTakeId, TUniquePtr<FBaseStream> InStream, const FString& InFileName);
	/** Fetches the specified files for all takes. */
	void FetchFiles(TUniquePtr<FBaseStream> InStream, TArray<FString> InFileNames);

private:

	inline static const float ConnectInterval = 5.0f;

	void InitializeDelegates();

	struct FEmpty
	{
	};

	void ConnectControlClient(FEmpty);

	void RegisterForAllEvents();
	void OnCPSEvent(TSharedPtr<FControlUpdate> InUpdateMessage);

	void StartConnectTimer(float InDelay = 0.0f);
	void OnConnectTick();
	void OnDisconnect(const FString& InCause);

	static TSharedRef<FCaptureTimerManager> GetTimerManager();

	TSharedRef<FCaptureTimerManager> TimerManager;

	FString DeviceIpAddress;
	uint16 DeviceControlPort;

	std::atomic_bool bIsConnected;

	using FConnectionThread = TQueueRunner<FEmpty>;
	TUniquePtr<FConnectionThread> ConnThread;
	FControlMessenger ControlMessenger;
	FCaptureTimerManager::FTimerHandle ConnectTimerHandle;

	TUniquePtr<FExportClient> ExportClient;

	FCriticalSection Mutex;
	TMap<FTakeId, FGetTakeMetadataResponse::FTakeObject> TakeMetadata;

	TMap<FTakeId, FExportClient::FTaskId> IdMap;
};

}
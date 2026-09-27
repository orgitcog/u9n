// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkFaceFootageIngest.h"

#include "Control/ControlMessenger.h"
#include "Utils/LiveLinkFaceConnectionExportStreams.h"

class FLiveLinkFaceConnectionIngest final
    : public FLiveLinkFaceIngestBase
{
public:

    FLiveLinkFaceConnectionIngest(const FString& InDeviceIPAddress,
                                  uint16 InDeviceControlPort,
								  bool bInShouldCompressDepthFiles);

	virtual ~FLiveLinkFaceConnectionIngest() override;
    //~ IFootageRetrievalAPI interface
    virtual void Startup(ETakeIngestMode InMode) override;
    virtual void Shutdown() override;
	virtual void GetTakes(const TArray<TakeId>& InIdList, TPerTakeCallback<void> InCallback) override;
	virtual void RefreshTakeListAsync(TCallback<void> InCallback) override;

    //~ FLiveLinkFaceIngestBase interface
    const FString& GetTakesOriginDirectory() const override;

    virtual bool IsProcessing() const override;
	virtual void CancelProcessing(const TArray<TakeId>& InIdList) override;

private:
	struct FCommsRequestParams {
		bool bForceFetchingTakeList = false;
		TCallback<void> ClientCallback = TCallback<void>{};
	};

	static constexpr int32 ConnectInterval = 5; // Seconds

	using FFilesMap = TMap<FString, TArray<FGetTakeMetadataResponse::FFileObject>>;
	using FExportMap = TMap<uint32, FString>; // Take name

	void OnProgressReport(const FString& InTakeName, float InProgress);
	void OnExportFinished(const FString& InTakeName, TProtocolResult<void> InResult);

	void OnControlClientDisconnected(const FString& InCause);

    FLiveLinkFaceTakeMetadata CreateTakeMetadata(const FGetTakeMetadataResponse::FTakeObject& InTake);
    FLiveLinkFaceVideoMetadata CreateVideoMetadata(const FGetTakeMetadataResponse::FVideoObject& InVideo);
    FLiveLinkFaceAudioMetadata CreateAudioMetadata(const FGetTakeMetadataResponse::FAudioObject& InAudio);

	void CancelAllExports();
	void CancelCleanup(const FString& InTakeName);

	void StartConnectTimer(bool bInInvokeDelay = false);
	void StopConnectTimer();
	void StartConnectTimer_GameThread(float InInvokeDelay);
	void StopConnectTimer_GameThread();
	void OnConnectTimer();

	void ConnectControlClient(FCommsRequestParams InParams);
	bool CheckIfTakeExists(const FString& InTakeName) const;
	TArray<TakeId> AddTakes(const TArray<FGetTakeMetadataResponse::FTakeObject>& InTakeObjects);

	TProtocolResult<TakeId> FindTakeIdByName(const FString& InTakeName) const;
	void RemoveExportByName(const FString& InTakeName);

	void FetchThumbnails(TArray<TakeId> InIdList);
	TArray<FString> GetTakeNamesByIds(TArray<TakeId> InTakeIds);

	void InvokeGetTakesCallbackFromGameThread();

	bool StartCaptureHandler(TSharedPtr<FBaseCommandArgs> InCommand);
	bool StopCaptureHandler(TSharedPtr<FBaseCommandArgs> InCommand);

	void ClearCachedTakesWithEvent();

	void RegisterForAllEvents();
	void OnEvent(TSharedPtr<FControlUpdate> InEvent);

	void AddTakeByName(const FString& InTakeName);
	void RemoveTakeByName(const FString& InTakeName);

    static bool TakeContainsFiles(const FGetTakeMetadataResponse::FTakeObject& Take, const TArray<FString>& FileNames);

    FString DeviceIPAddress;
    uint16 DeviceControlPort;

    FControlMessenger ControlMessenger;
	TUniquePtr<FExportClient> ExportClient;

    mutable FCriticalSection ExportMapMutex;
	FExportMap ExportMap;

    TArray<TakeId> CurrentTakeIdList;

	FFilesMap FilesTakeContainsMap;

	std::atomic<bool> bIsConnected;
	FTimerHandle ConnectionTimer;
	TQueueRunner<FCommsRequestParams> CommsThread;

	TPerTakeCallback<void> GetTakesCallback;
};

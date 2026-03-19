// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCaptureSource.h"

#include "Async/EventSourceUtils.h"

class IFootageIngestAPI;
class FBaseCommandArgs;

namespace UE::MetaHuman
{

struct METAHUMANCAPTURESOURCE_API FIngesterParams
{
	FIngesterParams(
		EMetaHumanCaptureSourceType InCaptureSourceType,
		FDirectoryPath InStoragePath,
		FDeviceAddress InDeviceAddress,
		uint16 InDeviceControlPort,
		bool InShouldCompressDepthFiles,
		bool InCopyImagesToProject,
		float InMinDistance,
		float InMaxDistance,
		EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
		EMetaHumanCaptureDepthResolutionType InDepthResolution
	);

	EMetaHumanCaptureSourceType CaptureSourceType;
	FDirectoryPath StoragePath;
	FDeviceAddress DeviceAddress;
	uint16 DeviceControlPort;
	bool ShouldCompressDepthFiles;
	bool CopyImagesToProject;
	float MinDistance;
	float MaxDistance;
	EMetaHumanCaptureDepthPrecisionType DepthPrecision;
	EMetaHumanCaptureDepthResolutionType DepthResolution;
};

class METAHUMANCAPTURESOURCE_API FIngester : public FCaptureEventSource
{
public:
	using FRefreshCallback = TManagedDelegate<FMetaHumanCaptureVoidResult>;
	using FGetTakesCallbackPerTake = TManagedDelegate<FMetaHumanCapturePerTakeVoidResult>;

	explicit FIngester(FIngesterParams InIngesterParams);
	virtual ~FIngester();

	void SetParams(FIngesterParams InIngesterParams);

	bool CanStartup() const;
	bool CanIngestTakes() const;
	bool CanCancel() const;

	/**
	 * @brief Startup the footage ingest API. Get information on the available takes based on the type of this Capture Source
	 * @param bSynchronous If true, this will be a blocking function. Useful when initializing from blueprints or python
	 */
	void Startup(ETakeIngestMode InMode = ETakeIngestMode::Async);
	void Refresh(FRefreshCallback InCallback);

	void SetTargetPath(const FString& InTargetIngestDirectory, const FString& InTargetFolderAssetPath);
	void Shutdown();
	bool IsProcessing() const;
	bool IsCancelling() const;

	void CancelProcessing(const TArray<TakeId>& InTakeIdList);
	int32 GetNumTakes() const;
	TArray<TakeId> GetTakeIds() const;
	bool GetTakeInfo(TakeId InTakeId, FMetaHumanTakeInfo& OutTakeInfo) const;

	bool GetTakes(const TArray<TakeId>& InTakeIdList, FGetTakesCallbackPerTake InCallback);
	TOptional<float> GetProcessingProgress(TakeId InTakeId) const;
	FText GetProcessName(TakeId InTakeId) const;
	bool ExecuteCommand(class TSharedPtr<class FBaseCommandArgs> InCommand);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGetTakesFinished, const TArray<FMetaHumanTake>& InTakes)
	FOnGetTakesFinished OnGetTakesFinishedDelegate;

	EMetaHumanCaptureSourceType GetCaptureSourceType() const;

private:
	void ProxyEvent(TSharedPtr<const FCaptureEvent> Event);

	TUniquePtr<class IFootageIngestAPI> FootageIngestAPI;
	FIngesterParams Params;
};

}

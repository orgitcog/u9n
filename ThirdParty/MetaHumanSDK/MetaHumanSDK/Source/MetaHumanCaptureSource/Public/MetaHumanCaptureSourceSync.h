// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCaptureSource.h"

#include "MetaHumanCaptureSourceSync.generated.h"

namespace UE::MetaHuman
{
class FIngester;
}

UCLASS(BlueprintType)
class METAHUMANCAPTURESOURCE_API UMetaHumanCaptureSourceSync
	: public UObject
{
	GENERATED_BODY()

public:
	UMetaHumanCaptureSourceSync();
	UMetaHumanCaptureSourceSync(FVTableHelper& Helper);
	virtual ~UMetaHumanCaptureSourceSync();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	bool CanStartup() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	bool CanIngestTakes() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	bool CanCancel() const;

	/**
	 * @brief Startup the MetaHuman|Footage Ingest API. Get information on the available takes based on the type of this Capture Source
	 * @param bSynchronous If true, this will be a blocking function. Useful when initializing from blueprints or python
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	void Startup();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	TArray<FMetaHumanTakeInfo> Refresh();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	void SetTargetPath(const FString& InTargetIngestDirectory, const FString& InTargetFolderAssetPath);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	void Shutdown();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	bool IsProcessing() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	bool IsCancelling() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	void CancelProcessing(const TArray<int32>& InTakeIdList);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	int32 GetNumTakes() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	TArray<int32> GetTakeIds() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	bool GetTakeInfo(int32 InTakeId, FMetaHumanTakeInfo& OutTakeInfo) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Footage Ingest API")
	TArray<FMetaHumanTake> GetTakes(const TArray<int32>& InTakeIdList);

public:

	UPROPERTY(EditAnywhere, Category = "Capture Source")
	EMetaHumanCaptureSourceType CaptureSourceType = EMetaHumanCaptureSourceType::Undefined;

	UPROPERTY(EditAnywhere, Category = "Capture Source", meta = (EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceArchives || CaptureSourceType == EMetaHumanCaptureSourceType::HMCArchives || CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceArchivesRGB || CaptureSourceType == EMetaHumanCaptureSourceType::MonoArchives", EditConditionHides))
	FDirectoryPath StoragePath;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property has changed its type"))
	FString DeviceAddress_DEPRECATED;

	UPROPERTY(EditAnywhere, DisplayName = "Device Address", Category = "Capture Source", meta = (EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceConnection", EditConditionHides))
	FDeviceAddress DeviceIpAddress;

	UPROPERTY(EditAnywhere, Category = "Capture Source", meta = (EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceConnection", EditConditionHides))
	uint16 DeviceControlPort = 14785;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property isn't used anymore as the port is being provided automatically by the OS"))
	uint16 ExportListeningPort_DEPRECATED = 8000;

	// TODO: Expose this parameter once uncompressed EXR's are supported
	UPROPERTY(/*EditAnywhere, DisplayName = "Compress Depth Files", Category = "Capture Source", meta = (EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceArchives || CaptureSourceType == EMetaHumanCaptureSourceType::HMCArchives || CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceConnection", EditConditionHides)*/)
	bool ShouldCompressDepthFiles = true;

	UPROPERTY(EditAnywhere, Category = "Capture Source", meta = (EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::HMCArchives", EditConditionHides))
	bool CopyImagesToProject = true;

	UPROPERTY(EditAnywhere, Category = "Capture Source",
			  meta = (ToolTip = "The minimum cm from the camera expected for valid depth information.\n Depth information closer than this will be ignored to help filter out noise.",
					  ClampMin = "0.0", ClampMax = "200.0",
					  UIMin = "0.0", UIMax = "200.0",
					  EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::HMCArchives",
					  EditConditionHides))
	float MinDistance = 10.0;

	UPROPERTY(EditAnywhere, Category = "Capture Source",
			  meta = (ToolTip = "The maximum cm from the camera expected for valid depth information.\n Depth information beyond this will be ignored to help filter out noise.",
					  ClampMin = "0.0", ClampMax = "200.0",
					  UIMin = "0.0", UIMax = "200.0",
					  EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::HMCArchives",
					  EditConditionHides))
	float MaxDistance = 25.0;

	UPROPERTY(EditAnywhere, Category = "Capture Source",
			  meta = (ToolTip = "Precision of the calculated depth data. Full precision is more accurate, but requires more disk space to store.",
					  EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::HMCArchives || CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceArchivesRGB || CaptureSourceType == EMetaHumanCaptureSourceType::MonoArchives",
					  EditConditionHides))
	EMetaHumanCaptureDepthPrecisionType DepthPrecision = EMetaHumanCaptureDepthPrecisionType::Eightieth;

	UPROPERTY(EditAnywhere, Category = "Capture Source",
			  meta = (ToolTip = "Resolution scaling applied to the calculated depth data. Full resolution is more accurate, but requires more disk space to store.",
					  EditCondition = "CaptureSourceType == EMetaHumanCaptureSourceType::HMCArchives || CaptureSourceType == EMetaHumanCaptureSourceType::LiveLinkFaceArchivesRGB || CaptureSourceType == EMetaHumanCaptureSourceType::MonoArchives",
					  EditConditionHides))
	EMetaHumanCaptureDepthResolutionType DepthResolution = EMetaHumanCaptureDepthResolutionType::Full;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif

private:
	TUniquePtr<UE::MetaHuman::FIngester> Ingester;

	// Do not expose this property to the editor or blueprints, it is only for garbage collection purposes.
	UPROPERTY(Transient)
	TObjectPtr<UMetaHumanCaptureSource> MetaHumanCaptureSource;
};
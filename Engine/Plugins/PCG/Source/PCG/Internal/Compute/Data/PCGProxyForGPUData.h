// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"

#include "Compute/PCGDataForGPU.h"

#include "PCGProxyForGPUData.generated.h"

#define UE_API PCG_API

struct FPCGContext;

/** A proxy for data residing on the GPU with functionality to read the data back to the CPU. */
UCLASS(MinimalAPI, ClassGroup = (Procedural), DisplayName = "GPU Proxy")
class UPCGProxyForGPUData : public UPCGData
{
	GENERATED_BODY()

public:
	UE_API void Initialize(TSharedPtr<FPCGProxyForGPUDataCollection> InDataCollection, int InDataIndexInCollection);

	//~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::ProxyForGPU; }
	UE_API virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	virtual bool CanBeSerialized() const override { return false; }
	virtual bool HoldsTransientResources() const override { return true; }
	virtual bool IsCacheable() const override { return false; }
	UE_API virtual void ReleaseTransientResources(const TCHAR* InReason = nullptr);
	UE_API virtual EPCGDataType GetUnderlyingDataType() const override;
	//~End UPCGData interface

	/** Returns the GPU info. Returns null if the buffer has been discarded. */
	UE_API TSharedPtr<const FPCGProxyForGPUDataCollection> GetInputDataCollectionInfo() const;

	int GetDataIndexInCollection() const { return DataIndexInCollection; }

	struct FReadbackResult
	{
		/** Set false until read back has been performed. */
		bool bComplete = false;

		/** The data created from readback, null if read back failed. */
		FPCGTaggedData TaggedData;
	};

	/** Populates a CPU data object representing the GPU data, performing a readback from GPU->CPU if required. */
	UE_API FReadbackResult GetCPUData(FPCGContext* InContext) const;

	/** Returns the element count for this data. Does not trigger a GPU->CPU readback. */
	UE_API int GetElementCount() const;

	/** Returns a description of this data. Does not trigger a GPU->CPU readback. */
	UE_API bool GetDescription(FPCGDataDesc& OutDescription) const;

	/** Apply the given per-data element counts to the data description. */
	UE_API void UpdateElementCountsFromReadback(const TArray<uint32>& InElementCounts);

	UE_API TSharedPtr<const FPCGProxyForGPUDataCollection> GetGPUInfo() const;

protected:
	UE_API TSharedPtr<FPCGProxyForGPUDataCollection> GetGPUInfoMutable() const;

protected:
	UPROPERTY()
	int32 DataIndexInCollection = INDEX_NONE;

	TSharedPtr<FPCGProxyForGPUDataCollection> DataCollectionOnGPU;
};

#undef UE_API

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Data/PCGProxyForGPUData.h"

#include "PCGModule.h"

void UPCGProxyForGPUData::Initialize(TSharedPtr<FPCGProxyForGPUDataCollection> InDataCollection, int InDataIndexInCollection)
{
	DataCollectionOnGPU = InDataCollection;
	DataIndexInCollection = InDataIndexInCollection;
}

void UPCGProxyForGPUData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	// This data does not have a bespoke CRC implementation so just use the unique object instance UID.
	AddUIDToCrc(Ar);
}

void UPCGProxyForGPUData::ReleaseTransientResources(const TCHAR* InReason)
{
#ifdef PCG_DATA_USAGE_LOGGING
	UE_LOG(LogPCG, Warning, TEXT("Releasing GPU data for '%s' due to %s"), *GetName(), InReason ? InReason : TEXT("NOREASON"));
#endif

	DataCollectionOnGPU.Reset();
}

EPCGDataType UPCGProxyForGPUData::GetUnderlyingDataType() const
{
	if (TSharedPtr<const FPCGProxyForGPUDataCollection> DataCollectionGPU = GetGPUInfo())
	{
		return DataCollectionGPU->GetDescription().DataDescs[DataIndexInCollection].Type;
	}

	return EPCGDataType::None;
}

TSharedPtr<const FPCGProxyForGPUDataCollection> UPCGProxyForGPUData::GetInputDataCollectionInfo() const
{
	return DataCollectionOnGPU;
}

int UPCGProxyForGPUData::GetElementCount() const
{
	if (TSharedPtr<const FPCGProxyForGPUDataCollection> DataCollectionGPU = GetGPUInfo())
	{
		return DataCollectionGPU->GetDescription().DataDescs[DataIndexInCollection].ElementCount;
	}

	return 0;
}

bool UPCGProxyForGPUData::GetDescription(FPCGDataDesc& OutDescription) const
{
	if (TSharedPtr<const FPCGProxyForGPUDataCollection> DataCollectionGPU = GetGPUInfo())
	{
		OutDescription = DataCollectionGPU->GetDescription().DataDescs[DataIndexInCollection];
		return true;
	}

	return false;
}

void UPCGProxyForGPUData::UpdateElementCountsFromReadback(const TArray<uint32>& InElementCounts)
{
	if (TSharedPtr<FPCGProxyForGPUDataCollection> GPUInfo = GetGPUInfoMutable())
	{
		GPUInfo->UpdateElementCountsFromReadback(InElementCounts);
	}
}

UPCGProxyForGPUData::FReadbackResult UPCGProxyForGPUData::GetCPUData(FPCGContext* InContext) const
{
	TSharedPtr<FPCGProxyForGPUDataCollection> DataOnGPU = GetGPUInfoMutable();
	if (!DataOnGPU)
	{
		UE_LOG(LogPCG, Error, TEXT("Data collection lost! Enabling the define PCG_DATA_USAGE_LOGGING may help to identify when resource was released."));

		return FReadbackResult{ .bComplete = true };
	}

	FPCGTaggedData ResultData;
	if (!DataOnGPU->GetCPUData(InContext, DataIndexInCollection, ResultData))
	{
		return FReadbackResult{ .bComplete = false };
	}

	return FReadbackResult{ .bComplete = true, .TaggedData = MoveTemp(ResultData) };
}

TSharedPtr<const FPCGProxyForGPUDataCollection> UPCGProxyForGPUData::GetGPUInfo() const
{
	return GetGPUInfoMutable();
}

TSharedPtr<FPCGProxyForGPUDataCollection> UPCGProxyForGPUData::GetGPUInfoMutable() const
{
	if (!DataCollectionOnGPU)
	{
		ensureMsgf(false, TEXT("Data %s: GPU data collection lost. Enabling the define 'PCG_DATA_USAGE_LOGGING' may help to identify when resource was released."), *GetName());
		return nullptr;
	}
	else if (!DataCollectionOnGPU->GetDescription().DataDescs.IsValidIndex(DataIndexInCollection))
	{
		ensureMsgf(false, TEXT("Data %s: DataIndexInCollection (%d) was out of range [0, %d)."), *GetName(), DataIndexInCollection, DataCollectionOnGPU->GetDescription().DataDescs.Num());
		return nullptr;
	}

	return DataCollectionOnGPU;
}

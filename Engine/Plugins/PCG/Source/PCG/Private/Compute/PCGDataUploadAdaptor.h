// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGDataForGPU.h"

#include "CoreMinimal.h"
#include "RendererInterface.h"

class FRDGBuilder;
class UPCGDataBinding;
enum class EPCGExportMode : uint8;

/** A helper to ingest incoming data objects and pass the required data to the data interface. */
class FPCGDataUploadAdaptor
{
public:
	FPCGDataUploadAdaptor(UPCGDataBinding* InDataBinding, const FPCGDataCollectionDesc* InTargetDataCollectionDesc, FName InInputPinLabel);

	/** Do any preparation work such as data readbacks. Returns true when preparation is complete. */
	bool PrepareData_GameThread();

	FRDGBufferSRVRef GetAttributeRemapBufferSRV(FRDGBuilder& InGraphBuilder);

	FRDGBufferSRVRef GetBufferToGraphStringKeySRV(FRDGBuilder& InGraphBuilder, int32& OutNumRemappedStringKeys);

	/** Create buffer with the element counts of each data. Stored in a buffer because we do not constrain the max data count. */
	FRDGBufferSRVRef GetDataElementCountsBufferSRV(FRDGBuilder& InGraphBuilder);

	/** Gets the buffer that can then be used to read the data in kernels. */
	FRDGBufferRef GetBuffer_RenderThread(FRDGBuilder& InGraphBuilder, EPCGExportMode InExportMode);

	bool UsesStringKeyRemap() const { return ExternalBufferMaxStringKeyValue != INDEX_NONE; }

	bool IsUploadingFromCPU() const { return !ExternalBufferForReuse; }

private:
	TRefCountPtr<FRDGPooledBuffer> ExternalBufferForReuse;

	int32 ExternalBufferMaxStringKeyValue = INDEX_NONE;

	TWeakObjectPtr<UPCGDataBinding> DataBinding;

	FPCGDataCollectionDesc TargetDataCollectionDesc;

	/** If data cannot be reused from GPU, will be packed into this buffer prior to upload. */
	TArray<uint32> PackedDataCollection;

	TArray<FIntVector2> SourceToTargetAttributeId;

	FName InputPinLabel;

	uint32 ExternalBufferSizeBytes = 0;

	/** Map from string key values buffer from upstream compute graph to corresponding string key values in this graph. */
	TArray<int32> BufferToGraphStringKey;
};

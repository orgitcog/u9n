// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/PCGDataUploadAdaptor.h"

#include "PCGDataCollectionAdaptorCS.h"
#include "PCGModule.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/Data/PCGProxyForGPUData.h"

#include "RenderCaptureInterface.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"

namespace PCGDataUploadAdaptor
{
#if !UE_BUILD_SHIPPING
	static int32 TriggerGPUCaptureDispatches = 0;
	static FAutoConsoleVariableRef CVarTriggerGPUCapture(
		TEXT("pcg.GPU.TriggerRenderCaptures.DataUploadAdaptor"),
		TriggerGPUCaptureDispatches,
		TEXT("Trigger GPU captures for this many of the subsequent data collection update dispatches."));
#endif // !UE_BUILD_SHIPPING
}

FPCGDataUploadAdaptor::FPCGDataUploadAdaptor(UPCGDataBinding* InDataBinding, const FPCGDataCollectionDesc* InTargetDataCollectionDesc, FName InInputPinLabel)
{
	check(IsInGameThread());
	check(InTargetDataCollectionDesc);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CopyDataDescription);

		// @todo_pcg: Avoid costly copy of data description.
		TargetDataCollectionDesc = *InTargetDataCollectionDesc;
	}
	InputPinLabel = InInputPinLabel;
	DataBinding = InDataBinding;

	int NumDataForThisPin = 0;

	// Are all our data items coming direct from GPU - and importantly are a complete, single data collection?
	bool bMultipleBuffersEncountered = false;
	TSharedPtr<const FPCGProxyForGPUDataCollection> UniqueDataCollection;
	for (int InputDataIndex = 0; InputDataIndex < DataBinding->InputDataCollection.TaggedData.Num(); ++InputDataIndex)
	{
		const FName DataPin = DataBinding->InputDataCollection.TaggedData[InputDataIndex].Pin;

		if (InInputPinLabel != DataPin)
		{
			continue;
		}

		++NumDataForThisPin;

		const UPCGProxyForGPUData* DataGPU = Cast<UPCGProxyForGPUData>(DataBinding->InputDataCollection.TaggedData[InputDataIndex].Data);
		if (!DataGPU)
		{
			// Encountered CPU data, no reuse for now.
			// TODO: Support multiple input buffers and only upload required data.
			UniqueDataCollection.Reset();
			break;
		}

		TSharedPtr<const FPCGProxyForGPUDataCollection> InputDataCollection = DataGPU->GetInputDataCollectionInfo();
		if (!InputDataCollection)
		{
			UE_LOG(LogPCG, Error, TEXT("No reuse: Missing data collection buffer!"));

			UniqueDataCollection.Reset();
			break;
		}

		if (!UniqueDataCollection)
		{
			UniqueDataCollection = InputDataCollection;
		}
		else if (UniqueDataCollection != InputDataCollection || DataGPU->GetDataIndexInCollection() != InputDataIndex)
		{
			// Multiple input buffers or out of order data items, no reuse for now.
			// TODO: support indirection of data items.
			UniqueDataCollection.Reset();
			break;
		}
	}

	if (UniqueDataCollection && UniqueDataCollection->GetDescription().DataDescs.Num() != NumDataForThisPin)
	{
		// We're not using all the data items from the buffer, no reuse for now.
		// TODO: Support indirection of data items.
		UniqueDataCollection.Reset();
	}

	if (UniqueDataCollection)
	{
		ExternalBufferForReuse = UniqueDataCollection->GetBuffer();
		ExternalBufferSizeBytes = UniqueDataCollection->GetBufferSizeBytes();

		ensureAlways(ExternalBufferMaxStringKeyValue == INDEX_NONE);

		for (const FPCGDataDesc& DataDesc : UniqueDataCollection->GetDescription().DataDescs)
		{
			for (const FPCGKernelAttributeDesc& AttrDesc : DataDesc.AttributeDescs)
			{
				for (int32 StringKeyValue : AttrDesc.GetUniqueStringKeys())
				{
					ExternalBufferMaxStringKeyValue = FMath::Max(ExternalBufferMaxStringKeyValue, StringKeyValue);
				}
			}
		}
	}
}

/** Do any preparation work such as data readbacks. Returns true when preparation is complete. */
bool FPCGDataUploadAdaptor::PrepareData_GameThread()
{
	UPCGDataBinding* Binding = DataBinding.Get();
	if (!ensure(Binding))
	{
		return true;
	}

	check(IsInGameThread());

	if (ExternalBufferForReuse)
	{
		ensureAlways(SourceToTargetAttributeId.IsEmpty());

		SourceToTargetAttributeId.Reserve(Binding->GetAttributeIdRemap().Num());

		for (const TPair<UPCGDataBinding::FSourceBufferAndAttributeIndex, int32>& Entry : Binding->GetAttributeIdRemap())
		{
			const TSharedPtr<const FPCGProxyForGPUDataCollection>& SourceBuffer = Entry.Key.Key;

			if (SourceBuffer && SourceBuffer->GetBuffer() == ExternalBufferForReuse)
			{
				const int32 GraphAttributeId = PCGComputeHelpers::GetAttributeIdFromMetadataAttributeIndex(Entry.Value);

				const int32 SourceBufferAttributeId = PCGComputeHelpers::GetAttributeIdFromMetadataAttributeIndex(Entry.Key.Value);

				ensure(GraphAttributeId >= PCGComputeConstants::NUM_RESERVED_ATTRS);
				ensure(SourceBufferAttributeId >= PCGComputeConstants::NUM_RESERVED_ATTRS);

				SourceToTargetAttributeId.Add(FIntVector2(SourceBufferAttributeId, GraphAttributeId));
			}
		}

		ensureAlways(BufferToGraphStringKey.IsEmpty());

		if (ExternalBufferMaxStringKeyValue != INDEX_NONE)
		{
			const int32 NumSourceStringKeys = ExternalBufferMaxStringKeyValue + 1;
			BufferToGraphStringKey.SetNumUninitialized(NumSourceStringKeys);

			// Now compute the mapping for any string keys.
			BufferToGraphStringKey[0] = 0;
			for (int32 SourceBufferStringKey = 1; SourceBufferStringKey < NumSourceStringKeys; ++SourceBufferStringKey)
			{
				const int32* GraphStringKeyIndex = Binding->GetSourceBufferToGraphStringKey().Find({ ExternalBufferForReuse, SourceBufferStringKey });

				BufferToGraphStringKey[SourceBufferStringKey] = GraphStringKeyIndex ? *GraphStringKeyIndex : SourceBufferStringKey;
			}
		}
	}
	else
	{
		const uint64 BufferSize = TargetDataCollectionDesc.ComputePackedSizeBytes();
		if (PCGComputeHelpers::IsBufferSizeTooLarge(BufferSize))
		{
			return true;
		}

		bool bAllDataReady = true;

		for (FPCGTaggedData& TaggedData : Binding->InputDataCollection.TaggedData)
		{
			if (TaggedData.Pin != InputPinLabel)
			{
				continue;
			}

			if (const UPCGProxyForGPUData* Proxy = Cast<UPCGProxyForGPUData>(TaggedData.Data))
			{
				UPCGProxyForGPUData::FReadbackResult Result = Proxy->GetCPUData(/*InContext=*/nullptr);

				if (!Result.bComplete)
				{
					bAllDataReady = false;
				}
				else
				{
					TaggedData.Data = Result.TaggedData.Data;
					TaggedData.Tags = MoveTemp(Result.TaggedData.Tags);
				}
			}
		}

		if (!bAllDataReady)
		{
			return false;
		}

		TargetDataCollectionDesc.PackDataCollection(Binding->InputDataCollection, InputPinLabel, Binding, PackedDataCollection);
	}

	return true;
}

FRDGBufferSRVRef FPCGDataUploadAdaptor::GetAttributeRemapBufferSRV(FRDGBuilder& InGraphBuilder)
{
	const FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FIntVector2), FMath::Max(SourceToTargetAttributeId.Num(), 1));

	if (SourceToTargetAttributeId.IsEmpty())
	{
		FRDGBufferRef NullBuffer = InGraphBuilder.CreateBuffer(Desc, TEXT("EmptyRemap"));
		InGraphBuilder.QueueBufferUpload(NullBuffer, &FUintVector2::ZeroValue, sizeof(FUintVector2::ZeroValue));

		return InGraphBuilder.CreateSRV(NullBuffer);
	}
	else
	{
		FRDGBufferRef Buffer = InGraphBuilder.CreateBuffer(Desc, TEXT("PCGDataCollectionUpload_AttributeRemap"));

		InGraphBuilder.QueueBufferUpload(Buffer, MakeArrayView(SourceToTargetAttributeId));

		return InGraphBuilder.CreateSRV(Buffer);
	}
}

FRDGBufferSRVRef FPCGDataUploadAdaptor::GetBufferToGraphStringKeySRV(FRDGBuilder& InGraphBuilder, int32& OutNumRemappedStringKeys)
{
	// Should only be called if UsesStringKeyRemap() returns true and ExternalBufferForReuse is set.
	check(ExternalBufferForReuse);

	OutNumRemappedStringKeys = BufferToGraphStringKey.Num();

	FRDGBufferRef Buffer = nullptr;

	if (!BufferToGraphStringKey.IsEmpty())
	{
		const FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(BufferToGraphStringKey.GetTypeSize(), BufferToGraphStringKey.Num());
		Buffer = InGraphBuilder.CreateBuffer(BufferDesc, TEXT("PCGDataUploadAdaptor_BufferToGraphStringKey"));

		InGraphBuilder.QueueBufferUpload(Buffer, MakeArrayView(BufferToGraphStringKey));
	}
	else
	{
		Buffer = GSystemTextures.GetDefaultStructuredBuffer(InGraphBuilder, sizeof(uint32));
	}

	return InGraphBuilder.CreateSRV(Buffer);
}

/** Create buffer with the element counts of each data. Stored in a buffer because we do not constrain the max data count. */
FRDGBufferSRVRef FPCGDataUploadAdaptor::GetDataElementCountsBufferSRV(FRDGBuilder& InGraphBuilder)
{
	TArray<uint32> DataElementCounts;
	DataElementCounts.Reserve(TargetDataCollectionDesc.DataDescs.Num());

	for (const FPCGDataDesc& DataDesc : TargetDataCollectionDesc.DataDescs)
	{
		DataElementCounts.Add(DataDesc.ElementCount);
	}

	if (DataElementCounts.IsEmpty())
	{
		const FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DataElementCounts.Num());
		FRDGBufferRef NullBuffer = InGraphBuilder.CreateBuffer(Desc, TEXT("EmptyDataElementCounts"));

		uint32 Zero = 0;
		InGraphBuilder.QueueBufferUpload(NullBuffer, &Zero, sizeof(Zero));

		return InGraphBuilder.CreateSRV(NullBuffer);
	}
	else
	{
		const FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DataElementCounts.Num());

		FRDGBufferRef Buffer = InGraphBuilder.CreateBuffer(Desc, TEXT("PCGDataCollectionUpload_DataElementCounts"));

		InGraphBuilder.QueueBufferUpload(Buffer, MakeArrayView(DataElementCounts));

		return InGraphBuilder.CreateSRV(Buffer);
	}
}

/** Gets the buffer that can then be used to read the data in kernels. */
FRDGBufferRef FPCGDataUploadAdaptor::GetBuffer_RenderThread(FRDGBuilder& InGraphBuilder, EPCGExportMode InExportMode)
{
	RDG_EVENT_SCOPE(InGraphBuilder, "FPCGDataUploadAdaptor::GetBuffer_RenderThread");

	if (!ExternalBufferForReuse)
	{
		// Not reusing an existing buffer, create a new buffer and pack the input data collection.
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateByteAddressDesc(sizeof(uint32) * PackedDataCollection.Num());
		if (InExportMode != EPCGExportMode::NoExport)
		{
			// We don't know for sure whether buffer will be read back or not, so need to flag the possibility if the buffer will be passed downstream.
			Desc.Usage |= BUF_SourceCopy;
		}

		FRDGBufferRef Buffer = InGraphBuilder.CreateBuffer(Desc, TEXT("PCGDataCollectionUpload"));

		InGraphBuilder.QueueBufferUpload(Buffer, PackedDataCollection.GetData(), PackedDataCollection.Num() * PackedDataCollection.GetTypeSize());

		return Buffer;
	}

	ensure(PackedDataCollection.IsEmpty());

	// Copy the external input buffer to a transient buffer, then run a CS to adapt it:
	// - Update data counts
	// - Remap attribute IDs

#if !UE_BUILD_SHIPPING
	RenderCaptureInterface::FScopedCapture RenderCapture(PCGDataUploadAdaptor::TriggerGPUCaptureDispatches > 0, InGraphBuilder, TEXT("FPCGDataUploadAdaptor"));
	PCGDataUploadAdaptor::TriggerGPUCaptureDispatches = FMath::Max(PCGDataUploadAdaptor::TriggerGPUCaptureDispatches - 1, 0);
#endif // !UE_BUILD_SHIPPING

	FRDGBufferRef InputBuffer = InGraphBuilder.RegisterExternalBuffer(ExternalBufferForReuse);
	const uint32 InputBufferSizeBytes = ExternalBufferSizeBytes;
	const uint32 InputBufferSizeUints = InputBufferSizeBytes / sizeof(uint32);

	const uint32 OutputSizeBytes = InputBufferSizeBytes;
	FRDGBufferDesc OutputDesc = FRDGBufferDesc::CreateByteAddressDesc(OutputSizeBytes);
	if (InExportMode != EPCGExportMode::NoExport)
	{
		// We don't know for sure whether buffer will be read back or not, so need to flag the possibility if the buffer will be passed downstream.
		OutputDesc.Usage |= BUF_SourceCopy;
	}

	FRDGBufferRef OutputBuffer = InGraphBuilder.CreateBuffer(OutputDesc, TEXT("PCG_UDC_AttributesRemapped"));

	// Populate using copy. We can't do everything in one pass trivially due to race conditions between groups.
	AddCopyBufferPass(InGraphBuilder, OutputBuffer, InputBuffer);

	FPCGDataCollectionAdaptorCS::FParameters* Parameters = InGraphBuilder.AllocParameters<FPCGDataCollectionAdaptorCS::FParameters>();
	Parameters->InSourceToTargetAttributeId = GetAttributeRemapBufferSRV(InGraphBuilder);
	Parameters->InDataElementCounts = GetDataElementCountsBufferSRV(InGraphBuilder);
	Parameters->InDataCollection = InGraphBuilder.CreateSRV(InputBuffer);
	Parameters->OutDataCollection = InGraphBuilder.CreateUAV(OutputBuffer);
	Parameters->InNumData = TargetDataCollectionDesc.DataDescs.Num();
	Parameters->InNumRemappedAttributes = SourceToTargetAttributeId.Num();

	const uint32 NumUpdateTasks = Parameters->InNumData + Parameters->InNumRemappedAttributes * Parameters->InNumData;
	const int GroupCount = FMath::DivideAndRoundUp(NumUpdateTasks, FPCGDataCollectionAdaptorCS::NumThreadsPerGroup);

	TShaderMapRef<FPCGDataCollectionAdaptorCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderUtils::AddPass(InGraphBuilder, RDG_EVENT_NAME("PCGDataCollectionAdaptor"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull /*TODO*/, Shader, Parameters, FIntVector(GroupCount, 1, 1));

	return OutputBuffer;
}

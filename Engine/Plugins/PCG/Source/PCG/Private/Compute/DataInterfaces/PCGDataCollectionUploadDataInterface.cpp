// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/DataInterfaces/PCGDataCollectionUploadDataInterface.h"

#include "PCGEdge.h"
#include "PCGGraphExecutionInspection.h"
#include "PCGModule.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGSubsystem.h"
#include "PCGDataCollectionAdaptorCS.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGDataUploadAdaptor.h"
#include "Compute/Data/PCGProxyForGPUData.h"

#include "RenderCaptureInterface.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"

UComputeDataProvider* UPCGDataCollectionUploadDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGDataProviderDataCollectionUpload>();
}

void UPCGDataProviderDataCollectionUpload::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataProviderDataCollectionUpload::Initialize);

	// @todo_pcg: Properly initializing this data provider causes unexpected behavior.
	// Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	const UPCGDataCollectionUploadDataInterface* DataInterface = CastChecked<UPCGDataCollectionUploadDataInterface>(InDataInterface);

	Binding = CastChecked<UPCGDataBinding>(InBinding);
	SetProducerSettings(DataInterface->GetProducerSettings());
	GraphBindingIndex = DataInterface->GraphBindingIndex;

	// Pick the data items from input data collection using any of the compute graph element virtual input pin labels.
	check(!DataInterface->DownstreamInputPinLabelAliases.IsEmpty());
	DownstreamInputPinLabels = DataInterface->DownstreamInputPinLabelAliases;
}

bool UPCGDataProviderDataCollectionUpload::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataProviderDataCollectionUpload::PrepareForExecute_GameThread);
	check(InBinding);

	if (!DataAdaptor)
	{
		PinDesc = Binding->GetCachedKernelPinDataDesc(GraphBindingIndex);

		if (ensure(PinDesc))
		{
			check(!DownstreamInputPinLabels.IsEmpty());
			DataAdaptor = MakeShared<FPCGDataUploadAdaptor>(InBinding, PinDesc, DownstreamInputPinLabels[0]);
		}
	}

	const bool bPreparationDone = !DataAdaptor || DataAdaptor->PrepareData_GameThread();

#if WITH_EDITOR
	if (bPreparationDone && DataAdaptor && DataAdaptor->IsUploadingFromCPU())
	{
		NotifyProducerUploadedData(InBinding);
	}
#endif

	return bPreparationDone;
}

void UPCGDataProviderDataCollectionUpload::ReleaseTransientResources(const TCHAR* InReason)
{
#ifdef PCG_DATA_USAGE_LOGGING
	UE_LOG(LogPCG, Warning, TEXT("%s: Releasing resources due to %s"), *GetName(), InReason ? InReason : TEXT("NOREASON"));
#endif

	DataAdaptor.Reset();
}

FComputeDataProviderRenderProxy* UPCGDataProviderDataCollectionUpload::GetRenderProxy()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataProviderDataCollectionUpload::GetRenderProxy);

	return new FPCGDataProviderDataCollectionUploadProxy(
		DataAdaptor,
		this,
		PinDesc,
		OutputPinLabel,
		OutputPinLabelAlias,
		ExportMode,
		bRequiresZeroInitialization,
		ElementCountMultiplier,
		bExposeElementCounters);
}

void UPCGDataProviderDataCollectionUpload::Reset()
{
	DownstreamInputPinLabels.Empty();
	DataAdaptor.Reset();

	Super::Reset();
}

FPCGDataProviderDataCollectionUploadProxy::FPCGDataProviderDataCollectionUploadProxy(
	TSharedPtr<FPCGDataUploadAdaptor> InDataAdaptor,
	TWeakObjectPtr<UPCGDataCollectionDataProvider> InDataProvider,
	const FPCGDataCollectionDesc* InPinDesc,
	FName InOutputPinLabel,
	FName InOutputPinLabelAlias,
	EPCGExportMode InExportMode,
	bool bInZeroInitialize,
	uint32 InElementCountMultiplier,
	bool bInExposeElementCounters)
	: FPCGDataCollectionDataProviderProxy(InDataProvider, InPinDesc, InExportMode, bInZeroInitialize, InElementCountMultiplier, bInExposeElementCounters, InOutputPinLabel, InOutputPinLabelAlias)
	, DataAdaptor(InDataAdaptor)
{
}

bool FPCGDataProviderDataCollectionUploadProxy::IsValid(FValidationData const& InValidationData) const
{
	if (!DataAdaptor.IsValid())
	{
		UE_LOG(LogPCG, Warning, TEXT("FPCGDataProviderDataCollectionUploadProxy invalid due to null DataAdaptor."));
		return false;
	}

	return FPCGDataCollectionDataProviderProxy::IsValid(InValidationData);
}

void FPCGDataProviderDataCollectionUploadProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataProviderDataCollectionUploadProxy::AllocateResources);

	{
		Buffer = DataAdaptor->GetBuffer_RenderThread(GraphBuilder, ExportMode);
		BufferUAV = GraphBuilder.CreateUAV(Buffer);
	}

	{
		// Ensure we always allocate at least 1 element so we have something to bind.
		const uint32 NumCounters = bExposeElementCounters ? FMath::Max(PinDesc.DataDescs.Num(), 1) : 1;

		FRDGBufferDesc CountersDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumCounters);
		CountersDesc.Usage |= BUF_SourceCopy;

		Counters = GraphBuilder.CreateBuffer(CountersDesc, TEXT("PCGDataCollectionUpload_Counters"));
		CountersUAV = GraphBuilder.CreateUAV(Counters);

		// A single counter, or a small number of counters, will be a very common case.
		if (NumCounters <= 4)
		{
			GraphBuilder.QueueBufferUpload(Counters, &FUintVector4::ZeroValue, NumCounters * sizeof(uint32));
		}
		else
		{
			TArray<uint32> Zeroes;
			Zeroes.SetNumZeroed(NumCounters);

			GraphBuilder.QueueBufferUpload(Counters, Zeroes.GetData(), Zeroes.Num() * Zeroes.GetTypeSize());
		}
	}

	BufferToGraphStringKeySRV = GetBufferToGraphStringKeySRV(GraphBuilder, NumRemappedStringKeys);
}

FRDGBufferSRVRef FPCGDataProviderDataCollectionUploadProxy::GetBufferToGraphStringKeySRV(FRDGBuilder& GraphBuilder, int32& OutNumRemappedStringKeys) const
{
	if (!DataAdaptor || !DataAdaptor->UsesStringKeyRemap())
	{
		// No remap required.
		return FPCGDataCollectionDataProviderProxy::GetBufferToGraphStringKeySRV(GraphBuilder, OutNumRemappedStringKeys);
	}
	else
	{
		return DataAdaptor->GetBufferToGraphStringKeySRV(GraphBuilder, OutNumRemappedStringKeys);
	}
}

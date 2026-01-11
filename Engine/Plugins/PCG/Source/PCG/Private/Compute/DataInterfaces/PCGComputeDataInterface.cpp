// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/DataInterfaces/PCGComputeDataInterface.h"

#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGGraphExecutionInspection.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGSettings.h"
#include "Compute/PCGComputeKernel.h"
#include "Compute/PCGDataBinding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGComputeDataInterface)

#if WITH_EDITOR
namespace PCGComputeDataInterfaceHelpers
{
	static TAutoConsoleVariable<bool> CVarEnableInternalKernelPinInspection(
		TEXT("pcg.GPU.EnableInternalKernelPinInspection"),
		false,
		TEXT("Allows internal kernel pins to be inspected and debugged. Note, this only works if the internal kernel pin name matches a pin name on the node."));
}
#endif

void UPCGComputeDataInterface::AddDownstreamInputPin(FName InInputPinLabel, const FName* InOptionalInputPinLabelAlias)
{
	DownstreamInputPinLabelAliases.AddUnique(InOptionalInputPinLabelAlias ? *InOptionalInputPinLabelAlias : InInputPinLabel);
}

void UPCGComputeDataInterface::SetOutputPin(FName InOutputPinLabel, const FName* InOptionalOutputPinLabelAlias)
{
	OutputPinLabel = InOutputPinLabel;
	OutputPinLabelAlias = InOptionalOutputPinLabelAlias ? *InOptionalOutputPinLabelAlias : InOutputPinLabel;
}

void UPCGComputeDataInterface::SetProducerSettings(const UPCGSettings* InProducerSettings)
{
	ResolvedProducerSettings = InProducerSettings;
	ProducerSettings = ResolvedProducerSettings;
}

const UPCGSettings* UPCGComputeDataInterface::GetProducerSettings() const
{
	if (!ResolvedProducerSettings)
	{
		FGCScopeGuard Guard;
		ResolvedProducerSettings = ProducerSettings.Get();
	}

	return ResolvedProducerSettings;
}

void UPCGComputeDataInterface::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UPCGComputeDataInterface* This = CastChecked<UPCGComputeDataInterface>(InThis);
	Collector.AddReferencedObject(This->ResolvedProducerSettings);
}

void UPCGComputeDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	// Bump counter so any async callbacks from previous usages of this provider will be invalidated.
	++GenerationCounter;

	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	const UPCGComputeDataInterface* DataInterface = CastChecked<UPCGComputeDataInterface>(InDataInterface);
	ProducerKernel = DataInterface->ProducerKernel;
	ResolvedProducerSettings = DataInterface->GetProducerSettings();
	ProducerSettings = ResolvedProducerSettings;

	GraphBindingIndex = DataInterface->GraphBindingIndex;

	// The original label is needed to store inspection data.
	OutputPinLabel = DataInterface->OutputPinLabel;

	// Use the aliased label for normal data output as this is the output from the compute graph.
	OutputPinLabelAlias = DataInterface->OutputPinLabelAlias;

	DownstreamInputPinLabelAliases = DataInterface->DownstreamInputPinLabelAliases;
}

void UPCGComputeDataProvider::Reset()
{
	Super::Reset();

	// Bump counter so any async callbacks from usages of this provider will be invalidated.
	++GenerationCounter;

	ProducerKernel = nullptr;
	ProducerSettings = nullptr;
	ResolvedProducerSettings = nullptr;
	GraphBindingIndex = INDEX_NONE;
	OutputPinLabel = NAME_None;
	OutputPinLabelAlias = NAME_None;
	DownstreamInputPinLabelAliases.Empty();
}

const UPCGSettings* UPCGComputeDataProvider::GetProducerSettings() const
{
	if (!ResolvedProducerSettings)
	{
		FGCScopeGuard Guard;
		ResolvedProducerSettings = ProducerSettings.Get();
	}

	return ResolvedProducerSettings;
}

void UPCGComputeDataProvider::SetProducerSettings(const UPCGSettings* InSettings)
{
	ProducerSettings = InSettings;
	ResolvedProducerSettings = InSettings;
}

void UPCGComputeDataProvider::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UPCGComputeDataProvider* This = CastChecked<UPCGComputeDataProvider>(InThis);
	Collector.AddReferencedObject(This->ResolvedProducerSettings);
}

#if WITH_EDITOR
void UPCGComputeDataProvider::NotifyProducerUploadedData(UPCGDataBinding* InBinding)
{
	const UPCGSettings* LocalProducerSettings = GetProducerSettings();

	if (LocalProducerSettings && !LocalProducerSettings->ShouldExecuteOnGPU())
	{
		const UPCGNode* ProducerNode = Cast<UPCGNode>(LocalProducerSettings->GetOuter());

		// Works around current issue where input output settings are outer'd to the graph rather than their node.
		if (!ProducerNode)
		{
			if (const UPCGGraph* Graph = Cast<UPCGGraph>(LocalProducerSettings->GetOuter()))
			{
				if (Graph->GetInputNode() && Graph->GetInputNode()->GetSettings() == LocalProducerSettings)
				{
					ProducerNode = Graph->GetInputNode();
				}
			}
		}

		TSharedPtr<FPCGContextHandle> ContextHandle = InBinding->ContextHandle.Pin();
		FPCGContext* Context = ContextHandle ? ContextHandle->GetContext() : nullptr;
		if (ProducerNode && Context && Context->GetStack() && Context->ExecutionSource.IsValid())
		{
			Context->ExecutionSource->GetExecutionState().GetInspection().NotifyCPUToGPUUpload(ProducerNode, Context->GetStack());
		}
	}
}
#endif // WITH_EDITOR

void UPCGExportableDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);
	Binding = CastChecked<UPCGDataBinding>(InBinding);

	const UPCGExportableDataInterface* DataInterface = CastChecked<UPCGExportableDataInterface>(InDataInterface);
	ExportMode = DataInterface->GetRequiresExport() ? EPCGExportMode::ComputeGraphOutput : EPCGExportMode::NoExport;

#if WITH_EDITOR
	// Some exportable data providers don't support inspect/debug because their producer settings are not available (e.g. GridLinkage).
	const UPCGSettings* LocalProducerSettings = GetProducerSettings();
	const bool bCanInspectOrDebug = LocalProducerSettings && (!ProducerKernel || !ProducerKernel->IsPinInternal(OutputPinLabel) || PCGComputeDataInterfaceHelpers::CVarEnableInternalKernelPinInspection.GetValueOnAnyThread());

	if (bCanInspectOrDebug)
	{
		const IPCGGraphExecutionSource* ExecutionSource = Binding->GetExecutionSource();

		if (LocalProducerSettings->bIsInspecting && ExecutionSource && ExecutionSource->GetExecutionState().GetInspection().IsInspecting())
		{
			ExportMode |= EPCGExportMode::Inspection;
		}

		if (LocalProducerSettings->bDebug)
		{
			ExportMode |= EPCGExportMode::DebugVisualization;
		}
	}
#endif
}

void UPCGExportableDataProvider::Reset()
{
	Super::Reset();

	ExportMode = EPCGExportMode::NoExport;
	OnDataExported = {};
	Binding.Reset();
	PinDesc = nullptr;
}

bool UPCGExportableDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	check(InBinding);

	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	PinDesc = InBinding->GetCachedKernelPinDataDesc(GraphBindingIndex);

	return true;
}

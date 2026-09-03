// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/PCGComputeGraph.h"

#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGNode.h"
#include "Compute/PCGCompilerDiagnostic.h"
#include "Compute/PCGComputeKernel.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Metadata/PCGMetadata.h"

#include "ComputeFramework/ComputeKernelCompileResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGComputeGraph)

#define PCG_DATA_DESCRIPTION_LOGGING 0

uint32 GetTypeHash(const FPCGKernelPin& In)
{
	return HashCombine(GetTypeHash(In.KernelIndex), GetTypeHash(In.PinLabel), GetTypeHash(In.bIsInput));
}

static FPCGCompilerDiagnostic ProcessCompilationMessage(const FComputeKernelCompileMessage& InMessage)
{
	FPCGCompilerDiagnostic Diagnostic;

	if (InMessage.Type == FComputeKernelCompileMessage::EMessageType::Error)
	{
		Diagnostic.Level = EPCGDiagnosticLevel::Error;
	}
	else if (InMessage.Type == FComputeKernelCompileMessage::EMessageType::Warning)
	{
		Diagnostic.Level = EPCGDiagnosticLevel::Warning;
	}
	else if (InMessage.Type == FComputeKernelCompileMessage::EMessageType::Info)
	{
		Diagnostic.Level = EPCGDiagnosticLevel::Info;
	}

	Diagnostic.Line = InMessage.Line;
	Diagnostic.ColumnStart = InMessage.ColumnStart;
	Diagnostic.ColumnEnd = InMessage.ColumnEnd;

	FString Message;
	if (!InMessage.VirtualFilePath.IsEmpty())
	{
		Message = InMessage.VirtualFilePath;
		if (InMessage.Line != -1)
		{
			if (InMessage.ColumnStart == InMessage.ColumnEnd)
			{
				Message += FString::Printf(TEXT(" (%d,%d)"), InMessage.Line, InMessage.ColumnStart);
			}
			else
			{
				Message += FString::Printf(TEXT(" (%d,%d-%d)"), InMessage.Line, InMessage.ColumnStart, InMessage.ColumnEnd);
			}
		}
		Message += TEXT(": ");
	}
	Message += InMessage.Text;
	Diagnostic.Message = FText::FromString(Message);

	return Diagnostic;
}

void UPCGComputeGraph::OnKernelCompilationComplete(int32 InKernelIndex, FComputeKernelCompileResults const& InCompileResults)
{
	const UPCGNode* Node = (KernelToNode.IsValidIndex(InKernelIndex) && KernelToNode[InKernelIndex].IsValid()) ? KernelToNode[InKernelIndex].Get() : nullptr;

	if (Node)
	{
		KernelToCompileMessages.FindOrAdd(KernelToNode[InKernelIndex].Get()) = InCompileResults.Messages;

		UPCGGraph* Graph = Cast<UPCGGraph>(Node->GetOuter());
		if (Graph)
		{
			FPCGCompilerDiagnostics Diagnostics;
			Diagnostics.Diagnostics.Reserve(InCompileResults.Messages.Num());

			Algo::Transform(InCompileResults.Messages, Diagnostics.Diagnostics, [](const FComputeKernelCompileMessage& InMessage)
			{
				return ProcessCompilationMessage(InMessage);
			});

#if WITH_EDITOR
			Graph->OnNodeSourceCompiledDelegate.Broadcast(Node, Diagnostics);
#endif
		}
	}
	else
	{
		// We may in general have kernels with no corresponding node.
		UE_LOG(LogPCG, Verbose, TEXT("Compilation message ignored for kernel index %d which has no associated node."), InKernelIndex);
	}
}

bool UPCGComputeGraph::ComputeKernelBindingDataDesc(int32 InBindingIndex, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComputeGraph::ComputeKernelBindingDataDesc);

	const FComputeGraphEdge& GraphEdge = GraphEdges[InBindingIndex];

#if PCG_DATA_DESCRIPTION_LOGGING
	UE_LOG(LogPCG, Warning, TEXT("Computing data desc for kernel %d, DI '%s', edge index %d (function '%s'), %s"),
		GraphEdge.KernelIndex,
		*DataInterfaces[GraphEdge.DataInterfaceIndex]->GetName(),
		InBindingIndex,
		*GraphEdge.BindingFunctionNameOverride,
		GraphEdge.bKernelInput ? TEXT("INPUT") : TEXT("OUTPUT"));
#endif

	// Find out if this data is coming from CPU (either from CPU nodes or from a separate compute graph).
	if (const FName* VirtualLabel = CPUDataBindingToVirtualPinLabel.Find(InBindingIndex))
	{
		// Create description from compute graph element input data.
		ComputeExternalPinDesc(*VirtualLabel, InBinding, OutDataDesc);

		return true;
	}
	else if (GraphEdge.bKernelInput)
	{
		const int32* UpstreamBindingIndex = DownstreamToUpstreamBinding.Find(InBindingIndex);
		return ensure(UpstreamBindingIndex) && ComputeKernelBindingDataDesc(*UpstreamBindingIndex, InBinding, OutDataDesc);
	}
	else
	{
		// Compute description of data from output binding. Data doesn't come from CPU but via a kernel. Consult the kernel for its data description.
		check(KernelInvocations.IsValidIndex(GraphEdge.KernelIndex));

		const UPCGComputeKernel* Kernel = CastChecked<UPCGComputeKernel>(KernelInvocations[GraphEdge.KernelIndex]);
		const FName* OutputPinLabel = KernelBindingToPinLabel.Find(InBindingIndex);

		return ensure(OutputPinLabel) && Kernel->ComputeOutputBindingDataDesc(this, *OutputPinLabel, InBinding, OutDataDesc);
	}
}

int32 UPCGComputeGraph::GetBindingIndex(const FPCGKernelPin& InKernelPin) const
{
	// Bindings in compute graphs roughly map to a single API on a Data Interface, like GetNumData(). There can be multiple bindings per PCG edge.
	// We choose the first binding index to represent the kernel pin.
	if (const int32* BindingIndex = KernelPinToFirstBinding.Find(InKernelPin))
	{
		return *BindingIndex;
	}
	else
	{
		ensure(false);
		return INDEX_NONE;
	}
}

void UPCGComputeGraph::GetKernelPins(TArray<FPCGKernelPin>& OutKernelPins) const
{
	KernelPinToFirstBinding.GenerateKeyArray(OutKernelPins);
}

bool UPCGComputeGraph::AreGraphSettingsValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComputeGraph::AreGraphSettingsValid);

	bool bAllKernelsValid = true;

	// Run validation on all kernels.
	for (const UComputeKernel* Kernel : KernelInvocations)
	{
		bAllKernelsValid &= CastChecked<UPCGComputeKernel>(Kernel)->AreKernelSettingsValid(InContext);
	}

	return bAllKernelsValid;
}

bool UPCGComputeGraph::IsGraphDataValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGComputeGraph::IsGraphDataValid);

	bool bAllKernelsValid = true;

	// Run validation on all kernels.
	for (const UComputeKernel* Kernel : KernelInvocations)
	{
		bAllKernelsValid &= CastChecked<UPCGComputeKernel>(Kernel)->IsKernelDataValid(InContext);
	}

	return bAllKernelsValid;
}

void UPCGComputeGraph::DebugLogDataDescriptions(const UPCGDataBinding* InBinding) const
{
#if PCG_KERNEL_LOGGING_ENABLED
	const UEnum* PCGDataTypeEnum = StaticEnum<EPCGDataType>();
	const UEnum* PCGKernelAttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();
	check(PCGDataTypeEnum && PCGKernelAttributeTypeEnum);

	auto LogDataDescription = [this, PCGDataTypeEnum, PCGKernelAttributeTypeEnum, InBinding](const FPCGDataCollectionDesc* InDataDesc, const FPCGKernelPin& InKernelPin)
	{
		if (!InDataDesc)
		{
			return;
		}

		for (int32 DataIndex = 0; DataIndex < InDataDesc->DataDescs.Num(); ++DataIndex)
		{
			const FPCGDataDesc& DataDesc = InDataDesc->DataDescs[DataIndex];

			UE_LOG(LogPCG, Display, TEXT("\t\tData Index %d"), DataIndex);
			UE_LOG(LogPCG, Display, TEXT("\t\t\tType: %s"), *PCGDataTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(DataDesc.Type)).ToString());
			
			if (DataDesc.IsDomain2D())
			{
				UE_LOG(LogPCG, Display, TEXT("\t\t\tNum Elements: (%d, %d)"), DataDesc.ElementCount2D.X, DataDesc.ElementCount2D.Y);
			}
			else
			{
				UE_LOG(LogPCG, Display, TEXT("\t\t\tNum Elements: %d"), DataDesc.ElementCount);
			}

			UE_LOG(LogPCG, Display, TEXT("\t\t\tAttributes (%d)"), DataDesc.AttributeDescs.Num());

			TSharedPtr<const FPCGProxyForGPUDataCollection> UpstreamDataCollectionInfo;

			if (const FName* VirtualLabel = GetVirtualPinLabelFromBindingIndex(GetBindingIndex(InKernelPin)))
			{
				TArray<FPCGTaggedData> PinInputs = InBinding->InputDataCollection.GetInputsByPin(*VirtualLabel);

				if (ensure(PinInputs.IsValidIndex(DataIndex)))
				{
					if (const UPCGProxyForGPUData* Proxy = Cast<UPCGProxyForGPUData>(PinInputs[DataIndex].Data))
					{
						UpstreamDataCollectionInfo = Proxy->GetGPUInfo();
					}
				}
			}

			for (int32 AttributeIndex = 0; AttributeIndex < DataDesc.AttributeDescs.Num(); ++AttributeIndex)
			{
				const FPCGKernelAttributeDesc& AttributeDesc = DataDesc.AttributeDescs[AttributeIndex];

				// If the data comes from an upstream / source buffer then look up its info.
				FString UniqueStringKeys;
				for (int32 GraphStringKey : AttributeDesc.GetUniqueStringKeys())
				{
					UniqueStringKeys += TEXT(", ");
					UniqueStringKeys += FString::Format(TEXT("{0}"), { GraphStringKey });

					if (const int32* UpstreamStringKey = UpstreamDataCollectionInfo ? InBinding->GetGraphToSourceBufferStringKey().Find({ UpstreamDataCollectionInfo->GetBuffer(), GraphStringKey }) : nullptr)
					{
						UniqueStringKeys += FString::Format(TEXT(" ({0})"), { *UpstreamStringKey });
					}
				}

				UE_LOG(LogPCG, Display, TEXT("\t\t\t\tID: %d\t\tName: %s\t\tType: %s\t\tUniqueStringKeys%s"),
					AttributeDesc.AttributeId,
					*AttributeDesc.AttributeKey.Identifier.ToString(),
					*PCGKernelAttributeTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(AttributeDesc.AttributeKey.Type)).ToString(),
					UniqueStringKeys.IsEmpty() ? TEXT(": ") : *UniqueStringKeys);
			}

			UE_LOG(LogPCG, Display, TEXT("\t\t\tTags (%d)"), DataDesc.TagStringKeys.Num());

			FString TagStringKeys;
			for (int32 TagStringKey : DataDesc.TagStringKeys)
			{
				TagStringKeys += TEXT(", ");
				TagStringKeys += FString::Format(TEXT("{0}"), { TagStringKey });
			}

			UE_LOG(LogPCG, Display, TEXT("\t\t\tTag String Keys%s"), *TagStringKeys);
		}
	};

	for (int32 KernelIndex = 0; KernelIndex < KernelInvocations.Num(); ++KernelIndex)
	{
		const UPCGComputeKernel* PCGKernel = Cast<UPCGComputeKernel>(KernelInvocations[KernelIndex]);

		if (PCGKernel && PCGKernel->GetLogDataDescriptions())
		{
			UE_LOG(LogPCG, Display, TEXT("Kernel: %s"), *PCGKernel->GetName());

			TArray<FPCGPinProperties> InputPins;
			PCGKernel->GetInputPins(InputPins);
			for (const FPCGPinProperties& Pin : InputPins)
			{
				if (Pin.Label != NAME_None)
				{
					if (const FPCGDataCollectionDesc* DataDesc = InBinding->GetCachedKernelPinDataDesc(PCGKernel, Pin.Label, true))
					{
						UE_LOG(LogPCG, Display, TEXT("\tInput Pin: %s (%d data)"), *Pin.Label.ToString(), DataDesc->DataDescs.Num());

						LogDataDescription(DataDesc, FPCGKernelPin(KernelIndex, Pin.Label, true));
					}
				}
			}

			TArray<FPCGPinPropertiesGPU> OutputPins;
			PCGKernel->GetOutputPins(OutputPins);
			for (const FPCGPinPropertiesGPU& Pin : OutputPins)
			{
				if (Pin.Label != NAME_None)
				{
					if (const FPCGDataCollectionDesc* DataDesc = InBinding->GetCachedKernelPinDataDesc(PCGKernel, Pin.Label, false))
					{
						UE_LOG(LogPCG, Display, TEXT("\tOutput Pin: %s (%d data)"), *Pin.Label.ToString(), DataDesc->DataDescs.Num());

						LogDataDescription(DataDesc, FPCGKernelPin(KernelIndex, Pin.Label, false));
					}
				}
			}
		}
	}
#endif
}

void UPCGComputeGraph::ComputeExternalPinDesc(FName InVirtualLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InBinding);

	const TArray<FPCGTaggedData> TaggedDatas = InBinding->InputDataCollection.GetInputsByPin(InVirtualLabel);

	for (const FPCGTaggedData& TaggedData : TaggedDatas)
	{
		if (!TaggedData.Data || !PCGComputeHelpers::IsTypeAllowedAsInput(TaggedData.Data->GetDataType()))
		{
			continue;
		}

		FPCGDataDesc DataDesc;
		if (ensure(ComputeTaggedDataPinDesc(TaggedData, InBinding, DataDesc)))
		{
			OutDataDesc.DataDescs.Add(MoveTemp(DataDesc));
		}
	}
}

bool UPCGComputeGraph::ComputeTaggedDataPinDesc(const FPCGTaggedData& InTaggedData, const UPCGDataBinding* InBinding, FPCGDataDesc& OutDescription) const
{
	if (const UPCGProxyForGPUData* Proxy = Cast<UPCGProxyForGPUData>(InTaggedData.Data))
	{
		if (!Proxy->GetDescription(OutDescription))
		{
			return false;
		}

		if (TSharedPtr<const FPCGProxyForGPUDataCollection> DataGPU = Proxy->GetGPUInfo())
		{
			// Remap attribute IDs and string keys.
			for (FPCGKernelAttributeDesc& AttrDesc : OutDescription.AttributeDescs)
			{
				const int32 Index = PCGComputeHelpers::GetMetadataAttributeIndexFromAttributeId(AttrDesc.AttributeId);
				if (Index == INDEX_NONE)
				{
					continue;
				}

				if (const int32* AttributeIndex = InBinding->GetAttributeIdRemap().Find(UPCGDataBinding::FSourceBufferAndAttributeIndex(DataGPU, Index)))
				{
					const int32 NewId = PCGComputeHelpers::GetAttributeIdFromMetadataAttributeIndex(*AttributeIndex);
					//UE_LOG(LogPCG, Warning, TEXT("Remapped attribute '%s' ID: %d -> %d"), *AttrDesc.Name.ToString(), AttrDesc.Index, NewId);
					AttrDesc.AttributeId = NewId;
				}

				// Map from the attribute IDs in the buffer / upstream compute graph to attribute IDs usable in the current graph.
				if (!InBinding->GetSourceBufferToGraphStringKey().IsEmpty())
				{
					TArray<int32> GraphStringKeys;
					GraphStringKeys.Reserve(AttrDesc.GetUniqueStringKeys().Num());
					bool bStringKeyRemapped = false;

					for (int32 SourceStringKey : AttrDesc.GetUniqueStringKeys())
					{
						if (const int32* GraphStringKey = InBinding->GetSourceBufferToGraphStringKey().Find({ DataGPU->GetBuffer(), SourceStringKey }))
						{
							//UE_LOG(LogPCG, Warning, TEXT("Remapped string key: %d -> %d"), SourceStringKey, *GraphStringKey);
							GraphStringKeys.AddUnique(*GraphStringKey);
							bStringKeyRemapped = true;
						}
						else
						{
							GraphStringKeys.AddUnique(SourceStringKey);
						}
					}

					if (bStringKeyRemapped)
					{
						AttrDesc.SetStringKeys(GraphStringKeys);
					}
				}
			}

			// Remap tag string keys.
			for (int32& TagStringKey : OutDescription.TagStringKeys)
			{
				if (const int32* GraphStringKey = InBinding->GetSourceBufferToGraphStringKey().Find({ DataGPU->GetBuffer(), TagStringKey }))
				{
					TagStringKey = *GraphStringKey;
				}
			}
		}
	}
	else
	{
		OutDescription = FPCGDataDesc(InTaggedData, InBinding);
	}

	return true;
}

#undef PCG_DATA_DESCRIPTION_LOGGING 

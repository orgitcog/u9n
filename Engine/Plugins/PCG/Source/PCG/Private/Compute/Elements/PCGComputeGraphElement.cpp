// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGComputeGraphElement.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "Components/PCGProceduralISMComponent.h"
#include "Compute/PCGComputeCommon.h"
#include "Data/PCGTextureData.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"
#include "Elements/PCGStaticMeshSpawnerKernel.h"
#include "Editor/IPCGEditorModule.h"
#include "Graph/PCGGraphCompiler.h"
#include "Helpers/PCGHelpers.h"

#include "ComputeWorkerInterface.h"
#include "ComputeFramework/ComputeFramework.h"
#include "ComputeFramework/ComputeKernelCompileResult.h"
#include "Engine/World.h"
#include "Logging/LogVerbosity.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGComputeGraphElement)

#define LOCTEXT_NAMESPACE "PCGComputeGraphElement"

#if WITH_EDITOR
static TAutoConsoleVariable<bool> CVarReadbackTextureDataOnInspect(
	TEXT("pcg.GPU.ReadbackTextureDataOnInspect"),
	true,
	TEXT("Reads texture data back to the CPU when inspected."));
#endif

FPCGComputeGraphContext::~FPCGComputeGraphContext()
{
	const FPCGStack* StackPtr = GetStack();
	TWeakObjectPtr<const UPCGGraph> GraphWeak = StackPtr ? StackPtr->GetNearestNonInlinedGraphForCurrentFrame() : nullptr;

	// Can be here on a worker thread, after unpinning the context in one of the async tasks.
	// Will execute immediately if on game thread, otherwise on next game thread frame.
	PCGHelpers::ExecuteOnGameThread(TEXT("~FPCGComputeGraphContext"), [DataBinding=DataBinding, GraphInstance=ComputeGraphInstance, bInstanceInitialized=bComputeGraphInstanceInitialized, GraphWeak, GridSize=GenerationGridSize, GraphIndex=ComputeGraphIndex]
	{
		if (DataBinding)
		{
			DataBinding->ReleaseTransientResources();
		}

		if (GraphInstance.IsValid())
		{
			for (TObjectPtr<UComputeDataProvider>& Provider : GraphInstance->GetDataProviders())
			{
				if (UPCGComputeDataProvider* PCGDataProvider = Cast<UPCGComputeDataProvider>(Provider.Get()))
				{
					PCGDataProvider->ReleaseTransientResources(TEXT("~FPCGComputeGraphContext"));
				}
			}

			if (bInstanceInitialized)
			{
				GraphInstance->ResetDataProviders();

				// Return instance to pool.
				if (ensure(GraphWeak.IsValid()) && GridSize != PCGHiGenGrid::UninitializedGridSize())
				{
					const UPCGGraph::FComputeGraphInstanceKey Key = { GridSize, GraphIndex };
					GraphWeak->ReturnComputeGraphInstanceToPool(Key, GraphInstance);
				}
			}
		}
	});
}

bool FPCGComputeGraphContext::HasPendingAsyncOperations() const
{
	return !ProvidersWithBufferExports.IsEmpty() || !ProvidersWithReadbacks.IsEmpty();
}

void FPCGComputeGraphContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	if (bComputeGraphInstanceInitialized)
	{
		Collector.AddPropertyReferences(FComputeGraphInstance::StaticStruct(), ComputeGraphInstance.Get());
	}

	for (TObjectPtr<UComputeDataProvider>& Provider : ProvidersWithBufferExports)
	{
		Collector.AddReferencedObject(Provider);
	}

	Collector.AddReferencedObject(DataBinding);
	Collector.AddReferencedObject(ComputeGraph);
}

#if WITH_EDITOR
bool FPCGComputeGraphElement::operator==(const FPCGComputeGraphElement& Other) const
{
	return ComputeGraphIndex == Other.ComputeGraphIndex;
}
#endif

bool FPCGComputeGraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGComputeGraphElement::ExecuteInternal);
	check(InContext);

	if (!InContext->ExecutionSource.IsValid())
	{
		UE_LOG(LogPCG, Warning, TEXT("FPCGComputeGraphElement: Execution source lost, element execution halted."));
		return true;
	}

	FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);

	auto SleepUntilNextFrame = [Context]()
	{
		Context->bIsPaused = true;
		FPCGModule::GetPCGModuleChecked().ExecuteNextTick([ContextHandle = Context->GetOrCreateHandle()]()
		{
			if (TSharedPtr<FPCGContextHandle> SharedHandle = ContextHandle.Pin())
			{
				if (FPCGContext* ContextPtr = SharedHandle->GetContext())
				{
					ContextPtr->bIsPaused = false;
				}
			}
		});
	};

	switch (Context->ExecutionSubPhase)
	{
	case EPCGComputeGraphExecutionPhase::GetComputeGraph:
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetComputeGraph);

		const IPCGGraphExecutionSource* Source = InContext->ExecutionSource.Get();
		const UPCGGraph* TopGraph = Source ? Source->GetExecutionState().GetGraph() : nullptr;
		const FPCGStack* StackPtr = Context->GetStack();
		if (!ensure(TopGraph) || !ensure(StackPtr))
		{
			return true;
		}

		// Higen is always disabled within dynamic subgraphs - will retrieve Uninitialized tasks (which are always cooked).
		const UPCGGraph* DynamicSubgraph = StackPtr ? StackPtr->GetNearestDynamicSubgraphForCurrentFrame() : nullptr;

		if (TopGraph->IsHierarchicalGenerationEnabled() && !DynamicSubgraph)
		{
			if (const UPCGComponent* Component = Cast<UPCGComponent>(Source))
			{
				if (Component->IsLocalComponent() || Component->IsPartitioned())
				{
					Context->GenerationGridSize = Component->GetGenerationGridSize();
				}
			}
		}

		if (UPCGSubsystem* Subsystem = UWorld::GetSubsystem<UPCGSubsystem>(Source->GetExecutionState().GetWorld()))
		{
			Context->ComputeGraph = Subsystem->GetComputeGraph(DynamicSubgraph ? DynamicSubgraph : TopGraph, Context->GenerationGridSize, ComputeGraphIndex);
		}

		if (!ensure(Context->ComputeGraph))
		{
			return true;
		}

		Context->ComputeGraphIndex = ComputeGraphIndex;

		if (!Context->ComputeGraph->AreGraphSettingsValid(Context))
		{
			return true;
		}

		Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::InitializeDataBindingAndComputeGraph;

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}
	case EPCGComputeGraphExecutionPhase::InitializeDataBindingAndComputeGraph: // Fallthrough
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(InitializeDataBindingAndComputeGraph);
		ensure(!Context->DataBinding);

		UPCGDataBinding* DataBinding = FPCGContext::NewObject_AnyThread<UPCGDataBinding>(Context);
		Context->DataBinding = DataBinding;

		DataBinding->Initialize(Context->ComputeGraph.Get(), Context);

		// Start data binding full initialization task which can run concurrently with initializing data providers.
		UE::Tasks::TTask<void> InitializeBindingTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [ContextHandle = Context->GetOrCreateHandle()]()
		{
			FPCGContext::FSharedContext<FPCGComputeGraphContext> SharedContext(ContextHandle);
			FPCGComputeGraphContext* Context = SharedContext.Get();
			if (!Context)
			{
				return;
			}

			Context->DataBinding->InitializeTables(Context);
		});

		const FPCGStack* StackPtr = Context->GetStack();
		const UPCGGraph* Graph = StackPtr ? StackPtr->GetNearestNonInlinedGraphForCurrentFrame() : nullptr;
		const UPCGGraph::FComputeGraphInstanceKey Key = { Context->GenerationGridSize, ComputeGraphIndex };

		if (!ensure(Graph))
		{
			return true;
		}

		bool bNewInstance = false;
		Context->ComputeGraphInstance = Graph->RetrieveComputeGraphInstanceFromPool(Key, bNewInstance);

		// The data provider initialization must not depend on the data binding being set up, although they still output of the PreInitialize such
		// as the SourceComponent.
		if (bNewInstance)
		{
			Context->ComputeGraphInstance->CreateDataProviders(Context->ComputeGraph.Get(), 0, Context->DataBinding.Get());
		}
		else
		{
			Context->ComputeGraphInstance->InitializeDataProviders(Context->ComputeGraph.Get(), 0, Context->DataBinding.Get());
		}

		Context->bComputeGraphInstanceInitialized = true;

		// Register all providers running async operations.
		for (UComputeDataProvider* ComputeDataProvider : Context->ComputeGraphInstance->GetDataProviders())
		{
			UPCGExportableDataProvider* DataProvider = Cast<UPCGExportableDataProvider>(ComputeDataProvider);
			if (!DataProvider)
			{
				continue;
			}

			const bool bDataProviderDebugged = DataProvider->GetProducerSettings() && DataProvider->GetProducerSettings()->bDebug;

			if (DataProvider->IsExportRequired() || bDataProviderDebugged)
			{
				Context->ProvidersWithBufferExports.Add(DataProvider);

				TWeakObjectPtr<UPCGExportableDataProvider> DataProviderWeak = DataProvider;

				const uint64 OriginatingGenerationCount = DataProvider->GenerationCounter;

				DataProvider->OnDataExported_GameThread().AddLambda([ContextHandle = Context->GetOrCreateHandle(), DataProviderWeak, GenerationCount = OriginatingGenerationCount]()
				{
					check(IsInGameThread());

					TSharedPtr<FPCGContextHandle> SharedHandle = ContextHandle.Pin();
					FPCGComputeGraphContext* ContextPtr = static_cast<FPCGComputeGraphContext*>(SharedHandle ? SharedHandle->GetContext() : nullptr);
					UPCGExportableDataProvider* DataProvider = DataProviderWeak.Get();

					if (!DataProvider || !ContextPtr || GenerationCount != DataProvider->GenerationCounter)
					{
						// Safe to just jump out. The GPU buffer is ref counted.
						return;
					}

					ContextPtr->ProvidersWithBufferExports.Remove(DataProvider);

					if (!ContextPtr->HasPendingAsyncOperations())
					{
						ContextPtr->bIsPaused = false;
					}
				});
			}

			UPCGDataCollectionDataProvider* DataCollectionDataProvider = Cast<UPCGDataCollectionDataProvider>(ComputeDataProvider);

			if (DataCollectionDataProvider && DataCollectionDataProvider->bExposeElementCounters)
			{
				Context->ProvidersWithReadbacks.Add(DataCollectionDataProvider);

				TWeakObjectPtr<UPCGDataCollectionDataProvider> DataProviderWeak = DataCollectionDataProvider;

				DataCollectionDataProvider->OnCountersReadBack_GameThread().AddLambda([ContextHandle = Context->GetOrCreateHandle(), DataProviderWeak]()
				{
					check(IsInGameThread());

					TSharedPtr<FPCGContextHandle> SharedHandle = ContextHandle.Pin();
					FPCGComputeGraphContext* ContextPtr = static_cast<FPCGComputeGraphContext*>(SharedHandle ? SharedHandle->GetContext() : nullptr);
					UPCGDataCollectionDataProvider* DataProvider = DataProviderWeak.Get();
					if (!DataProvider || !ContextPtr)
					{
						return;
					}

					ensure(ContextPtr->ProvidersWithReadbacks.Contains(DataProvider));
					ContextPtr->ProvidersWithReadbacks.Remove(DataProvider);

					if (!ContextPtr->HasPendingAsyncOperations())
					{
						ContextPtr->bIsPaused = false;
					}
				});
			}
		}

		InitializeBindingTask.Wait();

		Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::PreExecuteReadbacks;

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}
	case EPCGComputeGraphExecutionPhase::PreExecuteReadbacks: // Fallthrough
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PreExecuteReadbacks);

		bool bAllReady = true;

		for (UComputeDataProvider* ComputeDataProvider : Context->ComputeGraphInstance->GetDataProviders())
		{
			if (UPCGComputeDataProvider* DataProvider = Cast<UPCGComputeDataProvider>(ComputeDataProvider))
			{
				if (!DataProvider->PerformPreExecuteReadbacks_GameThread(Context->DataBinding.Get()))
				{
					bAllReady = false;
				}
			}
		}

		if (bAllReady)
		{
			Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::PrimeDataDescriptionsAndValidateData;
		}
		else
		{
			SleepUntilNextFrame();
			return false;
		}

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}
	case EPCGComputeGraphExecutionPhase::PrimeDataDescriptionsAndValidateData: // Fallthrough
	{
		// Note: Priming the data description cache must take place after pre-execute readbacks, as data descriptions
		// may rely on the readback data, e.g. analysis kernels.
		if (!Context->bDataDescrPrimeAndValidateScheduled)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PrimeDataDescriptionsAndValidateData);

			Context->bDataDescrPrimeAndValidateScheduled = true;

			// @todo_pcg: In the future perhaps we can just use Context->ScheduleGeneric(), but it's unclear at the moment if there is any guarantee this task
			// would be kicked off this tick. To avoid the unknown, we'll just launch a task directly here, but it should be investigated in the future.
			Context->DataDescrPrimeAndValidateTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [ContextHandle = Context->GetOrCreateHandle()]()
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(DataDescrPrimeAndValidateTask);

				FPCGContext::FSharedContext<FPCGComputeGraphContext> SharedContext(ContextHandle);
				FPCGComputeGraphContext* Context = SharedContext.Get();
				if (!Context)
				{
					return;
				}

				Context->DataBinding->PrimeDataDescriptionCache();

				// Graph data validation is currently allowed to depend on data descriptions, hence done serially here.
				Context->bGraphValid = Context->ComputeGraph->IsGraphDataValid(Context);
			});

			return false;
		}

		if (!ensure(Context->DataDescrPrimeAndValidateTask.IsValid()))
		{
			return true;
		}

		if (!Context->DataDescrPrimeAndValidateTask.IsCompleted())
		{
			SleepUntilNextFrame();
			return false;
		}

		Context->DataBinding->DebugLogDataDescriptions();

		if (!Context->bGraphValid)
		{
			return true;
		}

		Algo::Transform(Context->ComputeGraphInstance->GetDataProviders(), Context->DataProvidersPendingReadyForExecute, [](UComputeDataProvider* InProvider)
		{
			return Cast<UPCGComputeDataProvider>(InProvider);
		});

		Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::PrepareForExecute;

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}
	case EPCGComputeGraphExecutionPhase::PrepareForExecute: // Fallthrough
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PrepareForExecute);

		for (int Index = Context->DataProvidersPendingReadyForExecute.Num() - 1; Index >= 0; --Index)
		{
			UPCGComputeDataProvider* DataProvider = Context->DataProvidersPendingReadyForExecute[Index];

			if (!DataProvider || DataProvider->PrepareForExecute_GameThread(Context->DataBinding.Get()))
			{
				Context->DataProvidersPendingReadyForExecute.RemoveAtSwap(Index);
			}
		}

		if (Context->DataProvidersPendingReadyForExecute.IsEmpty())
		{
			Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::ValidateComputeGraphCompilation;
		}
		else
		{
			SleepUntilNextFrame();
			return false;
		}

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}
	case EPCGComputeGraphExecutionPhase::ValidateComputeGraphCompilation: // Fallthrough
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ValidateComputeGraphCompilation);

		if (Context->ComputeGraph->HasKernelResourcesPendingShaderCompilation())
		{
			UE_LOG(LogPCG, Verbose, TEXT("Deferring until next frame as the kernel has pending shader compilations."));
			SleepUntilNextFrame();
			return false;
		}
		else if (!Context->ComputeGraph->GetRenderProxy())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Context->ComputeGraph->UpdateResources);

			Context->ComputeGraph->UpdateResources();

			SleepUntilNextFrame();
			return false;
		}
		else
		{
			// Add any messages that may have occurred during compilation to visual logs.
#if WITH_EDITOR
			LogCompilationMessages(Context);
#endif

			// If there was any error then we should abort.
			using FNodeAndCompileMessages = const TPair<TObjectKey<const UPCGNode>, TArray<FComputeKernelCompileMessage>>;
			for (FNodeAndCompileMessages& NodeAndCompileMessages : Context->ComputeGraph->KernelToCompileMessages)
			{
				for (const FComputeKernelCompileMessage& Message : NodeAndCompileMessages.Get<1>())
				{
					// Some error messages were getting lost, and we were only getting the final 'failed' message. Treat this as failure and report for now.
					// TODO: Revert the 'failed' part once we're happy all relevant issues are bubbling up.
					if (Message.Type == FComputeKernelCompileMessage::EMessageType::Error || Message.Text.Contains(TEXT("failed"), ESearchCase::IgnoreCase))
					{
						return true;
					}
				}
			}
		}

		Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::ScheduleComputeGraph;

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}
	case EPCGComputeGraphExecutionPhase::ScheduleComputeGraph: // Fallthrough
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ScheduleComputeGraph);
		IPCGGraphExecutionSource* ExecutionSource = Context->ExecutionSource.Get();
		check(ExecutionSource&& ExecutionSource->GetExecutionState().GetWorld());

		const bool bGraphEnqueued = Context->ComputeGraphInstance->EnqueueWork(
			Context->ComputeGraph.Get(),
			InContext->ExecutionSource->GetExecutionState().GetWorld()->Scene,
			ComputeTaskExecutionGroup::EndOfFrameUpdate,
			FName(*InContext->ExecutionSource->GetExecutionState().GetDebugName()),
			/*InFallbackDelegate=*/FSimpleDelegate::CreateLambda([ContextHandle = Context->GetOrCreateHandle()]()
			{
				// This render thread delegate will be executed if SubmitWork fails at any stage.
				FPCGContext::FSharedContext<FPCGComputeGraphContext> SharedContext(ContextHandle);
				FPCGComputeGraphContext* Context = SharedContext.Get();
				if (!Context)
				{
					return;
				}

				// Wake up so that we can terminate execution of the ComputeGraphElement.
				Context->bIsPaused = false;
				Context->bGraphSubmitFailed = true;
			}));

		if (!bGraphEnqueued)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("EnqueueFailed", "Compute graph enqueue failed, check log for errors."));
			ResetAsyncOperations(InContext);
			return true;
		}

		Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::WaitForExecutionComplete;

		SleepUntilNextFrame();

		return false;
	}
	case EPCGComputeGraphExecutionPhase::WaitForExecutionComplete:
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaitForExecutionComplete);

		if (Context->bGraphSubmitFailed)
		{
			UE_LOG(LogPCG, Warning, TEXT("Submit of compute graph '%s' failed."), *Context->ComputeGraph->GetName());
			return true;
		}

		if (Context->HasPendingAsyncOperations())
		{
			// Still running. Likely we need a frame to pass in order to make progress with readbacks etc.
			SleepUntilNextFrame();

			return false;
		}

		Context->bExecutionSuccess = true;

		// Apply any read back element counts to the final data description.
		Context->DataBinding->ApplyElementCounts_GameThread();

		Context->DataProvidersPendingPostExecute.Reserve(Context->ComputeGraphInstance->GetNumDataProviders());
		for (UComputeDataProvider* Provider : Context->ComputeGraphInstance->GetDataProviders())
		{
			UPCGComputeDataProvider* PCGProvider = Cast<UPCGComputeDataProvider>(Provider);
			if (PCGProvider)
			{
				Context->DataProvidersPendingPostExecute.Add(PCGProvider);
			}
		}

		Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::PostExecute;

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}
	case EPCGComputeGraphExecutionPhase::PostExecute: // Fallthrough
	{
		for (int Index = Context->DataProvidersPendingPostExecute.Num() - 1; Index >= 0; --Index)
		{
			UPCGComputeDataProvider* DataProvider = Context->DataProvidersPendingPostExecute[Index];

			if (!DataProvider || DataProvider->PostExecute(Context->DataBinding.Get()))
			{
				Context->DataProvidersPendingPostExecute.RemoveAtSwap(Index);
			}
		}

		if (!Context->DataProvidersPendingPostExecute.IsEmpty())
		{
			SleepUntilNextFrame();
			return false;
		}

		// Currently we don't output anything if processing any readback data processing failed.
		if (ensure(Context->bExecutionSuccess) && ensure(Context->DataBinding))
		{
			Context->OutputData = Context->DataBinding->OutputDataCollection;
		}

		Context->ExecutionSubPhase = EPCGComputeGraphExecutionPhase::DebugAndInspection;
	}
	case EPCGComputeGraphExecutionPhase::DebugAndInspection: // Fallthrough
	{
#if WITH_EDITOR
		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}

		CollectDebugDataPrepareActions(Context);

		bool bAllReady = true;

		for (int32 Index = Context->DebugDataPrepareActions.Num() - 1; Index >= 0; --Index)
		{
			FPCGComputeGraphContext::FDebugDataPrepareAction Action = Context->DebugDataPrepareActions[Index];
			const bool bActionIsDone = !Action.IsSet() || Action(Context);

			bAllReady &= bActionIsDone;

			if (bActionIsDone)
			{
				Context->DebugDataPrepareActions.RemoveAtSwap(Index);
			}
		}

		if (!bAllReady)
		{
			SleepUntilNextFrame();
			return false;
		}

		ExecuteDebugDraw(Context);
		StoreDataForInspection(Context);
#endif
	}
	}

	return true;
}

void FPCGComputeGraphElement::PostExecuteInternal(FPCGContext* InContext) const
{
	check(InContext);
	FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);
	const FPCGStack* Stack = Context->GetStack();
	if (!Context->DataBinding || !ensure(Stack))
	{
		return;
	}

#if WITH_EDITOR
	if (Context->bExecutionSuccess)
	{
		for (TSoftObjectPtr<const UPCGNode> NodePtr : Context->DataBinding->Graph->KernelToNode)
		{
			const UPCGNode* Node = NodePtr.Get();
			IPCGGraphExecutionSource* ExecutionSource = Context->ExecutionSource.Get();
			if (ExecutionSource && Stack && Node)
			{
				ExecutionSource->GetExecutionState().GetInspection().NotifyNodeExecuted(Node, Stack, /*InTimer=*/nullptr, /*bUsedCache*/false);
			}
		}
	}
#endif
}

void FPCGComputeGraphElement::AbortInternal(FPCGContext* InContext) const
{
	ResetAsyncOperations(InContext);
}

void FPCGComputeGraphElement::ResetAsyncOperations(FPCGContext* InContext) const
{
	check(IsInGameThread());

	if (InContext)
	{
		FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);
		Context->ProvidersWithBufferExports.Reset();
		Context->ProvidersWithReadbacks.Reset();
	}
}

#if WITH_EDITOR
void FPCGComputeGraphElement::CollectDebugDataPrepareActions(FPCGComputeGraphContext* InContext) const
{
	if (!InContext || !InContext->DataBinding)
	{
		return;
	}

	const bool bReadbackTextureDataOnInspect = CVarReadbackTextureDataOnInspect.GetValueOnGameThread();

	auto CollectDebugDataPrepareAction = [InContext, bReadbackTextureDataOnInspect](FPCGDataToDebug& DataToDebug, bool bInIsInspecting)
	{
		if (const UPCGProxyForGPUData* ProxyData = Cast<UPCGProxyForGPUData>(DataToDebug.Data.Get()))
		{
			InContext->DebugDataPrepareActions.Add([&DataToDebug](FPCGComputeGraphContext* InContext)
			{
				const UPCGProxyForGPUData* ProxyData = CastChecked<UPCGProxyForGPUData>(DataToDebug.Data.Get());
				UPCGProxyForGPUData::FReadbackResult Readback = ProxyData->GetCPUData(InContext);
				return Readback.bComplete;
			});
		}
		else if (const UPCGTextureData* TextureData = Cast<UPCGTextureData>(DataToDebug.Data.Get()))
		{
			InContext->DebugDataPrepareActions.Add([&DataToDebug, bInIsInspecting, bReadbackTextureDataOnInspect](FPCGComputeGraphContext* InContext)
			{
				if (bInIsInspecting && !bReadbackTextureDataOnInspect)
				{
					return true;
				}

				// Duplicate and CPU-initialize the debug texture data.
				const UPCGTextureData* TextureData = CastChecked<UPCGTextureData>(DataToDebug.Data.Get());
				TObjectPtr<UPCGTextureData> DuplicateTextureData = Cast<UPCGTextureData>(DataToDebug.DataPendingInit);

				if (!DuplicateTextureData)
				{
					DuplicateTextureData = InContext->NewObject_AnyThread<UPCGTextureData>(InContext, GetTransientPackage());
					DuplicateTextureData->TexelSize = TextureData->TexelSize;
					DataToDebug.DataPendingInit = DuplicateTextureData;
				}

				return DuplicateTextureData->Initialize(TextureData->GetRefCountedTexture(), TextureData->TextureIndex, TextureData->GetTransform(), /*bInSkipReadbackToCPU=*/false);
			});
		}
	};

	for (FPCGDataToDebug& DataToDebug : InContext->DataBinding->DataToDebug)
	{
		CollectDebugDataPrepareAction(DataToDebug, /*bIsInspecting=*/false);
	}

	for (FPCGDataToDebug& DataToDebug : InContext->DataBinding->DataToInspect)
	{
		CollectDebugDataPrepareAction(DataToDebug, /*bIsInspecting=*/true);
	}
}

void FPCGComputeGraphElement::ExecuteDebugDraw(FPCGComputeGraphContext* InContext) const
{
	if (!InContext || !InContext->DataBinding)
	{
		return;
	}

	const FPCGDataVisualizationRegistry& DataVisRegistry = FPCGModule::GetConstPCGDataVisualizationRegistry();

	for (int Index = InContext->DataBinding->DataToDebug.Num() - 1; Index >= 0; --Index)
	{
		FPCGDataToDebug& DataToDebug = InContext->DataBinding->DataToDebug[Index];
		const UPCGData* DataToDisplay = nullptr;

		if (const UPCGProxyForGPUData* Proxy = Cast<UPCGProxyForGPUData>(DataToDebug.Data.Get()))
		{
			UPCGProxyForGPUData::FReadbackResult Readback = Proxy->GetCPUData(InContext);
			ensure(Readback.bComplete);

			DataToDisplay = Readback.TaggedData.Data.Get();
		}
		else if (const UPCGTextureData* Data = Cast<UPCGTextureData>(DataToDebug.Data.Get()))
		{
			DataToDisplay = DataToDebug.DataPendingInit;
		}

		InContext->DataBinding->DataToDebug.RemoveAtSwap(Index);

		const UPCGSettings* ProducerSettings = DataToDebug.ProducerSettings.Get();

		if (DataToDisplay && ProducerSettings)
		{
			if (const IPCGDataVisualization* DataVis = DataVisRegistry.GetDataVisualization(DataToDisplay->GetClass()))
			{
				DataVis->ExecuteDebugDisplay(InContext, ProducerSettings, DataToDisplay, InContext->GetTargetActor(nullptr));
			}
		}
	}
}

void FPCGComputeGraphElement::StoreDataForInspection(FPCGComputeGraphContext* InContext) const
{
	if (!InContext || !InContext->DataBinding || !InContext->ExecutionSource.IsValid())
	{
		return;
	}

	// Collect all data into a collection and store it.
	TMap<const UPCGSettings*, FPCGDataCollection> SettingsToDataCollection;

	for (FPCGDataToDebug& DataToInspect : InContext->DataBinding->DataToInspect)
	{
		if (const UPCGSettings* ProducerSettings = DataToInspect.ProducerSettings.Get())
		{
			FPCGDataCollection& DataCollection = SettingsToDataCollection.FindOrAdd(ProducerSettings);

			if (const UPCGProxyForGPUData* ProxyData = Cast<UPCGProxyForGPUData>(DataToInspect.Data.Get()))
			{
				UPCGProxyForGPUData::FReadbackResult Readback = ProxyData->GetCPUData(InContext);
				FPCGTaggedData TaggedData = Readback.TaggedData;
				TaggedData.Pin = DataToInspect.PinLabel;
				TaggedData.Tags.Append(DataToInspect.AdditionalTags);
				DataCollection.TaggedData.Add(TaggedData);
			}
			else if (const UPCGTextureData* TextureData = Cast<UPCGTextureData>(DataToInspect.Data.Get()))
			{
				const bool bReadbackTextureDataOnInspect = CVarReadbackTextureDataOnInspect.GetValueOnGameThread();

				FPCGTaggedData TaggedData;
				TaggedData.Data = bReadbackTextureDataOnInspect ? DataToInspect.DataPendingInit : DataToInspect.Data;
				TaggedData.Pin = DataToInspect.PinLabel;
				TaggedData.Tags = DataToInspect.AdditionalTags;
				DataCollection.TaggedData.Add(TaggedData);
			}
		}
	}

	for (TPair<const UPCGSettings*, FPCGDataCollection>& SettingsAndData : SettingsToDataCollection)
	{
		// Required by inspection code.
		SettingsAndData.Value.ComputeCrcs(/*bFullDataCrc=*/false);

		const UPCGNode* Node = Cast<UPCGNode>(SettingsAndData.Key->GetOuter());

		// TODO: Input data not yet supported.
		InContext->ExecutionSource->GetExecutionState().GetInspection().StoreInspectionData(InContext->GetStack(), Node, /*InTimer=*/nullptr, /*InInputData=*/{}, SettingsAndData.Get<1>(), /*bUsedCache*/false);
	}

	InContext->DataBinding->DataToInspect.Empty();
}

void FPCGComputeGraphElement::LogCompilationMessages(FPCGComputeGraphContext* InContext) const
{
	if (InContext->ExecutionSource.IsValid() && InContext->GetStack())
	{
		using FNodeAndCompileMessages = const TPair<TObjectKey<const UPCGNode>, TArray<FComputeKernelCompileMessage>>;
		for (FNodeAndCompileMessages& NodeAndCompileMessages : InContext->ComputeGraph->KernelToCompileMessages)
		{
			for (const FComputeKernelCompileMessage& Message : NodeAndCompileMessages.Get<1>())
			{
				// These messages already go to log. So just pick out the warnings and errors to display on graph. Need to convert
				// message type.
				ELogVerbosity::Type Verbosity = ELogVerbosity::All;
				if (Message.Type == FComputeKernelCompileMessage::EMessageType::Warning)
				{
					Verbosity = ELogVerbosity::Warning;
				}
				else if (Message.Type == FComputeKernelCompileMessage::EMessageType::Error)
				{
					Verbosity = ELogVerbosity::Error;
				}
				else if (Message.Text.Contains(TEXT("failed"), ESearchCase::IgnoreCase))
				{
					// Some error messages were getting lost, and we were only getting the final 'failed' message.
					// Treat this as failure and report for now.
					// TODO: Revert this once we're happy all relevant issues are bubbling up.
					Verbosity = ELogVerbosity::Error;
				}

				if (Verbosity < ELogVerbosity::Log)
				{
					if (IPCGEditorModule* PCGEditorModule = IPCGEditorModule::Get())
					{
						FPCGStack StackWithNode = *InContext->GetStack();
						StackWithNode.PushFrame(NodeAndCompileMessages.Get<0>().ResolveObjectPtr());

						FText LogText;

						if (Message.Line != INDEX_NONE)
						{
							if (Message.ColumnStart != INDEX_NONE)
							{
								LogText = FText::Format(LOCTEXT("ErrorWithLineColFormat", "[{0},{1}] {2}"), Message.Line, Message.ColumnStart, FText::FromString(Message.Text));
							}
							else
							{
								LogText = FText::Format(LOCTEXT("ErrorWithLineFormat", "[{0}] {1}"), Message.Line, FText::FromString(Message.Text));
							}
						}
						else
						{
							LogText = FText::FromString(Message.Text);
						}

						PCGEditorModule->GetNodeVisualLogsMutable().Log(StackWithNode, Verbosity, LogText);
					}
				}
			}
		}
	}
}
#endif

UPCGComputeGraphSettings::UPCGComputeGraphSettings()
{
#if WITH_EDITOR
	bExposeToLibrary = false;
#endif
}

FPCGElementPtr UPCGComputeGraphSettings::CreateElement() const
{
	return MakeShared<FPCGComputeGraphElement>(ComputeGraphIndex);
}

#undef LOCTEXT_NAMESPACE

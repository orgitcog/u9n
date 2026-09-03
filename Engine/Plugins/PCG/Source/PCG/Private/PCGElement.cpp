// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGElement.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGHiGenGridSize.h"
#include "Graph/PCGGraphCache.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#if WITH_EDITOR
#include "PCGDataVisualization.h"
#endif

#include "DrawDebugHelpers.h"
#include "EngineDefines.h" // For UE_ENABLE_DEBUG_DRAWING
#include "HAL/IConsoleManager.h"
#include "Utils/PCGExtraCapture.h"

#define LOCTEXT_NAMESPACE "PCGElement"

static TAutoConsoleVariable<bool> CVarPCGValidatePointMetadata(
	TEXT("pcg.debug.ValidatePointMetadata"),
	true,
	TEXT("Controls whether we validate that the metadata entry keys on the output point data are consistent"));

static TAutoConsoleVariable<bool> CVarPCGAllowPerDataCaching(
	TEXT("pcg.AllowPerDataCaching"),
	true,
	TEXT("Controls whether we test & split down inputs to check caching per input on primary loop nodes."));

static TAutoConsoleVariable<bool> CVarPCGShouldVerifyIfOutputsAreUsedMultipleTimes(
	TEXT("pcg.ShouldVerifyIfOutputsAreUsedMultipleTimes"),
	true,
	TEXT("Add small computation at the end of each node to detect if the data is used multiple times. Necessary for data stealing."));

static TAutoConsoleVariable<bool> CVarPCGEnablePointArrayDataToPointDataConversionWarnings(
	TEXT("pcg.EnablePointArrayDataToPointDataConversionWarnings"),
	false,
	TEXT("Warn about input conversions from PointArrayData to PointData so that code that needs to be updated is identified."));

#if WITH_EDITOR
#define PCG_ELEMENT_EXECUTION_BREAKPOINT() \
	if (Context && Context->GetInputSettingsInterface() && Context->GetInputSettingsInterface()->bBreakDebugger) \
	{ \
		UE_DEBUG_BREAK(); \
	}
#else
#define PCG_ELEMENT_EXECUTION_BREAKPOINT()
#endif

namespace PCGElementHelpers
{
	bool SplitDataPerPrimaryPin(const UPCGSettings* Settings, const FPCGDataCollection& Collection, EPCGElementExecutionLoopMode Mode, TArray<FPCGDataCollection>& OutPrimaryCollections, FPCGDataCollection& OutCommonCollection)
	{
		check(Settings);
		OutPrimaryCollections.Reset();
		OutCommonCollection.TaggedData.Reset();

		TArray<FPCGPinProperties> RequiredPins = Settings->AllInputPinProperties().FilterByPredicate([](const FPCGPinProperties& Props) { return Props.IsRequiredPin(); });

		// Early out
		if (Mode == EPCGElementExecutionLoopMode::SinglePrimaryPin && RequiredPins.Num() != 1)
		{
			return false;
		}

		TArray<FName> RequiredPinLabels;
		Algo::Transform(RequiredPins, RequiredPinLabels, [](const FPCGPinProperties& Props) { return Props.Label; });

		TArray<FPCGDataCollection> DataPerRequiredPin;
		DataPerRequiredPin.SetNum(RequiredPinLabels.Num());

		for (int32 DataIndex = 0; DataIndex < Collection.TaggedData.Num(); ++DataIndex)
		{
			const FPCGTaggedData& TaggedData = Collection.TaggedData[DataIndex];
			int32 RequiredPinIndex = RequiredPinLabels.IndexOfByKey(TaggedData.Pin);

			if (RequiredPinIndex == INDEX_NONE)
			{
				OutCommonCollection.TaggedData.Add(TaggedData);
			}
			else
			{
				DataPerRequiredPin[RequiredPinIndex].TaggedData.Add(TaggedData);
			}
		}

		if (DataPerRequiredPin.IsEmpty())
		{
			return true;
		}

		// Broadcast to final primary collections
		if (Mode == EPCGElementExecutionLoopMode::SinglePrimaryPin)
		{
			check(DataPerRequiredPin.Num() == 1);
			OutPrimaryCollections.Reserve(DataPerRequiredPin[0].TaggedData.Num());

			for(int32 DataIndex = 0; DataIndex < DataPerRequiredPin[0].TaggedData.Num(); ++DataIndex)
			{
				FPCGDataCollection& OutPrimaryCollection = OutPrimaryCollections.Emplace_GetRef();
				OutPrimaryCollection.TaggedData.Add(DataPerRequiredPin[0].TaggedData[DataIndex]);
			}
		}
		else if (Mode == EPCGElementExecutionLoopMode::MatchingPrimaryPins)
		{
			const int32 NumberOfData = DataPerRequiredPin[0].TaggedData.Num();

			// Validate matching number of entries
			for (int32 RequiredPinIndex = 1; RequiredPinIndex < DataPerRequiredPin.Num(); ++RequiredPinIndex)
			{
				if (DataPerRequiredPin[RequiredPinIndex].TaggedData.Num() != NumberOfData)
				{
					return false;
				}
			}

			OutPrimaryCollections.SetNum(NumberOfData);

			for (int32 DataIndex = 0; DataIndex < NumberOfData; ++DataIndex)
			{
				for (int32 RequiredPinIndex = 0; RequiredPinIndex < DataPerRequiredPin.Num(); ++RequiredPinIndex)
				{
					OutPrimaryCollections[DataIndex].TaggedData.Add(DataPerRequiredPin[RequiredPinIndex].TaggedData[DataIndex]);
				}
			}
		}
		/*else if (Mode == EPCGElementExecutionLoopMode::CartesianPins)
		{
			int32 NumberOfCollections = 1;
			for (int32 RequiredPinIndex = 0; RequiredPinIndex < DataPerRequiredPin.Num(); ++RequiredPinIndex)
			{
				NumberOfCollections *= DataPerRequiredPin[RequiredPinIndex].TaggedData.Num();
			}

			OutPrimaryCollections.SetNum(NumberOfCollections);

			for (int32 RequiredPinIndex = 0; RequiredPinIndex < DataPerRequiredPin.Num(); ++RequiredPinIndex)
			{
				const FPCGDataCollection& PinData = DataPerRequiredPin[RequiredPinIndex];

				for (int32 OutCollectionIndex = 0; OutCollectionIndex < OutPrimaryCollections.Num(); ++OutCollectionIndex)
				{
					OutPrimaryCollections[OutCollectionIndex].TaggedData.Add(PinData.TaggedData[OutCollectionIndex % PinData.TaggedData.Num()]);
				}
			}
		}*/
		else
		{
			// Invalid mode
			return false;
		}

		return true;
	}
}

bool IPCGElement::Execute(FPCGContext* Context) const
{
	// Note that this might be re-executed even if in the "Done" state because the task might have dynamic dependencies.
	check(Context && Context->AsyncState.NumAvailableTasks != 0 && Context->CurrentPhase <= EPCGExecutionPhase::Done);
	check(Context->AsyncState.bIsRunningOnMainThread || !CanExecuteOnlyOnMainThread(Context));

	PCGUtils::FScopedCallOutputDevice OutputDevice;

	while (Context->CurrentPhase != EPCGExecutionPhase::Done)
	{
		PCGUtils::FScopedCall ScopedCall(*this, Context, OutputDevice);
		bool bExecutionPostponed = false;

		switch (Context->CurrentPhase)
		{
			case EPCGExecutionPhase::NotExecuted:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::NotExecuted);
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

				PreExecute(Context);

				break;
			}

			case EPCGExecutionPhase::PrepareData:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::PrepareData);
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

				if (PrepareData(Context))
				{
					Context->CurrentPhase = EPCGExecutionPhase::Execute;
				}
				else
				{
					bExecutionPostponed = true;
				}
				break;
			}

			case EPCGExecutionPhase::Execute:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::Execute);
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

#if UE_ENABLE_DEBUG_DRAWING 
				if (PCGSystemSwitches::CVarPCGDebugDrawGeneratedCells.GetValueOnAnyThread())
				{
					PCGHelpers::DebugDrawGenerationVolume(Context);
				}
#endif // UE_ENABLE_DEBUG_DRAWING

				if (ExecuteInternal(Context))
				{
					Context->CurrentPhase = EPCGExecutionPhase::PostExecute;
				}
				else
				{
					bExecutionPostponed = true;
				}
				break;
			}

			case EPCGExecutionPhase::PostExecute:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::PostExecute);
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

				PostExecute(Context);
				break;
			}

			default: // should not happen
			{
				check(0);
				break;
			}
		}

		if (bExecutionPostponed || 
			Context->AsyncState.ShouldStop() ||
			(!Context->AsyncState.bIsRunningOnMainThread && CanExecuteOnlyOnMainThread(Context))) // phase change might require access to main thread
		{
			break;
		}
	}

	return Context->CurrentPhase == EPCGExecutionPhase::Done;
}

void IPCGElement::PreExecute(FPCGContext* Context) const
{
	check(Context);
	// Check for early outs (task cancelled + node disabled)
	// Early out to stop execution
	if (Context->InputData.bCancelExecution || (!Context->ExecutionSource.GetWeakObjectPtr().IsExplicitlyNull() && !Context->ExecutionSource.IsValid()))
	{
		Context->OutputData.bCancelExecution = true;

		if (IsCancellable())
		{
			// Skip task completely
			Context->CurrentPhase = EPCGExecutionPhase::Done;
			return;
		}
	}

	// Prepare to move to prepare data phase
	Context->CurrentPhase = EPCGExecutionPhase::PrepareData;

	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();

	if (!SettingsInterface)
	{
		return;
	}

	if (!SettingsInterface->bEnabled)
	{
		//Pass-through - no execution
		DisabledPassThroughData(Context);
		Context->CurrentPhase = EPCGExecutionPhase::PostExecute;
		return;
	}

	// Will override the settings if there is any override.
	Context->OverrideSettings();

	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();

	// If we were supposed to execute on GPU and end up here, then GPU compilation failed. Pass through.
	if (Settings && Settings->ShouldExecuteOnGPU())
	{
		DisabledPassThroughData(Context);
		Context->CurrentPhase = EPCGExecutionPhase::PostExecute;
		return;
	}

	if (CVarPCGAllowPerDataCaching.GetValueOnAnyThread())
	{
		// Default implementation when the entries in a primary loop can be processed independently, e.g. they can appear in the cache separately
		// Implementation note: this supposes that the current node has only ONE required pin, and not multiple.
		// For more complex cases (such as multiple required pins, whether cartesian or matching), the implementation should use common code instead to streamline this process -
		// both when getting the results from the cache but also when writing them
		if (ExecutionLoopMode(Settings) != EPCGElementExecutionLoopMode::NotALoop && IsCacheableInstance(Settings))
		{
			PreExecutePrimaryLoopElement(Context, Settings);
		}
	}
}

void IPCGElement::PreExecutePrimaryLoopElement(FPCGContext* Context, const UPCGSettings* Settings) const
{
	check(Context);

	if (!Settings)
	{
		return;
	}

	// Mark inputs in the order they're presented so we can appropriately find the relation from output to input after the execution
	// TODO: this is not sufficient to do a proper mapping from output to input when we have a cartesian loop
	for (int32 DataIndex = 0; DataIndex < Context->InputData.TaggedData.Num(); ++DataIndex)
	{
		Context->InputData.TaggedData[DataIndex].OriginalIndex = DataIndex;
	}

	TArray<FPCGDataCollection> PrimaryDataCollections;
	FPCGDataCollection OtherData;
	if (!PCGElementHelpers::SplitDataPerPrimaryPin(Settings, Context->InputData, ExecutionLoopMode(Settings), PrimaryDataCollections, OtherData))
	{
		return;
	}

	// Implementation note: if there is a single primary data collection, then there's no point checking in the cache again, since that has already been done.
	if (PrimaryDataCollections.Num() <= 1)
	{
		return;
	}

	const bool bShouldComputeFullOutputDataCrc = ShouldComputeFullOutputDataCrc(Context);

	// Check against the cache if subcollections of one data from the primary data collection + the other data is found already in the cache.
	// If so, we can remove the matching input data - note that this is somewhat trivial in the single pin & matching pin cases, but in general is not going to work for cartesian cases.
	for (int32 PrimaryDataIndex = PrimaryDataCollections.Num() - 1; PrimaryDataIndex >= 0; --PrimaryDataIndex)
	{
		const FPCGDataCollection& PrimaryDataCollection = PrimaryDataCollections[PrimaryDataIndex];
		FPCGDataCollection SubCollection = PrimaryDataCollection;
		SubCollection.TaggedData.Append(OtherData.TaggedData);

		SubCollection.TaggedData.Sort([](const FPCGTaggedData& A, const FPCGTaggedData& B) { return A.OriginalIndex < B.OriginalIndex; });
		SubCollection.ComputeCrcs(bShouldComputeFullOutputDataCrc);

		FPCGGetFromCacheParams CacheParams = { .Node = Context->Node, .Element = this, .ExecutionSource = Context->ExecutionSource.Get() };
		GetDependenciesCrc(FPCGGetDependenciesCrcParams(&SubCollection, Settings, Context->ExecutionSource.Get()), CacheParams.Crc);

		FPCGDataCollection SubCollectionOutput;
		if(Context->GetFromCache(CacheParams, SubCollectionOutput))
		{
			// Found a match in the cache, add it to the output, and remove the matching inputs.
			// Note that in the input part we'll take only the data present in the PrimaryDataCollection here, as we will reintroduce only those then.
			// IMPLEMENTATION NOTE: the order is important here; if we can't guarantee the hint index, then we should just do a remove!
			TPair<FPCGDataCollection, FPCGDataCollection>& InputToOutputResults = Context->CachedInputToOutputInternalResults.Emplace_GetRef();
			InputToOutputResults.Key = PrimaryDataCollection;
			InputToOutputResults.Value = SubCollectionOutput;

			for (int32 SubDataIndex = PrimaryDataCollection.TaggedData.Num() - 1; SubDataIndex >= 0; --SubDataIndex)
			{
				Context->InputData.TaggedData.RemoveAt(PrimaryDataCollection.TaggedData[SubDataIndex].OriginalIndex);
			}
		}
	}

	//
	// TODO : if there are no inputs left, then we could skip the execute phase
	// 
}

bool IPCGElement::PrepareData(FPCGContext* Context) const
{
	if (!ConvertInputsIfNeeded(Context))
	{
		return false;
	}
	
	return PrepareDataInternal(Context);
}

bool IPCGElement::ConvertInputsIfNeeded(FPCGContext* Context) const
{
	if (Context->InputData.TaggedData.IsEmpty())
	{
		return true;
	}

	// todo_pcg: Force-allow GPU resident data to tip-toe around ABI for hotfix. Add implementation of SupportsGPUResidentData to the hi-gen grid size node in 5.7.
	if (!SupportsGPUResidentData(Context) && Context->GetInputSettings<UPCGHiGenGridSizeSettings>() == nullptr)
	{
		bool bHasPendingReadbacks = false;

		// If there are any proxies in the input data, request readback to CPU.
		for (FPCGTaggedData& TaggedData : Context->InputData.TaggedData)
		{
			if (const UPCGProxyForGPUData* DataWithGPUSupport = Cast<UPCGProxyForGPUData>(TaggedData.Data))
			{
#if WITH_EDITOR
				if (Context->Node && Context->GetStack() && Context->ExecutionSource.IsValid())
				{
					Context->ExecutionSource->GetExecutionState().GetInspection().NotifyGPUToCPUReadback(Context->Node, Context->GetStack());
				}
#endif

				// Poll until readback is done.
				UPCGProxyForGPUData::FReadbackResult Result = DataWithGPUSupport->GetCPUData(Context);

				if (Result.bComplete)
				{
					// Readback done, replace the proxy with the result data.
					ensure(Result.TaggedData.Data);
					TaggedData.Data = Result.TaggedData.Data;
					TaggedData.Tags.Append(Result.TaggedData.Tags);

					// Will update referenced data objects.
					Context->bInputDataModified = true;
				}
				else
				{
					bHasPendingReadbacks = true;
				}
			}
		}

		if (bHasPendingReadbacks)
		{
			Context->bIsPaused = true;

			// Not ready to execute and unlikely to be in the very short term, sleep until next frame.
			ExecuteOnGameThread(UE_SOURCE_LOCATION, [ContextHandle = Context->GetOrCreateHandle()]()
			{
				if (TSharedPtr<FPCGContextHandle> SharedHandle = ContextHandle.Pin())
				{
					if (FPCGContext* ContextPtr = SharedHandle->GetContext())
					{
						ContextPtr->bIsPaused = false;
					}
				}
			});

			return false;
		}
	}

	if (!SupportsBasePointDataInputs(Context))
	{
		bool bDataWasConverted = false;

		for (FPCGTaggedData& Data : Context->InputData.TaggedData)
		{
			if (const UPCGBasePointData* BasePointData = Cast<UPCGBasePointData>(Data.Data))
			{
				// No doesn't support anything else than UPCGPointData
				if (!BasePointData->IsA<UPCGPointData>())
				{
					Data.Data = BasePointData->ToPointData(Context);
					bDataWasConverted = true;
				}
			}
		}

		if (bDataWasConverted && CVarPCGEnablePointArrayDataToPointDataConversionWarnings.GetValueOnAnyThread())
		{
			const UPCGSettings* Settings = Context->GetOriginalSettings<UPCGSettings>();
			const FText SettingsName = Settings ? FText::FromString(Settings->GetName()) : LOCTEXT("UnknownSettings", "Unknown");
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("UnsupportedPointArrayData", "ToPointData was called on inputs of node '{0}'. Consider implementing support for UPCGBasePointData (IPCGElement::SupportsBasePointDataInputs)."), SettingsName));
		}
	}

	return true;
}

bool IPCGElement::PrepareDataInternal(FPCGContext* Context) const
{
	return true;
}

void IPCGElement::PostExecute(FPCGContext* Context) const
{
	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();

	// Allow sub class to do some processing here
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("IPCGElement::PostExecute::PostExecuteInternal (%s)"), Settings ? *Settings->GetName() : TEXT("")));
		PostExecuteInternal(Context);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::CleanupAndValidateOutput);
		// Cleanup and validate output
		CleanupAndValidateOutput(Context);
	}

	if (CVarPCGAllowPerDataCaching.GetValueOnAnyThread())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::PostExecutePrimaryLoopElement);
		if (!Context->OutputData.bCancelExecution && ExecutionLoopMode(Settings) != EPCGElementExecutionLoopMode::NotALoop && IsCacheableInstance(Settings))
		{
			PostExecutePrimaryLoopElement(Context, Settings);
		}
	}

	// Output data Crc
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::CRC);

		// Some nodes benefit from computing an actual CRC from the data. This can halt the propagation of change/executions through the graph. For
		// data like landscapes we will never have a full accurate data crc for it so we'll tend to assume changed which triggers downstream
		// execution. Performing a detailed CRC of output data can detect real change in the data and halt the cascade of execution.
		const bool bShouldComputeFullOutputDataCrc = ShouldComputeFullOutputDataCrc(Context);

		// Compute Crc from output data which will include output pin labels.
		Context->OutputData.ComputeCrcs(bShouldComputeFullOutputDataCrc);
	}

#if WITH_EDITOR
	const bool bHasErrorsOrWarnings = Context->Node && Context->HasVisualLogs();
#else
	const bool bHasErrorsOrWarnings = false;
#endif

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::StoreInCache);
		// Store result in cache
		// TODO - There is a potential mismatch here between using the Settings (incl. overrides) and the input settings interface (pre-overrides), as done in the graph executor.
		// TODO - The dependencies CRC here should always be valid except in the indirection case, which we should normalize to allow caching (tested here, otherwise it can ensure in the graph cache)
		if (!Context->OutputData.bCancelExecution && !bHasErrorsOrWarnings && Context->DependenciesCrc.IsValid() && IsCacheableInstance(Settings))
		{
			bool bCacheable = true;

			// GPU proxies are never cached. Caching them would hold on to precious graphics memory.
			for (const FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				if (TaggedData.Data && !TaggedData.Data->IsCacheable())
				{
					bCacheable = false;
					break;
				}
			}

			if (bCacheable)
			{
				FPCGStoreInCacheParams Params = { .Element = this, .Crc = Context->DependenciesCrc };
				Context->StoreInCache(Params, Context->OutputData);
			}
		}
	}

	// Analyze if the output data is used multiple times, if the element requires it.
	if (ShouldVerifyIfOutputsAreUsedMultipleTimes(Settings))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::ShouldVerifyIfOutputsAreUsedMultipleTimes);
		
		TSet<const UPCGData*> InputData;
		TMap<const UPCGData*, int> OutputTaggedDataMap;
		OutputTaggedDataMap.Reserve(Context->OutputData.TaggedData.Num());
		for (FPCGTaggedData& OutputData : Context->OutputData.TaggedData)
		{
			OutputTaggedDataMap.FindOrAdd(OutputData.Data, 0) += 1;
		}

		for (const FPCGTaggedData& InputTaggedData: Context->InputData.TaggedData)
		{
			InputData.Add(InputTaggedData.Data);
		}
		
		for (FPCGTaggedData& OutputData : Context->OutputData.TaggedData)
		{
			// Enforce that pinless data is always used multiple times, or if the debug mode is enabled, or was detected multiple times in the output.
			if (OutputTaggedDataMap[OutputData.Data] > 1 || OutputData.bPinlessData || (Settings && Settings->CanBeDebugged() && Settings->bDebug))
			{
				OutputData.bIsUsedMultipleTimes = true;
				continue;
			}
			
			// For data that are marked to be used multiple times, they are potentially not used multiple times if they are not passthrough
			// (hence if they are not in the input). So set them back to false in that case. It will be set to true again by the executor
			// if it is actually used in multiple places.
			if (OutputData.bIsUsedMultipleTimes && !InputData.Contains(OutputData.Data))
			{
#if !UE_BUILD_SHIPPING
				OutputData.OriginatingNode = Context->Node;
#endif //!UE_BUILD_SHIPPING
				OutputData.bIsUsedMultipleTimes = false;
			}
		}
	}
	
#if WITH_EDITOR
	// Register the element to the component indicating the element has run and can have dynamic tracked keys.
	if (Settings && Settings->CanDynamicallyTrackKeys() && Context->ExecutionSource.IsValid())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::RegisterDynamicTracking);
		Context->ExecutionSource->GetExecutionState().RegisterDynamicTracking(Settings, {});
	}
#endif // WITH_EDITOR

	Context->CurrentPhase = EPCGExecutionPhase::Done;
}

void IPCGElement::PostExecutePrimaryLoopElement(FPCGContext* Context, const UPCGSettings* Settings) const
{
	check(Context);

	if (!Settings)
	{
		return;
	}

#if WITH_EDITOR
	const bool bHasErrorsOrWarnings = Context->Node && Context->HasVisualLogs();
#else
	const bool bHasErrorsOrWarnings = false;
#endif

	// Store individual results in the cache; here we will try to match the remaining hint indices from the input data with the ones given at the output.
	TArray<FPCGDataCollection> PrimaryDataCollections;
	FPCGDataCollection OtherData;
	if (!Context->OutputData.bCancelExecution && !bHasErrorsOrWarnings && PCGElementHelpers::SplitDataPerPrimaryPin(Settings, Context->InputData, ExecutionLoopMode(Settings), PrimaryDataCollections, OtherData))
	{
		const bool bShouldComputeFullOutputDataCrc = ShouldComputeFullOutputDataCrc(Context);

		for (const FPCGDataCollection& PrimaryDataCollection : PrimaryDataCollections)
		{
			if (PrimaryDataCollection.TaggedData.IsEmpty() || PrimaryDataCollection.TaggedData[0].OriginalIndex == INDEX_NONE)
			{
				continue;
			}

			FPCGDataCollection SubCollectionOutput;
			for (int32 DataIndex = 0; DataIndex < Context->OutputData.TaggedData.Num(); ++DataIndex)
			{
				const FPCGTaggedData& TaggedData = Context->OutputData.TaggedData[DataIndex];
				if (TaggedData.OriginalIndex == PrimaryDataCollection.TaggedData[0].OriginalIndex)
				{
					SubCollectionOutput.TaggedData.Add(Context->OutputData.TaggedData[DataIndex]);
				}
			}

			bool bCacheable = true;

			for (const FPCGTaggedData& TaggedData : SubCollectionOutput.TaggedData)
			{
				if (TaggedData.Data && !TaggedData.Data->IsCacheable())
				{
					bCacheable = false;
					break;
				}
			}

			if (bCacheable)
			{
				FPCGDataCollection SubCollection = PrimaryDataCollection;
				SubCollection.TaggedData.Append(OtherData.TaggedData);

				SubCollection.TaggedData.Sort([](const FPCGTaggedData& A, const FPCGTaggedData& B) { return A.OriginalIndex < B.OriginalIndex; });
				SubCollection.ComputeCrcs(bShouldComputeFullOutputDataCrc);

				FPCGCrc DependenciesCrc;
				GetDependenciesCrc(FPCGGetDependenciesCrcParams(&SubCollection, Settings, Context->ExecutionSource.Get()), DependenciesCrc);

				SubCollectionOutput.ComputeCrcs(bShouldComputeFullOutputDataCrc);

				FPCGStoreInCacheParams Params = { .Element = this, .Crc = DependenciesCrc };
				Context->StoreInCache(Params, SubCollectionOutput);
			}
		}
	}

	// Put back cached results and set aside input (needed for inspection) if any
	if (!Context->CachedInputToOutputInternalResults.IsEmpty())
	{
		// Push cached results back to the final output data, from the last to the first, at the right place.
		for (int CachedCollectionIndex = Context->CachedInputToOutputInternalResults.Num() - 1; CachedCollectionIndex >= 0; --CachedCollectionIndex)
		{
			// Since we've assigned original indices up front, we should be able to respect this here.
			const FPCGDataCollection& CachedInputData = Context->CachedInputToOutputInternalResults[CachedCollectionIndex].Key;
			const int32 CacheInputOriginalIndex = (CachedInputData.TaggedData.IsEmpty() ? INDEX_NONE : CachedInputData.TaggedData[0].OriginalIndex);
			const int32 InsertInputIndex = CacheInputOriginalIndex != INDEX_NONE ? Context->InputData.TaggedData.IndexOfByPredicate([CacheInputOriginalIndex](const FPCGTaggedData& TaggedData) { return TaggedData.OriginalIndex > CacheInputOriginalIndex; }) : INDEX_NONE;

			if (InsertInputIndex != INDEX_NONE)
			{
				Context->InputData.TaggedData.Insert(CachedInputData.TaggedData, InsertInputIndex);
			}
			else
			{
				Context->InputData.TaggedData.Append(CachedInputData.TaggedData);
			}

			// Note: we're modifying the context data, but it will not be reused after
			FPCGDataCollection& CachedOutputData = Context->CachedInputToOutputInternalResults[CachedCollectionIndex].Value;

			// Since this data comes from the cache, we can't rely on its original index; however, it should be used as-if it was keyed on the input original index.
			const int32 CacheOutputOriginalIndex = CacheInputOriginalIndex;

			// Apply matching original index so that further iterations place the data at the right indices.
			for (FPCGTaggedData& TaggedData : CachedOutputData.TaggedData)
			{
				TaggedData.OriginalIndex = CacheOutputOriginalIndex;
			}

			const int32 InsertOutputIndex = CacheOutputOriginalIndex != INDEX_NONE ? Context->OutputData.TaggedData.IndexOfByPredicate([CacheOutputOriginalIndex](const FPCGTaggedData& TaggedData) { return TaggedData.OriginalIndex > CacheOutputOriginalIndex; }) : INDEX_NONE;

			if (InsertOutputIndex != INDEX_NONE)
			{
				Context->OutputData.TaggedData.Insert(CachedOutputData.TaggedData, InsertOutputIndex);
			}
			else
			{
				Context->OutputData.TaggedData.Append(CachedOutputData.TaggedData);
			}
		}
	}
}

void IPCGElement::Abort(FPCGContext* Context) const
{
	AbortInternal(Context);
}

void IPCGElement::DisabledPassThroughData(FPCGContext* Context) const
{
	check(Context);

	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();
	check(Settings);

	if (!Context->Node)
	{
		// Full pass-through if we don't have a node
		Context->OutputData = Context->InputData;
		return;
	}

	if (Context->Node->GetInputPins().Num() == 0 || Context->Node->GetOutputPins().Num() == 0)
	{
		// No input pins or not output pins, return nothing
		return;
	}

	const UPCGPin* PassThroughInputPin = Context->Node->GetPassThroughInputPin();
	const UPCGPin* PassThroughOutputPin = Context->Node->GetPassThroughOutputPin();
	if (!PassThroughInputPin || !PassThroughOutputPin)
	{
		// No pin to grab pass through data from or to pass data to.
		return;
	}

	const EPCGDataType OutputType = PassThroughOutputPin->GetCurrentTypes();

	// Grab data from pass-through pin, push it all to output pin
	Context->OutputData.TaggedData = Context->InputData.GetInputsByPin(PassThroughInputPin->Properties.Label);
	for (FPCGTaggedData& Data : Context->OutputData.TaggedData)
	{
		Data.Pin = PassThroughOutputPin->Properties.Label;
	}

	// Pass through input data if both it and the output are params, or if the output type supports it (e.g. if we have a incoming
	// surface connected to an input pin of type Any, do not pass the surface through to an output pin of type Point).
	auto InputDataShouldPassThrough = [OutputType](const FPCGTaggedData& InData)
	{
		EPCGDataType InputType = InData.Data ? InData.Data->GetDataType() : EPCGDataType::None;
		const bool bInputTypeNotWiderThanOutputType = !(InputType & ~OutputType);

		// Right now we allow edges from Spatial to Concrete. This can happen for example if a point processing node
		// is receiving a Spatial data, and the node is disabled, it will want to pass the Spatial data through. In the
		// future we will force collapses/conversions. For now, allow an incoming Spatial to pass out through a Concrete.
		// TODO remove!
		const bool bAllowSpatialToConcrete = !!(InputType & EPCGDataType::Spatial) && !!(OutputType & EPCGDataType::Concrete);

		return (InputType != EPCGDataType::Param || OutputType == EPCGDataType::Param) && (bInputTypeNotWiderThanOutputType || bAllowSpatialToConcrete);
	};

	// Now remove any non-params edges, and if only one edge should come through, remove the others
	if (Settings->OnlyPassThroughOneEdgeWhenDisabled())
	{
		// Find first incoming non-params data that is coming through the pass through pin
		TArray<FPCGTaggedData> InputsOnFirstPin = Context->InputData.GetInputsByPin(PassThroughInputPin->Properties.Label);
		const int FirstNonParamsDataIndex = InputsOnFirstPin.IndexOfByPredicate(InputDataShouldPassThrough);

		if (FirstNonParamsDataIndex != INDEX_NONE)
		{
			// Remove everything except the data we found above
			for (int Index = Context->OutputData.TaggedData.Num() - 1; Index >= 0; --Index)
			{
				if (Index != FirstNonParamsDataIndex)
				{
					Context->OutputData.TaggedData.RemoveAt(Index);
				}
			}
		}
		else
		{
			// No data found to return
			Context->OutputData.TaggedData.Empty();
		}
	}
	else
	{
		// Remove any incoming non-params data that is coming through the pass through pin
		TArray<FPCGTaggedData> InputsOnFirstPin = Context->InputData.GetInputsByPin(PassThroughInputPin->Properties.Label);
		for (int Index = InputsOnFirstPin.Num() - 1; Index >= 0; --Index)
		{
			const FPCGTaggedData& Data = InputsOnFirstPin[Index];

			if (!InputDataShouldPassThrough(Data))
			{
				Context->OutputData.TaggedData.RemoveAt(Index);
			}
		}
	}
}

FPCGContext* IPCGElement::CreateContext()
{
	return new FPCGContext();
}

#if WITH_EDITOR
bool IPCGElement::DebugDisplay(FPCGContext* Context) const
{
	// Check Debug flag.
	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();
	if (!SettingsInterface || !SettingsInterface->bDebug)
	{
		return true;
	}

	// If graph is being inspected, only display Debug if the component is being inspected, or in the HiGen case also display if
	// this component is a parent of an inspected component (because this data is available to child components).

	// If the graph is not being inspected, or the current component is being inspected, then we know we should display
	// debug, if not then we do further checks.
	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	UPCGGraph* Graph = SourceComponent ? SourceComponent->GetGraph() : nullptr;
	if (Graph && Graph->IsInspecting() && !SourceComponent->GetExecutionState().GetInspection().IsInspecting() && Graph->DebugFlagAppliesToIndividualComponents())
	{
		// If we're no doing HiGen, or if the current component is not a local component (and therefore will not have children),
		// then do not display debug.
		if (!Graph->IsHierarchicalGenerationEnabled())
		{
			return true;
		}

		// If a child of this component is being inspected (a local component on smaller grid and overlapping) then we still show debug,
		// because this data is available to that child for use.
		if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(SourceComponent->GetExecutionState().GetWorld()))
		{
			const uint32 ThisGenerationGridSize = SourceComponent->GetGenerationGridSize();

			bool bFoundInspectedChildComponent = false;
			Subsystem->ForAllOverlappingComponentsInHierarchy(SourceComponent, [ThisGenerationGridSize, &bFoundInspectedChildComponent](UPCGComponent* InLocalComponent)
			{
				if (InLocalComponent->GetGenerationGridSize() < ThisGenerationGridSize && InLocalComponent->GetExecutionState().GetInspection().IsInspecting())
				{
					bFoundInspectedChildComponent = true;
				}
			});

			// If no inspected child component then don't display debug.
			if (!bFoundInspectedChildComponent)
			{
				return true;
			}
		}
	}

	// In the case of a node with multiple output pins, we will select only the inputs from the first non-empty pin.
	const UPCGPin* FirstOutPin = Context->Node ? Context->Node->GetFirstConnectedOutputPin() : nullptr;

	bool bHasPendingReadbacks = false;

	TArray<FPCGTaggedData> DataToDebug;
	DataToDebug.Reserve(Context->OutputData.TaggedData.Num());

	// If there are any proxies in the input data, request readback to CPU.
	for (const FPCGTaggedData& Output : Context->OutputData.TaggedData)
	{
		// Skip output if we're filtering on the first pin or the the data is null.
		if (!Output.Data || (FirstOutPin && FirstOutPin->Properties.Label != Output.Pin))
		{
			continue;
		}

		if (const UPCGProxyForGPUData* DataWithGPUSupport = Cast<UPCGProxyForGPUData>(Output.Data))
		{
			// Poll until readback is done.
			UPCGProxyForGPUData::FReadbackResult Result = DataWithGPUSupport->GetCPUData(Context);

			if (Result.bComplete)
			{
				// Readback done. Queue up data for debug (unless we've found pending readbacks, in which case we'll run again later).
				if (ensure(Result.TaggedData.Data) && !bHasPendingReadbacks)
				{
					FPCGTaggedData TaggedData = Output;
					TaggedData.Data = Result.TaggedData.Data;
					TaggedData.Tags.Append(Result.TaggedData.Tags);

					DataToDebug.Add(TaggedData);
				}
			}
			else
			{
				bHasPendingReadbacks = true;
			}
		}
		else
		{
			DataToDebug.Add(Output);
		}
	}

	if (bHasPendingReadbacks)
	{
		// Signal that data was not ready, will run again later.
		return false;
	}

	const FPCGDataVisualizationRegistry& DataVisRegistry = FPCGModule::GetConstPCGDataVisualizationRegistry();

	for (const FPCGTaggedData& Data : DataToDebug)
	{
		if (const IPCGDataVisualization* DataVis = DataVisRegistry.GetDataVisualization(Data.Data->GetClass()))
		{
			DataVis->ExecuteDebugDisplay(Context, SettingsInterface, Data.Data, Context->GetTargetActor(nullptr));
		}
	}

	return true;
}
#endif // WITH_EDITOR

void IPCGElement::CleanupAndValidateOutput(FPCGContext* Context) const
{
	check(Context);
	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();
	const UPCGSettings* Settings = SettingsInterface ? SettingsInterface->GetSettings() : nullptr;

	// Implementation note - disabled passthrough nodes can happen only in subgraphs/ spawn actor nodes
	// which will behave properly when disabled. 
	if (Settings && !IsPassthrough(Settings))
	{
		// Cleanup any residual labels if the node isn't supposed to produce them
		// TODO: this is a bit of a crutch, could be refactored out if we review the way we push tagged data
		TArray<FPCGPinProperties> OutputPinProperties = Settings->AllOutputPinProperties();
		if(OutputPinProperties.Num() == 1)
		{
			for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				if (!TaggedData.bPinlessData)
				{
					TaggedData.Pin = OutputPinProperties[0].Label;
				}				
			}
		}

		// Validate all out data for errors in labels
#if WITH_EDITOR
		if (SettingsInterface->bEnabled)
		{
			// remove null outputs
			Context->OutputData.TaggedData.RemoveAll([this, Context](const FPCGTaggedData& TaggedData){

				if (TaggedData.Data == nullptr)
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("NullPinOutputData", "Invalid output(s) generated for pin '{0}'"), FText::FromName(TaggedData.Pin)));
					return true;
				}

				return false;
			});


			for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				const int32 MatchIndex = OutputPinProperties.IndexOfByPredicate([&TaggedData](const FPCGPinProperties& InProp) { return TaggedData.Pin == InProp.Label; });
				if (MatchIndex == INDEX_NONE)
				{
					// Only display an error if we expected this data to have a pin.
					if (!TaggedData.bPinlessData)
					{
						PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("OutputCannotBeRouted", "Output data generated for non-existent output pin '{0}'"), FText::FromName(TaggedData.Pin)));
					}
				}
				else if (ensure(TaggedData.Data))
				{
					// Try to get dynamic current pin types, otherwise settle for static types
					const UPCGPin* OutputPin = Context->Node ? Context->Node->GetOutputPin(OutputPinProperties[MatchIndex].Label) : nullptr;
					const EPCGDataType PinTypes = OutputPin ? OutputPin->GetCurrentTypes() : OutputPinProperties[MatchIndex].AllowedTypes;

					EPCGDataType DataType = TaggedData.Data->GetUnderlyingDataType();
					const bool bTypesOverlap = !!(PinTypes & DataType);
					const bool bTypeIsSubset = !(~PinTypes & DataType);
					// TODO: Temporary fix for Settings directly from InputData (ie. from elements with code and not PCG nodes)
					if ((!bTypesOverlap || !bTypeIsSubset) && DataType != EPCGDataType::Settings)
					{
						PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("OutputIncompatibleType", "Output data generated for pin '{0}' does not have a compatible type: '{1}'. Consider using more specific/narrower input pin types, or more general/wider output pin types."), FText::FromName(TaggedData.Pin), FText::FromString(UEnum::GetValueAsString(DataType))));
					}
				}

				if (CVarPCGValidatePointMetadata.GetValueOnAnyThread())
				{
					if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(TaggedData.Data))
					{
						const int32 MaxMetadataEntry = PointData->Metadata ? PointData->Metadata->GetItemCountForChild() : 0;

						bool bHasError = false;

						const TConstPCGValueRange<int64> MetadataEntryRange = PointData->GetConstMetadataEntryValueRange();

						for(int32 PointIndex = 0; PointIndex < MetadataEntryRange.Num() && !bHasError; ++PointIndex)
						{
							bHasError |= (MetadataEntryRange[PointIndex] >= MaxMetadataEntry);
						}

						if (bHasError)
						{
							PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("OutputMissingPointMetadata", "Output generated for pin '{0}' does not have valid point metadata"), FText::FromName(TaggedData.Pin)));
						}
					}
				}
			}
		}
#endif
	}
}

FPCGContext* IPCGElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	return nullptr;
}

FPCGContext* IPCGElement::Initialize(const FPCGInitializeElementParams& InParams)
{
	check(InParams.InputData);
	// For backward compatibility (call the old signature)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPCGContext* Context = Initialize(*InParams.InputData, Cast<UPCGComponent>(InParams.ExecutionSource.GetObject()), InParams.Node);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	if (!Context)
	{
		Context = CreateContext();
		Context->InitFromParams(InParams);
	}
	else
	{
		Context->ExecutionSource = InParams.ExecutionSource;

		const UPCGSettings* Settings = InParams.Node ? InParams.Node->GetSettings() : nullptr;
		const FString SettingsName = Settings ? Settings->GetName() : TEXT("Unknown");
		UE_LOG(LogPCG, Warning, TEXT("This node '%s' implements a deprecated version of Initialize. Please implement version with FPCGInitializeElementParams parameter."), *SettingsName);
	}

	return Context;
}

bool IPCGElement::IsCacheable(const UPCGSettings* Settings) const
{
	return !Settings || !Settings->ShouldExecuteOnGPU();
}

bool IPCGElement::IsCacheableInstance(const UPCGSettingsInterface* InSettingsInterface) const
{
	if (InSettingsInterface)
	{
		if (!InSettingsInterface->bEnabled)
		{
			return false;
		}
		else
		{
			return IsCacheable(InSettingsInterface->GetSettings());
		}
	}
	else
	{
		return false;
	}
}

bool IPCGElement::ShouldVerifyIfOutputsAreUsedMultipleTimes(const UPCGSettings* InSettings) const
{
	return CVarPCGShouldVerifyIfOutputsAreUsedMultipleTimes.GetValueOnAnyThread();
}

void IPCGElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	// do nothing
}

void IPCGElement::GetDependenciesCrc(const FPCGGetDependenciesCrcParams& InParams, FPCGCrc& OutCrc) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("IPCGElement::GetDependenciesCrc (%s)"), InParams.Settings ? *InParams.Settings->GetName() : TEXT("")));
	
	check(InParams.InputData);
	FPCGCrc CopyCrc(OutCrc);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GetDependenciesCrc(*InParams.InputData, InParams.Settings, Cast<UPCGComponent>(InParams.ExecutionSource), OutCrc);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Call to deprecated method didn't yield a different Crc so we calculate it here
	if (!OutCrc.IsValid() || CopyCrc == OutCrc)
	{
		// Start from a random prime.
		FPCGCrc Crc(1000003);

		// The cached data CRCs are computed in FPCGGraphExecutor::BuildTaskInput and incorporate data CRC, tags, output pin label and input pin label.
		for (const FPCGCrc& DataCrc : InParams.InputData->DataCrcs)
		{
			Crc.Combine(DataCrc);
		}

		if (InParams.Settings)
		{
			FPCGCrc SettingsCrc = InParams.Settings->GetSettingsCrc();
			if (ensure(SettingsCrc.IsValid()))
			{
				Crc.Combine(SettingsCrc);
			}
		}

		if (InParams.ExecutionSource && (!InParams.Settings || InParams.Settings->UseSeed()))
		{
			Crc.Combine(InParams.ExecutionSource->GetExecutionState().GetSeed());
		}

		OutCrc = Crc;
	}
	else
	{
		const FString SettingsName = InParams.Settings ? InParams.Settings->GetName() : TEXT("Unknown");
		UE_LOG(LogPCG, Warning, TEXT("This node '%s' implements a deprecated version of GetDependenciesCrc. Please implement version with FPCGGetDependenciesCrcParams parameter."), *SettingsName);
	}
}

EPCGCachingStatus IPCGElement::RetrieveResultsFromCache(IPCGGraphCache* Cache, const UPCGNode* Node, const FPCGDataCollection& Input, IPCGGraphExecutionSource* ExecutionSource, FPCGDataCollection& Output, FPCGCrc* OutCrc) const
{
	if (!Cache)
	{
		return EPCGCachingStatus::NotInCache;
	}

	const UPCGSettingsInterface* SettingsInterface = Input.GetSettingsInterface(Node ? Node->GetSettingsInterface() : nullptr);
	const UPCGSettings* Settings = SettingsInterface ? SettingsInterface->GetSettings() : nullptr;
	const bool bCacheable = IsCacheableInstance(SettingsInterface);

	FPCGGetFromCacheParams Params = { .Node = Node, .Element = this, .ExecutionSource = ExecutionSource };

	if (Settings && bCacheable)
	{
		GetDependenciesCrc(FPCGGetDependenciesCrcParams(&Input, Settings, ExecutionSource), Params.Crc);

		if (OutCrc)
		{
			*OutCrc = Params.Crc;
		}
	}

	if(Params.Crc.IsValid() && Cache->GetFromCache(Params, Output))
	{
		return EPCGCachingStatus::Cached;
	}
	else
	{
		return EPCGCachingStatus::NotInCache;
	}
}

#undef LOCTEXT_NAMESPACE

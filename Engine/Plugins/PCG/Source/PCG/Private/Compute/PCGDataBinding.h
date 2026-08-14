// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "Compute/PCGDataForGPU.h"

#include "RenderGraphResources.h"

#include "PCGDataBinding.generated.h"

class UComputeKernel;
class UPCGComponent;
class UPCGComputeGraph;
class UPCGComputeKernel;
class UPCGProxyForGPUData;
struct FPCGComputeGraphContext;
struct FPCGContextHandle;
struct FPCGKernelPin;

USTRUCT()
struct FPCGSpawnerPrimitive
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Component;

	UPROPERTY()
	int NumInstances = INDEX_NONE;
};

USTRUCT()
struct FPCGSpawnerPrimitives
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FPCGSpawnerPrimitive> Primitives;

	TArray<FBox> PrimitiveMeshBounds;

	/** Cumulative distribution function values (one per primitive) to enable choosing a primitive based on a random draw value. */
	TArray<float> SelectionCDF;

	// Same for all primitives
	uint32 NumCustomFloats = 0;

	// Same for all primitives
	TArray<FUintVector4> AttributeIdOffsetStrides;

	int32 SelectorAttributeId = INDEX_NONE;

	TArray<int32> PrimitiveStringKeys;
};

USTRUCT()
struct FPCGDataToDebug
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UPCGData> Data;

	UPROPERTY()
	TObjectPtr<UPCGData> DataPendingInit;
	
	UPROPERTY()
	TWeakObjectPtr<const UPCGSettings> ProducerSettings;
	
	UPROPERTY()
	FName PinLabel;
	
	UPROPERTY()
	FName PinLabelAlias;

	// @todo_pcg: This is a big hack to support tags on texture data proxies. Should be replaced with a proper abstraction.
	/** Allow appending additional tags to the CPU tagged data. */
	UPROPERTY()
	TSet<FString> AdditionalTags;
};

UCLASS(Transient, Category = PCG)
class UPCGDataBinding : public UObject
{
	GENERATED_BODY()

public:
	/** Pre-initialization, set up pointers and store input data. */
	void Initialize(const UPCGComputeGraph* InComputeGraph, FPCGContext* InContext);

	/** Initialization look up tables from input data. */
	void InitializeTables(FPCGContext* InContext);

	/** Clear state and release any handles to resources such as GPU buffers. */
	void ReleaseTransientResources();

	int32 GetAttributeId(const FPCGKernelAttributeKey& InAttribute) const { ensure(bTablesInitialized); return AttributeTable.GetAttributeId(InAttribute); }
	int32 GetAttributeId(FPCGAttributeIdentifier InIdentifier, EPCGKernelAttributeType InType) const { ensure(bTablesInitialized); return AttributeTable.GetAttributeId(InIdentifier, InType); }

	/** Get the set of all unique strings entering the compute graph. */
	const TArray<FString>& GetStringTable() const { ensure(bTablesInitialized); return StringTable; }

	/** Computes a description of data for every pin in the compute graph and caches it. */
	void PrimeDataDescriptionCache();
	bool IsDataDescriptionCachePrimed() const { return bIsDataDescriptionCachePrimed; }

	/** Computes a description of data for a single pin in the compute graph and caches it. */
	bool ComputeKernelPinDataDesc(const FPCGKernelPin& InKernelPin, FPCGDataCollectionDesc* OutDesc);

	/** Get description of data produced by a pin. */
	const FPCGDataCollectionDesc* GetCachedKernelPinDataDesc(const UPCGComputeKernel* InKernel, FName InPinLabel, bool bIsInput) const;
	const FPCGDataCollectionDesc* GetCachedKernelPinDataDesc(int32 InGraphBindingIndex) const;

	void ReceiveDataFromGPU_GameThread(UPCGData* InData, const UPCGSettings* InProducerSettings, EPCGExportMode InExportMode, FName InPinLabel, FName InPinLabelAlias);
	void ReceiveDataFromGPU_GameThread(UPCGData* InData, const UPCGSettings* InProducerSettings, EPCGExportMode InExportMode, FName InPinLabel, FName InPinLabelAlias, const TSet<FString>& AdditionalTags);

	void ReceiveElementCountsFromGPU_GameThread(FName InPinLabel, const TArray<uint32>& InCounters);

	void ApplyElementCounts_GameThread();

	using FSourceBufferAndAttributeIndex = TPair<TSharedPtr<const FPCGProxyForGPUDataCollection>, /*Source Metadata Attribute Index*/int32>;

	const TMap<FSourceBufferAndAttributeIndex, /*Attribute Index*/int32>& GetAttributeIdRemap() const { return AttributeIdRemap; }

	using FSourceBufferAndStringKey = TPair<TRefCountPtr<FRDGPooledBuffer>, /*String Key*/int32>;

	/** Map from string key value in graph to the key value for this string in the source buffer. */
	const TMap<FSourceBufferAndStringKey, /*Graph String Key*/int32>& GetSourceBufferToGraphStringKey() const { return SourceBufferToGraphStringKey; }

	/** Map from string key value in source buffer to the key value usable in this graph. */
	const TMap<FSourceBufferAndStringKey, /*Source Buffer String Key*/int32>& GetGraphToSourceBufferStringKey() const { return GraphToSourceBufferStringKey; }

	IPCGGraphExecutionSource* GetExecutionSource() const;

	/** Helper to get index in input data collection of the first data item for the given kernel and input pin label. */
	int32 GetFirstInputDataIndex(const UPCGComputeKernel* InKernel, FName InPinLabel) const;

	/** If there is a GPU proxy at the given index of the input data collection, triggers a readback and replaces the data item with CPU data if readback succeeds.
	* Returns false while readback is in progress.
	*/
	bool ReadbackInputDataToCPU(int32 InInputDataIndex);

	void DebugLogDataDescriptions() const;

private:
	/** Loop over all metadata attributes in all input data ensure all attributes are registered in attribute table. */
	void AddInputDataAttributesToTable();
	void AddInputDataStringsToTable();
	void AddInputDataTagsToTable();

public:
	/** Compute graph element output data. Data items are labelled with unique virtual output pin labels so that the can be routed correctly by the graph
	 * executor to downstream nodes. */
	UPROPERTY()
	FPCGDataCollection OutputDataCollection;

	UPROPERTY()
	TObjectPtr<const UPCGComputeGraph> Graph = nullptr;

	/** Data arriving on compute graph element. Since the compute graph is collapsed to a single element, all data crossing from CPU to GPU is in a single collection. */
	UPROPERTY()
	FPCGDataCollection InputDataCollection;

	UPROPERTY()
	TMap<TObjectPtr<const UComputeKernel>, FPCGSpawnerPrimitives> MeshSpawnersToPrimitives;

	UPROPERTY()	
	TArray<TObjectPtr<const UComputeKernel>> CompletedMeshSpawners;

	UPROPERTY()
	TArray<FPCGDataToDebug> DataToDebug;

	UPROPERTY()
	TArray<FPCGDataToDebug> DataToInspect;

	TWeakPtr<FPCGContextHandle> ContextHandle;

protected:
	/** Final element counts read back from GPU. */
	TMap<FName, TArray<uint32>> PinToElementCounts;

	/** All attributes present in graph at execution time. Coherent across all graph branches. Seeded using statically known created attributes at compile time, then
	 * augmented with incoming attributes from input data collection at runtime. */
	UPROPERTY()
	FPCGKernelAttributeTable AttributeTable;

	UPROPERTY()
	TArray<FString> StringTable;

	/** Remaps attribute IDs to avoid collisions with this compute graph's attribute IDs. */
	TMap<FSourceBufferAndAttributeIndex, /*Attribute ID*/int32> AttributeIdRemap;

	/** Map from string key value in graph to the key value for this string in the source buffer. */
	TMap<FSourceBufferAndStringKey, /*Graph String Key*/int32> SourceBufferToGraphStringKey;

	/** Map from string key value in source buffer to the key value usable in this graph. */
	TMap<FSourceBufferAndStringKey, /*Source Buffer String Key*/int32> GraphToSourceBufferStringKey;

	/** Cache of data descriptions to amortize cost of computing them at runtime. */
	TMap</*Binding index*/int32, FPCGDataCollectionDesc> DataDescriptionCache;
	std::atomic<bool> bIsDataDescriptionCachePrimed = false;

	std::atomic<bool> bTablesInitialized = false;
};

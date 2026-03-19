// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeKernel.h"
#include "Compute/PCGComputeCommon.h"

#include "UObject/ObjectKey.h"

#include "PCGComputeKernel.generated.h"

class UPCGComputeGraph;
class UPCGDataBinding;
class UPCGSettings;
struct FPCGContext;
struct FPCGDataCollectionDesc;
struct FPCGGPUCompilationContext;
struct FPCGKernelAttributeKey;
struct FPCGPinProperties;
struct FPCGPinPropertiesGPU;

class UComputeDataInterface;

UENUM()
enum class EPCGKernelLogVerbosity
{
	Verbose,
	Warning,
	Error
};

USTRUCT()
struct FPCGKernelLogEntry
{
	GENERATED_BODY()

	FPCGKernelLogEntry() = default;

	explicit FPCGKernelLogEntry(const FText& InMessage, EPCGKernelLogVerbosity InVerbosity)
		: Message(InMessage)
		, Verbosity(InVerbosity)
	{}

	UPROPERTY()
	FText Message;

	UPROPERTY()
	EPCGKernelLogVerbosity Verbosity = EPCGKernelLogVerbosity::Verbose;
};

#if WITH_EDITOR
struct FPCGComputeKernelParams
{
	const UPCGSettings* Settings = nullptr;
	bool bLogDescriptions = false;
};
#endif

UCLASS(Abstract)
class UPCGComputeKernel : public UComputeKernel
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** Initialize kernel. Editor only as shader compilation only available in editor. */
	void Initialize(const FPCGComputeKernelParams& InParams);
#endif

	/** Get/set index of kernel in kernels array on owning compute graph. */
	int32 GetKernelIndex() const { return KernelIndex; }
	void SetKernelIndex(int32 InKernelIndex) { KernelIndex = InKernelIndex; }

	/** Gets settings for node associated with this kernel, if any. */
	virtual const UPCGSettings* GetSettings() const;

	/** Performs settings validation and returns true if this node is suitable for execution. */
	virtual bool AreKernelSettingsValid(FPCGContext* InContext) const;

	/** Performs data validation and returns true if this node is suitable for deployment to the GPU. */
	virtual bool IsKernelDataValid(FPCGContext* InContext) const { return true; }

#if WITH_EDITOR
	/** Produces the node specific portion of kernel shader source text, including the main entry point. */
	virtual FString GetCookedSource(FPCGGPUCompilationContext& InOutContext) const PURE_VIRTUAL(UPCGComputeKernel::GetCookedSource, return {};);

	/** Get the name of the main kernel function in the source. This name is also displayed in GPU profile scopes. */
	virtual FString GetEntryPoint() const PURE_VIRTUAL(UPCGComputeKernel::GetEntryPoint, return TEXT("Main"););

	/**
	 * Gathers additional compute sources referenced by this kernel.
	 * This is preferred to directly using includes, since it allows us to detect hash diffs and recompile when additional sources are changed externally.
	 * Note, you should not create new compute sources here, as their object names are not deterministic and will result in a new hash every time.
	 */
	virtual void GatherAdditionalSources(TArray<TObjectPtr<class UComputeSource>>& OutAdditionalSources) const;

	/** Create additional input data interfaces to marshal any required input data. */
	virtual void CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const {}

	/** Create additional output data interfaces to marshal any required output data. */
	virtual void CreateAdditionalOutputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const {}

	/** Whether compute graphs should be split at the output of this node. Useful for reading back runtime statistics or diagnostic info before continuing. */
	virtual bool SplitGraphAtOutput() const { return false; }

	/** Mark a pin as being internal to the kernel graph. This means it is not on the CPU/GPU boundary. */
	void AddInternalPin(FName InPinLabel) { InternalPinLabels.Add(InPinLabel); }

	/** Whether the pin matching the given label should be eligible for inspect/debug. */
	bool IsPinInternal(FName InPinLabel) const { return InternalPinLabels.Contains(InPinLabel); }
#endif

	/** Compute a description of data that will be output from pin InOutputPinLabel. */
	virtual bool ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const PURE_VIRTUAL(UPCGComputeKernel::ComputeOutputBindingDataDesc, return {};);

	/** Compute how many threads should be dispatched to execute this node on the GPU. */
	virtual int ComputeThreadCount(const UPCGDataBinding* Binding) const PURE_VIRTUAL(UPCGComputeKernel::ComputeThreadCount, return 0;);

	/** Get any data labels that are used statically by this node. */
	virtual void GetDataLabels(FName InPinLabel, TArray<FString>& OutDataLabels) const {}

	/** Initialize all buffer data to 0. This is not free so should only be used when kernel execution requires 0-initialized data, such as for counters. */
	virtual bool DoesOutputPinRequireZeroInitialization(FName InOutputPinLabel) const { return false; }

	/** Whether the output pin should have a buffer of counters attached, useful for compaction and other counting requirements. */
	virtual bool DoesOutputPinRequireElementCounters(FName InOutputPinLabel) const { return false; }

	/** Add any strings emitted by this node that are known statically at compile time. */
	virtual void AddStaticCreatedStrings(TArray<FString>& InOutStringTable) const {}

	/** Get all the attributes read or written by this node for which we know the name and type statically. In cases that only the
	* attribute name used by the node is known (example: user specifies SM Spawner instance data attributes by name only, not type), the attribute
	* is omitted from this list and must be resolved at execution time in the data provider.
	*/
	virtual void GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const {}

	/** Get multiplier to be applied to the dispatched thread count. */
	virtual uint32 GetThreadCountMultiplier() const { return 1u; }

	/** Multiplier to be applied to the element count of each output data. */
	virtual uint32 GetElementCountMultiplier(FName InOutputPinLabel) const { return 1u; }

	/** Helper to compute data descriptions when data is entirely determined by pin properties. */
	void ComputeDataDescFromPinProperties(const FPCGPinPropertiesGPU& OutputPinProps, const TArrayView<const FPCGPinProperties>& InInputPinProps, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutPinDesc) const;

	virtual void GetInputPins(TArray<FPCGPinProperties>& OutPins) const {}
	virtual void GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const {}

	virtual bool GetLogDataDescriptions() const { return bLogDataDescriptions; }

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
protected:
#if WITH_EDITOR
	/** Implement to do any additional initialization specific to your kernel. */
	virtual void InitializeInternal() {}

	/** Performs validation on compile time information (e.g. Node, Settings, etc.). Caches validation errors/warnings. */
	virtual bool PerformStaticValidation();

	/** Do any validation of the PCG node. Returns true if node is valid and GPU execution can proceed. */
	bool ValidatePCGNode(TArray<FPCGKernelLogEntry>& OutLogEntries) const;

	/** Validate input edges, for example checking connected pin types are compatible. */
	bool AreInputEdgesValid(TArray<FPCGKernelLogEntry>& OutLogEntries) const;
#endif

protected:
	/** Index into kernels array on owning compute graph. */
	UPROPERTY()
	int32 KernelIndex = INDEX_NONE;

	UPROPERTY()
	TSoftObjectPtr<const UPCGSettings> Settings;

	UPROPERTY()
	bool bLogDataDescriptions = false;

	UPROPERTY()
	bool bInitialized = false;

	UPROPERTY()
	bool bHasStaticValidationErrors = false;

	// @todo_pcg: These are a bit of a hack to avoid supporting static/compilation logging in PCGNodeVisualLogs.h
	/** Log entries created and cached on initialization. Are logged during AreKernelSettingsValid(), unless PCG_KERNEL_LOGGING_ENABLED is false. */
	UPROPERTY()
	TArray<FPCGKernelLogEntry> StaticLogEntries;

#if WITH_EDITOR
	TSet<FName> InternalPinLabels;
#endif

	mutable TObjectPtr<const UPCGSettings> ResolvedSettings = nullptr;
};

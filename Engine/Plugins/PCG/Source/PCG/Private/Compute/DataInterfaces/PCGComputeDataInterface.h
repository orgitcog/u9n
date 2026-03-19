// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGComputeCommon.h"

#include "ComputeFramework/ComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGComputeDataInterface.generated.h"

class UPCGComputeKernel;
class UPCGDataBinding;

UCLASS(Abstract, ClassGroup = (Procedural))
class UPCGComputeDataInterface : public UComputeDataInterface
{
	GENERATED_BODY()

public:
	/** Register a downstream pin (and optionally compute graph element pin alias). */
	virtual void AddDownstreamInputPin(FName InInputPinLabel, const FName* InOptionalInputPinLabelAlias = nullptr);

	/** Set the output pin label and label alias that this data interface is associated with. */
	virtual void SetOutputPin(FName InOutputPinLabel, const FName* InOptionalOutputPinLabelAlias = nullptr);

	void SetProducerSettings(const UPCGSettings* InProducerSettings);
	const UPCGSettings* GetProducerSettings() const;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

public:
	UPROPERTY()
	int32 GraphBindingIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<const UPCGComputeKernel> ProducerKernel;

	/** Label of output pin that this data interface is associated with. */
	UPROPERTY()
	FName OutputPinLabel;

	UPROPERTY()
	FName OutputPinLabelAlias;

	/** Generated PCG data will be assigned these labels. */
	UPROPERTY()
	TArray<FName> DownstreamInputPinLabelAliases;

protected:
	UPROPERTY()
	TSoftObjectPtr<const UPCGSettings> ProducerSettings;

	mutable TObjectPtr<const UPCGSettings> ResolvedProducerSettings = nullptr;
};

UCLASS(Abstract, ClassGroup (Procedural))
class UPCGComputeDataProvider : public UComputeDataProvider
{
	GENERATED_BODY()

public:
	virtual void Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask) override;
	virtual void Reset() override;

	/** Called before PrepareForExecute to allow data providers to do any readbacks and finalize data descriptions. */
	virtual bool PerformPreExecuteReadbacks_GameThread(UPCGDataBinding* InBinding) { return true; }

	/* Called prior to GetRenderProxy and execution. Any processing of data descriptions should be done here (after PerformPreExecuteReadbacks has been called). */
	virtual bool PrepareForExecute_GameThread(UPCGDataBinding* InBinding) { return true; }

	/** Return true when all done. */
	virtual bool PostExecute(UPCGDataBinding* InBinding) { return true; }

	/** Release any handles to transient resources like GPU buffers. */
	virtual void ReleaseTransientResources(const TCHAR* InReason = nullptr) {}

	const UPCGSettings* GetProducerSettings() const;
	void SetProducerSettings(const UPCGSettings* InSettings);

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:
#if WITH_EDITOR
	void NotifyProducerUploadedData(UPCGDataBinding* InBinding);
#endif

public:
	int32 GraphBindingIndex = INDEX_NONE;
	FName OutputPinLabel;
	FName OutputPinLabelAlias;
	TArray<FName> DownstreamInputPinLabelAliases;

	/** Bumped each time the data provider is initialized or reset, so that async callbacks can detect if they originated from a previous usage of the data provider and no-op. */
	std::atomic<uint64> GenerationCounter = 0;

protected:
	UPROPERTY()
	TObjectPtr<const UPCGComputeKernel> ProducerKernel;

private:
	/** Settings of node that produces this data, normally the upstream node. */
	UPROPERTY()
	TSoftObjectPtr<const UPCGSettings> ProducerSettings;

	mutable TObjectPtr<const UPCGSettings> ResolvedProducerSettings = nullptr;
};

UCLASS(Abstract, ClassGroup = (Procedural))
class UPCGExportableDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	void SetRequiresExport(bool bInRequiresExport) { bRequiresExport = bInRequiresExport; }
	bool GetRequiresExport() const { return bRequiresExport; }

protected:
	/** Whether this data is passed to downstream tasks outside of this compute graph. */
	UPROPERTY()
	bool bRequiresExport = false;
};

UCLASS(Abstract, ClassGroup (Procedural))
class UPCGExportableDataProvider : public UPCGComputeDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	virtual void Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask) override;
	virtual void Reset() override;
	//~ End UComputeDataInterface Interface

	//~ Begin UPCGComputeDataProvider Interface
	virtual bool PrepareForExecute_GameThread(UPCGDataBinding* InBinding) override;
	//~ End UPCGComputeDataProvider Interface

	/** Returns true if the generated buffer should be exported to a persistent buffer to be passed to downstream nodes. */
	bool IsExportRequired() const { return ExportMode != EPCGExportMode::NoExport; }

	EPCGExportMode GetExportMode() const { return ExportMode; }
	UPCGDataBinding* GetDataBinding() const { return Binding.Get(); }
	const FPCGDataCollectionDesc* GetPinDescription() const { return PinDesc; }

	DECLARE_EVENT(UPCGExportableDataProvider, FOnDataExported);
	FOnDataExported& OnDataExported_GameThread() { return OnDataExported; }

protected:
	EPCGExportMode ExportMode = EPCGExportMode::NoExport;
	FOnDataExported OnDataExported;

	TWeakObjectPtr<UPCGDataBinding> Binding;
	const FPCGDataCollectionDesc* PinDesc = nullptr;
};

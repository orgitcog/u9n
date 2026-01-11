// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"

#include "Compute/PCGDataBinding.h"

#include "PCGDataCollectionUploadDataInterface.generated.h"

class FPCGDataUploadAdaptor;

/** Compute Framework Data Interface for reading PCG data. */
UCLASS(ClassGroup = (Procedural))
class UPCGDataCollectionUploadDataInterface : public UPCGDataCollectionDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface
};

/** Compute Framework Data Provider for reading a PCG Data Collection. */
UCLASS()
class UPCGDataProviderDataCollectionUpload : public UPCGDataCollectionDataProvider
{
	GENERATED_BODY()

public:
	virtual void ReleaseTransientResources(const TCHAR* InReason = nullptr) override;

	//~ Begin UComputeDataProvider Interface
	virtual void Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask) override;
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	virtual void Reset() override;
	//~ End UComputeDataProvider Interface

	//~ Begin UPCGComputeDataProvider Interface
	virtual bool PrepareForExecute_GameThread(UPCGDataBinding* InBinding) override;
	//~ End UPCGComputeDataProvider Interface

	TArray<FName> DownstreamInputPinLabels;

	TSharedPtr<FPCGDataUploadAdaptor> DataAdaptor;
};

class FPCGDataProviderDataCollectionUploadProxy : public FPCGDataCollectionDataProviderProxy
{
public:
	FPCGDataProviderDataCollectionUploadProxy(
		TSharedPtr<FPCGDataUploadAdaptor> InDataAdaptor,
		TWeakObjectPtr<UPCGDataCollectionDataProvider> InDataProvider,
		const FPCGDataCollectionDesc* InPinDesc,
		FName InOutputPinLabel,
		FName InOutputPinLabelAlias,
		EPCGExportMode InExportMode,
		bool bInZeroInitialize,
		uint32 InElementCountMultiplier,
		bool bInExposeElementCounters);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	virtual FRDGBufferSRVRef GetBufferToGraphStringKeySRV(FRDGBuilder& GraphBuilder, int32& OutNumRemappedStringKeys) const override;

protected:
	TSharedPtr<FPCGDataUploadAdaptor> DataAdaptor;

	TRefCountPtr<FRDGPooledBuffer> ExternalBufferForReuse;
};

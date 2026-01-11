// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGCopyPointsAnalysisDataInterface.generated.h"

class FPCGCopyPointsAnalysisDataInterfaceParameters;
class UPCGCopyPointsSettings;

/** Data Interface to marshal Copy Points settings to the GPU. */
UCLASS(ClassGroup = (Procedural))
class UPCGCopyPointsAnalysisDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGCopyPointsAnalysis"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface
};

UCLASS()
class UPCGCopyPointsAnalysisDataProvider : public UPCGComputeDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	virtual void Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask) override;
	virtual bool PrepareForExecute_GameThread(UPCGDataBinding* InBinding) override;
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	//~ End UComputeDataProvider Interface

public:
	UPROPERTY()
	TObjectPtr<const UPCGCopyPointsSettings> Settings;

	UPROPERTY()
	int32 MatchAttributeId = INDEX_NONE;

	UPROPERTY()
	int32 SelectedFlagAttributeId = INDEX_NONE;

	UPROPERTY()
	bool bCopyEachSourceOnEveryTarget = true;
};

class FPCGCopyPointsAnalysisDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	struct FCopyPointsAnalysisData_RenderThread
	{
		int32 MatchAttributeId = INDEX_NONE;
		int32 SelectedFlagAttributeId = INDEX_NONE;
		bool bCopyEachSourceOnEveryTarget = true;
	};

	FPCGCopyPointsAnalysisDataProviderProxy(FCopyPointsAnalysisData_RenderThread InData)
		: Data(MoveTemp(InData))
	{}

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	using FParameters = FPCGCopyPointsAnalysisDataInterfaceParameters;

	FCopyPointsAnalysisData_RenderThread Data;
};

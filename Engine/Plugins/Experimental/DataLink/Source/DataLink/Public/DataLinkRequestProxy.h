// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "DataLinkRequestProxy.generated.h"

class IDataLinkSinkProvider;
class FDataLinkExecutor;
enum class EDataLinkExecutionResult : uint8;
struct FDataLinkInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDataLinkRequestComplete, const FInstancedStruct&, OutputData, EDataLinkExecutionResult, ExecutionResult);

UCLASS(MinimalAPI)
class UDataLinkRequestProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnDataLinkRequestComplete OnRequestComplete;

	UFUNCTION(BlueprintCallable, Category="Data Link", meta=(BlueprintInternalUseOnly))
	static DATALINK_API UDataLinkRequestProxy* CreateRequestProxy(FDataLinkInstance InDataLinkInstance
		, UObject* InExecutionContext
		, TScriptInterface<IDataLinkSinkProvider> InDataLinkSinkProvider);

	DATALINK_API void ProcessRequest(FDataLinkInstance&& InDataLinkInstance
		, UObject* InExecutionContext
		, TScriptInterface<IDataLinkSinkProvider> InDataLinkSinkProvider);

private:
	void OnExecutionFinished(const FDataLinkExecutor& InExecutor, EDataLinkExecutionResult InResult, FConstStructView InOutputData);

	TSharedPtr<FDataLinkExecutor> DataLinkExecutor;
};

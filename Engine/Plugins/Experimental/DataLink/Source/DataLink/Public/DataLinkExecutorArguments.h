// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "DataLinkDelegates.h"
#include "DataLinkInstance.h"
#include "StructUtils/StructView.h"

class UDataLinkGraph;
struct FDataLinkSink;

class FDataLinkExecutorArguments
{
	friend class FDataLinkExecutor;

public:
	DATALINK_API FDataLinkExecutorArguments(const FDataLinkInstance& InInstance);

	DATALINK_API FDataLinkExecutorArguments(FDataLinkInstance&& InInstance);

#if WITH_DATALINK_CONTEXT
	/** Added context information of the execution */
	DATALINK_API FDataLinkExecutorArguments&& SetContextName(FString&& InContextName);
#endif

	/** Sets the object responsible for this execution */
	DATALINK_API FDataLinkExecutorArguments&& SetContextObject(UObject* InContextObject);

	/** Sets the Sink to use. If unset, the Executor will create its own temporary sink */
	DATALINK_API FDataLinkExecutorArguments&& SetSink(const TSharedPtr<FDataLinkSink>& InSink);

	/** The delegate to call when the execution finishes */
	DATALINK_API FDataLinkExecutorArguments&& SetOnFinish(FOnDataLinkExecutionFinished&& InDelegate);

private:
	FDataLinkInstance Instance;

#if WITH_DATALINK_CONTEXT
	FString ContextName;
#endif

	TObjectPtr<UObject> ContextObject;

	TSharedPtr<FDataLinkSink> Sink;

	FOnDataLinkExecutionFinished OnFinishDelegate;
};

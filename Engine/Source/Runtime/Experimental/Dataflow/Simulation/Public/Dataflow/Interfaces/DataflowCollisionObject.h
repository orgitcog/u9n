// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Dataflow/DataflowSimulationInterface.h"
#include "Dataflow/DataflowSimulationProxy.h"
#include "DataflowCollisionObject.generated.h"

#define UE_API DATAFLOWSIMULATION_API

/**
 * Dataflow collision object proxy (PT)
 */
USTRUCT()
struct FDataflowCollisionObjectProxy : public FDataflowSimulationProxy
{
	GENERATED_BODY()
	
	UE_API FDataflowCollisionObjectProxy();
	virtual ~FDataflowCollisionObjectProxy() override = default;

	/** Get the proxy script struct */
	virtual const UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}
};

UINTERFACE(MinimalAPI)
class UDataflowCollisionObjectInterface : public UDataflowSimulationInterface
{
	GENERATED_BODY()
};

/**
 * Dataflow collision object interface to send/receive datas (GT <-> PT)
 */
class IDataflowCollisionObjectInterface : public IDataflowSimulationInterface
{
	GENERATED_BODY()
public:
	UE_API IDataflowCollisionObjectInterface();

	/** Get the simulation type */
	virtual FString GetSimulationType() const override
	{
		return FDataflowCollisionObjectProxy::StaticStruct()->GetName();
	}
};

#undef UE_API

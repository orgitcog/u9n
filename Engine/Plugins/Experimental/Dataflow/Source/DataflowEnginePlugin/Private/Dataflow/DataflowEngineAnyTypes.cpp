// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEngineAnyTypes.h"

#include "Dataflow/DataflowAnyTypeRegistry.h"

namespace UE::Dataflow
{
	void RegisterEngineAnyTypes()
	{
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowDynamicMeshArray);
	}
}


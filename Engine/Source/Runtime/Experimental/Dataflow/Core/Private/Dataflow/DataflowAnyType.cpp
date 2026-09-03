// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowAnyType.h"
#include "Dataflow/DataflowAnyTypeRegistry.h"

namespace UE::Dataflow
{
	void RegisterAnyTypes()
	{
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowAnyType);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowAllTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowArrayTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowNumericTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowVectorTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowStringTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowBoolTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowTransformTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowStringConvertibleTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowUObjectConvertibleTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowSelectionTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowRotationTypes);

		// array types 
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowVectorArrayTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowNumericArrayTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowStringArrayTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowBoolArrayTypes);
		UE_DATAFLOW_REGISTER_ANYTYPE(FDataflowTransformArrayTypes);
	}

	bool AreTypesCompatible(FName TypeA, FName TypeB)
	{
		return FAnyTypesRegistry::AreTypesCompatibleStatic(TypeA, TypeB);
	}
}
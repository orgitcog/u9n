// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"

#include "DataflowSelectionNodes.generated.h"

#define UE_API DATAFLOWNODES_API



USTRUCT(meta = (DataflowFlesh))
struct FSelectionSetDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSelectionSetDataflowNode, "SelectionSet", "Dataflow", "")

public:
	typedef TArray<int32> DataType;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
		FString Indices = FString("1 2 3");

	UPROPERTY(meta = (DataflowOutput, DisplayName = "SelectionSet"))
		TArray<int32> IndicesOut;

	FSelectionSetDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&IndicesOut);
	}

	UE_API virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


namespace UE::Dataflow
{
	void RegisterSelectionNodes();
}

#undef UE_API

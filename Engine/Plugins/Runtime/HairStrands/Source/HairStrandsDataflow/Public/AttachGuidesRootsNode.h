// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GroomAsset.h"
#include "ChaosLog.h"
#include "Dataflow/DataflowConnectionTypes.h"
#include "Dataflow/DataflowCore.h"

#include "AttachGuidesRootsNode.generated.h"

/** Attach the guides roots by setting their kinematic weights to 1.0f */
USTRUCT(meta = (Experimental, DataflowGroom))
struct FAttachGuidesRootsDataflowNode : public FDataflowNode
{
	GENERATED_BODY()
	
	DATAFLOW_NODE_DEFINE_INTERNAL(FAttachGuidesRootsDataflowNode, "AttachGuidesRoots", "Groom", "")
	DATAFLOW_NODE_RENDER_TYPE("GuidesRender", FName("FGroomCollection"), "Collection")

public:
	
	FAttachGuidesRootsDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
		RegisterOutputConnection(&KinematicWeightsKey);
	}

	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface

	/** Managed array collection to be used to store datas */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;
	
	/** Group index on which the roots will be attached. -1 will attach all the groups */
	UPROPERTY(EditAnywhere, Category="Groups", meta = (DisplayName = "Group Index"))
	int32 GroupIndex = INDEX_NONE;
	
	/** Point Kinematic weights key to be used in other nodes if necessary */
	UPROPERTY(meta = (DisplayName = "Kinematic Weights", DataflowOutput))
	FCollectionAttributeKey KinematicWeightsKey;
};


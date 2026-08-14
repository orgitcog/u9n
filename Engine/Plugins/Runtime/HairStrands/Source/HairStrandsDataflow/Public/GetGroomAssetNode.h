// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GroomAsset.h"
#include "ChaosLog.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"
#include "GroomCollectionFacades.h"

#include "GetGroomAssetNode.generated.h"

/** Get the groom asset guides */
USTRUCT(meta = (Experimental, DataflowGroom))
struct FGetGroomAssetDataflowNode : public FDataflowNode
{
	GENERATED_BODY()
	
	DATAFLOW_NODE_DEFINE_INTERNAL(FGetGroomAssetDataflowNode, "GetGroomAsset", "Groom", "")

public:
	
	FGetGroomAssetDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&Collection);
	}

	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual TArray<UE::Dataflow::FRenderingParameter> GetRenderParametersImpl() const override;
	//~ End FDataflowNode interface

	/** Input asset to read the guides from */
	UPROPERTY(EditAnywhere, Category = "Groom", meta = (DisplayName = "GroomAsset"))
	TObjectPtr<const UGroomAsset> GroomAsset = nullptr;

	/** Type of curves to use to fill the groom collection (guides/strands) */
	UPROPERTY(EditAnywhere, Category = "Groom", meta = (DisplayName = "Display Curves"))
	EGroomCollectionType CurvesType = EGroomCollectionType::Strands;

	/** Managed array collection used to store the guides */
	UPROPERTY(meta = (DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;
};

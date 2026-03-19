// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GroomAsset.h"
#include "ChaosLog.h"
#include "BuildCardsSettingsNode.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"

#include "GenerateCardsClumpsNode.generated.h"

USTRUCT()
struct FCardsClumpsSettings
{
	GENERATED_BODY()

	// Card filter name for which we will override the number of cards/flyaways
	UPROPERTY(EditAnywhere, Category="Groom Cards")
	FName FilterName = NAME_None;

	// Total number of cards to generate for this LOD settings
	UPROPERTY(EditAnywhere, Category="Groom Cards", meta=(ClampMin="1", ClampMax="100000"))
	int32 NumCards = 200;

	// Maximum number of cards to assign to flyaway strands
	UPROPERTY(EditAnywhere, Category="Groom Cards", meta=(ClampMin="0", ClampMax="1000"))
	int32 NumFlyaways = 10;
};

/** Generate the clumps used to build the cards from the strands */
USTRUCT(meta = (Experimental, DataflowGroom))
struct FGenerateCardsClumpsNode : public FDataflowNode
{
	GENERATED_BODY()
	
	DATAFLOW_NODE_DEFINE_INTERNAL(FGenerateCardsClumpsNode, "GenerateCardsClumps", "Groom", "")
	DATAFLOW_NODE_RENDER_TYPE("ClumpsRender", FName("FCardsCollection"), "Collection")

public:

	static const FName CurveClumpIndicesAttribute;
	static const FName ObjectNumClumpsAttribute;
	
	FGenerateCardsClumpsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&CardsSettings);
		RegisterOutputConnection(&Collection, &Collection);
		RegisterOutputConnection(&CardsSettings, &CardsSettings);
	}

	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface
	
	/** Per LOD settings for each cards generation */
	UPROPERTY(EditAnywhere, Category = "GroomCards")
	TArray<FCardsClumpsSettings> ClumpsSettings;

	/** Managed array collection to be used to store datas */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;

	/** Generator settings to be used */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Cards Settings", DataflowPassthrough  = "CardsSettings"));
	TArray<FGroomCardsSettings> CardsSettings;
};


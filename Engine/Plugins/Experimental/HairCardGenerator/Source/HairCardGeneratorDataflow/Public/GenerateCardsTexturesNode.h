// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GroomAsset.h"
#include "ChaosLog.h"
#include "BuildCardsSettingsNode.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"

#include "GenerateCardsTexturesNode.generated.h"

USTRUCT()
struct FCardsTextureSettings
{
	GENERATED_BODY()

	// Card filter name for which we will override the number of textures
	UPROPERTY(EditAnywhere, Category="Groom Cards")
	FName FilterName = NAME_None;

	// Total number of triangles to generate for this LOD settings
	UPROPERTY(EditAnywhere, Category="Groom Cards")
	int32 NumTextures = 75;
};

/** Generate the clumps used to build the cards from the strands */
USTRUCT(meta = (Experimental, DataflowGroom))
struct FGenerateCardsTexturesNode : public FDataflowNode
{
	GENERATED_BODY()
	
	DATAFLOW_NODE_DEFINE_INTERNAL(FGenerateCardsTexturesNode, "GenerateCardsTextures", "Groom", "")
	DATAFLOW_NODE_RENDER_TYPE("TextureRender", FName("FCardsCollection"), "Collection")

public:


	static const FName CardsObjectsGroup;
	static const FName ObjectTextureIndicesAttribute;
	static const FName VertexTextureUVsAttribute;
	
	FGenerateCardsTexturesNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
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
	TArray<FCardsTextureSettings> TextureSettings;

	/** Managed array collection to be used to store datas */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;

	/** Generator settings to be used */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "CardsSettings", DataflowPassthrough  = "CardsSettings"));
	TArray<FGroomCardsSettings> CardsSettings;
};


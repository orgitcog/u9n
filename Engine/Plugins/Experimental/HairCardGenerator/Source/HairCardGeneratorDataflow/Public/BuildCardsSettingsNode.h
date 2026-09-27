// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GroomAsset.h"
#include "ChaosLog.h"
#include "HairCardGeneratorPluginSettings.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"

#include "BuildCardsSettingsNode.generated.h"

USTRUCT()
struct FGroomCardsSettings
{
	GENERATED_BODY()
	
	/** Generator settings to be built */
	UPROPERTY(EditAnywhere, Category = "Groom Cards");
	TObjectPtr<UHairCardGeneratorPluginSettings> GenerationSettings = nullptr;

	/** Generation flags to output the assets */
	UPROPERTY(EditAnywhere, Category = "Groom Cards");
	uint8 GenerationFlags = 0;

	/** Pipeline flags to generate clumps, geometry and textures */
	UPROPERTY(EditAnywhere, Category = "Groom Cards");
	uint8 PipelineFlags = 0;

	/** Groom asset to generate the */
    UPROPERTY(EditAnywhere, Category = "Groom Cards");
    TObjectPtr<UGroomAsset> GroomAsset = nullptr;
};

USTRUCT()
struct FGroomFilterSettings
{
	GENERATED_BODY()

	// Filter group name to be identified in the override settings
	UPROPERTY(EditAnywhere, Category="Groom Cards")
	FName FilterName;
	
	// LOD index of the filter group settings
	UPROPERTY(EditAnywhere, Category="Groom Cards")
	int32 LODIndex = -1;

	// Group index of the filter group settings
	UPROPERTY(EditAnywhere, Category="Groom Cards")
	int32 GroupIndex = 0;

	// Card group names that will belong to the filter group settings
	UPROPERTY(EditAnywhere, Category="Groom Cards")
	TArray<FName> CardGroups;
};

/** Build the cards generation settings */
USTRUCT(meta = (Experimental, DataflowGroom))
struct FBuildCardsSettingsNode : public FDataflowNode
{
	GENERATED_BODY()
	
	DATAFLOW_NODE_DEFINE_INTERNAL(FBuildCardsSettingsNode, "BuildCardsSettings", "Groom", "")

public:
	
	FBuildCardsSettingsNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
	: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterOutputConnection(&Collection, &Collection);
		RegisterOutputConnection(&CardsSettings);
	}

	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface

	/** Groom asset to build the cards settings from */
	UPROPERTY(EditAnywhere, Category = "Groom Cards")
	TObjectPtr<UGroomAsset> GroomAsset;
	
	/** Managed array collection to be used to store datas */
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough  = "Collection"))
	FManagedArrayCollection Collection;

	/** Generator cards settings to be built */
	UPROPERTY(meta = (DataflowOutput));
	TArray<FGroomCardsSettings> CardsSettings;
	
	/** List of filter setings to override */
	UPROPERTY(EditAnywhere, Category = "Groom Cards")
	TArray<FGroomFilterSettings> FilterSettings;
};


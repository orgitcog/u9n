// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowTerminalNode.h"
#include "Dataflow/DataflowImage.h"

#include "DataflowTextureAssetNodes.generated.h"

class UTexture2D;

/*
* terminal node to a save a dependent 2D texture
*/
USTRUCT(meta = (DataflowTerminal))
struct FDataflowTextureTerminalNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowTextureTerminalNode, "TextureTerminal", "Terminal", "")

private:
	UPROPERTY(meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Image"))
	FDataflowImage Image;

	UPROPERTY(EditAnywhere, Category = Asset, meta = (DataflowInput))
	TObjectPtr<UTexture2D> TextureAsset = nullptr;

	virtual void Evaluate(UE::Dataflow::FContext& Context) const override;
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, UE::Dataflow::FContext& Context) const override;

public:
	FDataflowTextureTerminalNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
};


/**
* Import a texture asset as an image. Texture must have CPU availability.
*/
USTRUCT(Meta = (Experimental))
struct FDataflowTextureToImageNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowTextureToImageNode, "TextureToImage", "Image", "Texture Image")

public:

	FDataflowTextureToImageNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:

	UPROPERTY(EditAnywhere, Category = "Texture", meta = (DataflowInput))
	TObjectPtr<UTexture2D> TextureAsset = nullptr;

	UPROPERTY(meta = (DataflowOutput))
	FDataflowImage Image;

	//~ Begin FDataflowNode interface
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface
};


/**
* Create a transient texture asset from an image
*/
USTRUCT(meta = (DataflowTerminal))
struct FDataflowImageToTextureNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowImageToTextureNode, "ImageToTexture", "Image", "Image Texture")

private:
	UPROPERTY(meta = (DataflowInput, DataflowIntrinsic))
	FDataflowImage Image;

	UPROPERTY(EditAnywhere, Category = "Texture", meta = (DataflowInput))
	FName TextureName;

	UPROPERTY(meta = (DataflowOutput))
	TObjectPtr<UTexture2D> TransientTexture = nullptr;

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

public:
	FDataflowImageToTextureNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
};



namespace UE::Dataflow
{
	void RegisterTextureAssetNodes();
}


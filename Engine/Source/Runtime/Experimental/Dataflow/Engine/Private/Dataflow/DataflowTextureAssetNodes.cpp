// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowTextureAssetNodes.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Engine/Texture2D.h"

namespace UE::Dataflow
{
	void RegisterTextureAssetNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowTextureTerminalNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowTextureToImageNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowImageToTextureNode);
	}
}

namespace DataflowTextureAssetNodes::Private
{
	void UpdateTexture2DFromImage(UTexture2D& InTexture, const FImage& InImage)
	{
		// convert to the BGRA8
		FImage ConvertedImage(InImage);
		ConvertedImage.ChangeFormat(ERawImageFormat::Type::BGRA8, EGammaSpace::Linear);

#if WITH_EDITOR
		InTexture.PreEditChange(nullptr);
#endif

#if WITH_EDITORONLY_DATA
		InTexture.Source.Init(ConvertedImage);
#endif
		InTexture.UpdateResource();

#if WITH_EDITOR
		InTexture.PostEditChange();
#endif
	}
}


////////////////////////////////////////////////////////////////////////////////////////////

FDataflowTextureTerminalNode::FDataflowTextureTerminalNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowTerminalNode(InParam, InGuid)
{
	RegisterInputConnection(&Image);
	RegisterInputConnection(&TextureAsset);
	RegisterOutputConnection(&Image, &Image);
}

void FDataflowTextureTerminalNode::Evaluate(UE::Dataflow::FContext& Context) const
{
	// forward values
	SafeForwardInput(Context, &Image, &Image);
}


void FDataflowTextureTerminalNode::SetAssetValue(TObjectPtr<UObject> Asset, UE::Dataflow::FContext& Context) const
{
	UTexture2D* AssetToSet = Cast<UTexture2D>(Asset.Get());
	if (!AssetToSet)
	{
		// use the input instead
		AssetToSet = GetValue(Context, &TextureAsset);
	}

	if (AssetToSet)
	{
		const FDataflowImage& InImage = GetValue(Context, &Image);
		if (InImage.GetWidth() > 0 && InImage.GetHeight() > 0)
		{
			DataflowTextureAssetNodes::Private::UpdateTexture2DFromImage(*AssetToSet, InImage.GetImage());
		}
	}
}


FDataflowTextureToImageNode::FDataflowTextureToImageNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&TextureAsset);
	RegisterOutputConnection(&Image);
}

void FDataflowTextureToImageNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Image))
	{
		if (const TObjectPtr<UTexture2D> InTextureAsset = GetValue(Context, &TextureAsset))
		{
			FImage TempImage;
			if (const FSharedImageConstRef CpuCopy = InTextureAsset->GetCPUCopy())
			{
				CpuCopy->CopyTo(TempImage, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
			}
#if WITH_EDITORONLY_DATA
			else if (InTextureAsset->Source.IsValid())
			{
				FImage MipImage;
				InTextureAsset->Source.GetMipImage(MipImage, 0);
				MipImage.CopyTo(TempImage, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
			}
#endif
			else
			{
				// TODO: Handle more ways to get image data from textures
				Context.Warning(TEXT("Unable to read image from texture"));

				FDataflowImage EmptyImage;
				SetValue(Context, EmptyImage, &Image);
				return;
			}

			const TArrayView64<FVector4f> Buffer = { (FVector4f*)TempImage.AsRGBA32F().GetData(), TempImage.AsRGBA32F().Num() };
			FDataflowImage OutImage;
			OutImage.CreateRGBA32F(TempImage.GetWidth(), TempImage.GetHeight());
			OutImage.CopyRGBAPixels(Buffer);

			SetValue(Context, OutImage, &Image);
			return;
		}

		FDataflowImage EmptyImage;
		SetValue(Context, EmptyImage, &Image);
	}
}


FDataflowImageToTextureNode::FDataflowImageToTextureNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Image);
	RegisterInputConnection(&TextureName);
	RegisterOutputConnection(&TransientTexture);
}


void FDataflowImageToTextureNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&TransientTexture))
	{
		const FName InName = GetValue(Context, &TextureName);
		UObject* Package = GetTransientPackage();
		const FName UniqueName = MakeUniqueObjectName(Package, UTexture2D::StaticClass(), InName);
		TObjectPtr<UTexture2D> OutTexture = NewObject<UTexture2D>(Package, UniqueName, RF_Transient);

		const FDataflowImage InImage = GetValue(Context, &Image);

		if (InImage.GetWidth() > 0 && InImage.GetHeight() > 0)
		{
			DataflowTextureAssetNodes::Private::UpdateTexture2DFromImage(*OutTexture, InImage.GetImage());
		}
		else
		{
			Context.Warning(TEXT("Input image is empty"));
		}

		SetValue(Context, OutTexture, &TransientTexture);
		return;
	}
}

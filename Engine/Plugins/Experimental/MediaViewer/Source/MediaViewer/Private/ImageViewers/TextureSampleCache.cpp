// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageViewers/TextureSampleCache.h"

#include "Engine/Texture.h"
#include "ImageViewers/TextureMipCache.h"
#include "Misc/ScopeLock.h"
#include "PixelFormat.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace UE::MediaViewer::Private
{

bool CanCopyTextureWithRHICommand(EPixelFormat InFormat)
{
	switch (InFormat)
	{
		// Textures encoding that cannot be read by the RHI.
		// Based on the list of invalid texture formats in the Metal RHI (see MetalRHI.cpp)
		case EPixelFormat::PF_DXT1:
		case EPixelFormat::PF_DXT3:
		case EPixelFormat::PF_DXT5:
		case EPixelFormat::PF_BC4:
		case EPixelFormat::PF_BC5:
		case EPixelFormat::PF_BC6H:
		case EPixelFormat::PF_BC7:
		case EPixelFormat::PF_DepthStencil:
		case EPixelFormat::PF_ShadowDepth:
		case EPixelFormat::PF_D24:
		case EPixelFormat::PF_A1:
		case EPixelFormat::PF_PVRTC2:
		case EPixelFormat::PF_PVRTC4:
		case EPixelFormat::PF_R5G6B5_UNORM:
		case EPixelFormat::PF_B5G5R5A1_UNORM:
		case EPixelFormat::PF_ATC_RGB:
		case EPixelFormat::PF_ATC_RGBA_E:
		case EPixelFormat::PF_ATC_RGBA_I:
		case EPixelFormat::PF_X24_G8:
		case EPixelFormat::PF_ETC1:
		case EPixelFormat::PF_ETC2_RGB:
		case EPixelFormat::PF_ETC2_RGBA:
		case EPixelFormat::PF_ASTC_4x4:
		case EPixelFormat::PF_ASTC_6x6:
		case EPixelFormat::PF_ASTC_8x8:
		case EPixelFormat::PF_ASTC_10x10:
		case EPixelFormat::PF_ASTC_12x12:
		case EPixelFormat::PF_ASTC_4x4_HDR:
		case EPixelFormat::PF_ASTC_6x6_HDR:
		case EPixelFormat::PF_ASTC_8x8_HDR:
		case EPixelFormat::PF_ASTC_10x10_HDR:
		case EPixelFormat::PF_ASTC_12x12_HDR:
		case EPixelFormat::PF_L8:
		case EPixelFormat::PF_R16G16B16A16_SNORM:
		case EPixelFormat::PF_PLATFORM_HDR_0:
		case EPixelFormat::PF_PLATFORM_HDR_1:
		case EPixelFormat::PF_NV12:
		case EPixelFormat::PF_ETC2_R11_EAC:
		case EPixelFormat::PF_ETC2_RG11_EAC:
		case EPixelFormat::PF_R32G32B32_UINT:
		case EPixelFormat::PF_R32G32B32_SINT:
		case EPixelFormat::PF_R32G32B32F:
		case EPixelFormat::PF_R64_UINT:
		case EPixelFormat::PF_R9G9B9EXP5:
		case EPixelFormat::PF_P010:
		case EPixelFormat::PF_ASTC_4x4_NORM_RG:
		case EPixelFormat::PF_ASTC_6x6_NORM_RG:
		case EPixelFormat::PF_ASTC_8x8_NORM_RG:
		case EPixelFormat::PF_ASTC_10x10_NORM_RG:
		case EPixelFormat::PF_ASTC_12x12_NORM_RG:
		case EPixelFormat::PF_R8G8B8:
			return false;

		default:
			return true;
	}
}

FTextureSampleCache::FTextureSampleCache()
	: PixelFormat(PF_Unknown)
{
}

FTextureSampleCache::FTextureSampleCache(TNotNull<UTexture*> InTexture, EPixelFormat InPixelFormat)
	: Texture(InTexture)
	, PixelFormat(InPixelFormat)
	, SampleCS(FCriticalSection())
{
	if (!CanCopyTextureWithRHICommand(PixelFormat))
	{
		MipCache = MakeShared<FTextureMipCache>(InTexture);
	}
}

bool FTextureSampleCache::IsValid() const
{
	return ::IsValid(Texture);
}

bool FTextureSampleCache::NeedsUpdate(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime) const
{
	FScopeLock Lock(&SampleCS);
	return bDirty || !PixelColorSample.IsSet() || InPixelCoordinates != PixelColorSample->Coordinates
		|| (InTime.IsSet() && (PixelColorSample->Time != InTime.GetValue()));
}

const FLinearColor* FTextureSampleCache::GetPixelColor(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime)
{
	if (!Texture)
	{
		return nullptr;
	}

	const bool bNeedsUpdate = NeedsUpdate(InPixelCoordinates, InTime);

	if (bNeedsUpdate)
	{
		if (!CanCopyTextureWithRHICommand(PixelFormat))
		{
			SetPixelColor_Mip(InPixelCoordinates, InTime);
		}
		else
		{
			SetPixelColor_RHI(InPixelCoordinates, InTime);
		}
	}

	{
		FScopeLock Lock(&SampleCS);

		if (PixelColorSample.IsSet())
		{
			return &PixelColorSample->Color;
		}
	}

	return nullptr;
}

void FTextureSampleCache::MarkDirty()
{
	FScopeLock Lock(&SampleCS);
	bDirty = true;
}

void FTextureSampleCache::Invalidate()
{
	{
		FScopeLock Lock(&SampleCS);
		PixelColorSample.Reset();
	}

	if (MipCache.IsValid())
	{
		MipCache->Invalidate();
	}
}

FTextureSampleCache& FTextureSampleCache::operator=(const FTextureSampleCache& InOther)
{
	FScopeLock MyLock(&SampleCS);
	FScopeLock OtherLock(&InOther.SampleCS);

	Texture = InOther.Texture;
	PixelFormat = InOther.PixelFormat;
	bDirty = InOther.bDirty;
	PixelColorSample = InOther.PixelColorSample;
	MipCache = InOther.MipCache;
	
	return *this;
}

void FTextureSampleCache::SetPixelColor_RHI(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime)
{
	ENQUEUE_RENDER_COMMAND(GetPixelColors)
	(
		[ThisWeak = SharedThis(this).ToWeakPtr(), InPixelCoordinates, InTime](FRHICommandListImmediate& RHICmdList)
		{
			TSharedPtr<FTextureSampleCache> This = ThisWeak.Pin();

			if (!This.IsValid() || !This->IsValid())
			{
				return;
			}

			if (FTextureResource* TextureResource = This->Texture->GetResource())
			{
				const FIntVector Size = TextureResource->TextureRHI->GetSizeXYZ();
				TArray<FLinearColor> Data;
				FIntRect Rect(InPixelCoordinates.X, InPixelCoordinates.Y, InPixelCoordinates.X + 1, InPixelCoordinates.Y + 1);
				RHICmdList.ReadSurfaceData(TextureResource->TextureRHI, Rect, Data, FReadSurfaceDataFlags());

				if (!Data.IsEmpty())
				{
					{
						FScopeLock Lock(&This->SampleCS);

						This->PixelColorSample = {
							InPixelCoordinates,
							InTime,
							Data[0]
						};

						This->bDirty = false;
					}
					return;
				}
			}

			{
				FScopeLock Lock(&This->SampleCS);
				This->PixelColorSample.Reset();
			}
		}
	);
}

void FTextureSampleCache::SetPixelColor_Mip(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime)
{
	if (!MipCache.IsValid())
	{
		return;
	}

	const FImage* Mip = MipCache->GetMipImage(/* Mip level */ 0);

	if (!Mip)
	{
		FScopeLock Lock(&SampleCS);
		PixelColorSample.Reset();
		return;
	}

	{
		FScopeLock Lock(&SampleCS);

		PixelColorSample = {
			InPixelCoordinates,
			InTime,
			Mip->GetOnePixelLinear(InPixelCoordinates.X, InPixelCoordinates.Y)
		};

		bDirty = false;
	}
}

} // UE::MediaViewer::Private

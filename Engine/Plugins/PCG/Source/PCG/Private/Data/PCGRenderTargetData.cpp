// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGRenderTargetData.h"

#include "PCGContext.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGRenderTargetData)

void UPCGRenderTargetData::Initialize(UTextureRenderTarget2D* InRenderTarget, const FTransform& InTransform, bool bInSkipReadbackToCPU, bool bInTakeOwnershipOfRenderTarget)
{
	RenderTarget = InRenderTarget;
	Transform = InTransform;
	bSkipReadbackToCPU = bInSkipReadbackToCPU;
	bOwnsRenderTarget = bInTakeOwnershipOfRenderTarget;

	ColorData.Reset();

	if (RenderTarget)
	{
		Width = RenderTarget->SizeX;
		Height = RenderTarget->SizeY;

		if (!bSkipReadbackToCPU)
		{
			if (FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UPCGRenderTargetData::Initialize::ReadData);
				ETextureRenderTargetFormat Format = RenderTarget->RenderTargetFormat;
				if ((Format == RTF_RGBA16f) || (Format == RTF_RGBA32f) || (Format == RTF_RGBA8) || (Format == RTF_R8) || (Format == RTF_RGB10A2))
				{
					const FIntRect Rect = FIntRect(0, 0, RenderTarget->SizeX, RenderTarget->SizeY);
					const FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
					RTResource->ReadLinearColorPixels(ColorData, ReadPixelFlags, Rect);
				}
			}
		}

		// Never take resource ownership on assets.
		if (RenderTarget->IsAsset())
		{
			bOwnsRenderTarget = false;
		}
	}

	Bounds = FBox(EForceInit::ForceInit);
	Bounds += FVector(-1.0f, -1.0f, 0.0f);
	Bounds += FVector(1.0f, 1.0f, 0.0f);
	Bounds = Bounds.TransformBy(Transform);
}

void UPCGRenderTargetData::ReleaseTransientResources(const TCHAR* InReason)
{
	if (bOwnsRenderTarget && RenderTarget)
	{
		RenderTarget->ReleaseResource();
		RenderTarget = nullptr;
		bOwnsRenderTarget = false;
	}
}

void UPCGRenderTargetData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	// This data does not have a bespoke CRC implementation so just use a global unique data CRC.
	AddUIDToCrc(Ar);
}

UTexture* UPCGRenderTargetData::GetTexture() const
{
	return RenderTarget;
}

FTextureRHIRef UPCGRenderTargetData::GetTextureRHI() const
{
	// TODO: This makes no attempt to actually acquire the resource after it has been written or, nor does it ensure
	// resource transitions/barriers. Only works if RT is already populated and does not work if RT is rendered every frame.
	FTextureResource* Resource = RenderTarget ? RenderTarget->GetResource() : nullptr;
	return Resource ? Resource->GetTextureRHI() : nullptr;
}

UPCGSpatialData* UPCGRenderTargetData::CopyInternal(FPCGContext* Context) const
{
	UPCGRenderTargetData* NewRenderTargetData = FPCGContext::NewObject_AnyThread<UPCGRenderTargetData>(Context);

	CopyBaseTextureData(NewRenderTargetData);

	// TODO: We can't really support copying owned things at this point, so we will relinquish assumed ownership.
	if (!ensure(!bOwnsRenderTarget))
	{
		const_cast<UPCGRenderTargetData*>(this)->bOwnsRenderTarget = false;
	}

	NewRenderTargetData->RenderTarget = RenderTarget;
	NewRenderTargetData->bOwnsRenderTarget = false;

	return NewRenderTargetData;
}

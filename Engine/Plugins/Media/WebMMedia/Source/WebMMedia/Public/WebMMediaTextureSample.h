// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaTextureSample.h"
#include "MediaObjectPool.h"
#include "RHI.h"
#include "RHIUtilities.h"

#define UE_API WEBMMEDIA_API

class FRHITexture;

class FWebMMediaTextureSample
	: public IMediaTextureSample
	, public IMediaPoolable
{
public:
	UE_API FWebMMediaTextureSample();

public:
	UE_API void Initialize(FIntPoint InDisplaySize, FIntPoint InTotalSize, FTimespan InTime, FTimespan InDuration);
	UE_API void CreateTexture();

public:
	//~ IMediaTextureSample interface
	UE_API virtual const void* GetBuffer() override;
	UE_API virtual FIntPoint GetDim() const override;
	UE_API virtual FTimespan GetDuration() const override;
	UE_API virtual EMediaTextureSampleFormat GetFormat() const override;
	UE_API virtual FIntPoint GetOutputDim() const override;
	UE_API virtual uint32 GetStride() const override;
	UE_API virtual FRHITexture* GetTexture() const override;
	UE_API virtual FMediaTimeStamp GetTime() const override;
	UE_API virtual bool IsCacheable() const override;
	UE_API virtual bool IsOutputSrgb() const override;

public:
	//~ IMediaPoolable interface
	UE_API virtual void ShutdownPoolable() override;

public:
	UE_API TRefCountPtr<FRHITexture> GetTextureRef() const;

private:
	TRefCountPtr<FRHITexture> Texture;
	FTimespan Time;
	FTimespan Duration;
	FIntPoint TotalSize;
	FIntPoint DisplaySize;
};

class FWebMMediaTextureSamplePool : public TMediaObjectPool<FWebMMediaTextureSample> { };

#undef UE_API

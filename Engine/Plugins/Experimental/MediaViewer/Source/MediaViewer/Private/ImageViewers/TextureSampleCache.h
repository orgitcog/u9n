// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

#include "HAL/CriticalSection.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Misc/NotNull.h"
#include "Misc/Optional.h"
#include "Misc/Timespan.h"

class UTexture;
enum EPixelFormat : uint8;

namespace UE::MediaViewer::Private
{

class FTextureMipCache;

class FTextureSampleCache : public TSharedFromThis<FTextureSampleCache>
{
public:
	FTextureSampleCache();
	FTextureSampleCache(TNotNull<UTexture*> InTexture, EPixelFormat InPixelFormat);

	bool IsValid() const;

	const FLinearColor* GetPixelColor(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime = {});

	void MarkDirty();

	void Invalidate();

	FTextureSampleCache& operator=(const FTextureSampleCache& InOther);

protected:
	struct FPixelColorSample
	{
		FIntPoint Coordinates;
		TOptional<FTimespan> Time;
		FLinearColor Color;
	};

	UTexture* Texture = nullptr;
	EPixelFormat PixelFormat;
	bool bDirty = true;
	TOptional<FPixelColorSample> PixelColorSample;
	TSharedPtr<FTextureMipCache> MipCache;
	mutable FCriticalSection SampleCS;

	void SetPixelColor_RHI(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime);

	void SetPixelColor_Mip(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime);

	bool NeedsUpdate(const FIntPoint& InPixelCoordinates, const TOptional<FTimespan>& InTime) const;
};

} // UE::MediaViewer::Private

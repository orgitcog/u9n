// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImageViewer/MediaImageViewer.h"

namespace UE::MediaViewer::Private
{

class FNullImageViewer : public FMediaImageViewer
{
public:
	static TSharedRef<FNullImageViewer> GetNullImageViewer();

	FNullImageViewer();
	virtual ~FNullImageViewer() = default;

	//~ Begin FMediaImageViewer
	virtual TOptional<TVariant<FColor, FLinearColor>> GetPixelColor(const FIntPoint& InPixelCoords, int32 InMipLevel) const override;
	//~ End FMediaImageViewer

protected:
	//~ Begin FMediaImageViewer
	virtual TSharedPtr<FMediaViewerLibraryItem> CreateLibraryItem() const override;
	virtual void PaintImage(FMediaImagePaintParams& InPaintParams, const FMediaImagePaintGeometry& InPaintGeometry) override;
	//~ End FMediaImageViewer
};

} // UE::MediaViewer::Private

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImageViewer/IMediaImageViewerFactory.h"
#include "ImageViewer/MediaImageViewer.h"
#include "Library/MediaViewerLibraryItem.h"
#include "UObject/GCObject.h"

#include "ImageViewers/TextureSampleCache.h"
#include "Math/Color.h"
#include "Misc/Optional.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

#include "MediaSourceImageViewer.generated.h"

class SHorizontalBox;
class UMediaSource;
class UMediaStream;

USTRUCT()
struct FMediaSourceImageViewerSettings
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Texture")
	TObjectPtr<UMediaSource> MediaSource = nullptr;
};

namespace UE::MediaViewer::Private
{

class FMediaSourceImageViewer : public FMediaImageViewer, public FGCObject
{
public:
	struct FFactory : public IMediaImageViewerFactory
	{
		FFactory()
		{
			Priority = 5000;
		}

		virtual bool SupportsAsset(const FAssetData& InAssetData) const override;
		virtual TSharedPtr<FMediaImageViewer> CreateImageViewer(const FAssetData& InAssetData) const override;
		virtual TSharedPtr<FMediaViewerLibraryItem> CreateLibraryItem(const FAssetData& InAssetData) const override;

		virtual bool SupportsObject(TNotNull<UObject*> InObject) const override;
		virtual TSharedPtr<FMediaImageViewer> CreateImageViewer(TNotNull<UObject*> InObject) const override;
		virtual TSharedPtr<FMediaViewerLibraryItem> CreateLibraryItem(TNotNull<UObject*> InObject) const override;

		virtual bool SupportsItemType(FName InItemType) const override;
		virtual TSharedPtr<FMediaViewerLibraryItem> CreateLibraryItem(const FMediaViewerLibraryItem& InSavedItem) const override;
	};

	struct FItem : public FMediaViewerLibraryItem, public FGCObject
	{
		TObjectPtr<UTexture> Texture;

		FItem(const FText& InName, const FText& InToolTip, bool bInTransient, const FString& InStringValue);

		FItem(const FGuid& InId, const FText& InName, const FText& InToolTip, bool bInTransient, const FString& InStringValue);

		//~ Begin FMediaViewerLibraryItem
		virtual TSharedPtr<FSlateBrush> CreateThumbnail() override;
		//~ End FMediaViewerLibraryItem

		//~ Begin FGCObject
		virtual FString GetReferencerName() const override;
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		//~ End FGCObject
	};

	struct FAssetItem : public FItem
	{
		FAssetItem(const FText& InName, const FText& InToolTip, bool bInTransient, TNotNull<UMediaSource*> InMediaSource);

		FAssetItem(const FGuid& InId, const FText& InName, const FText& InToolTip, bool bInTransient, TNotNull<UMediaSource*> InMediaSource);

		FAssetItem(FPrivateToken&& InPrivateToken, const FMediaViewerLibraryItem& InItem);

		virtual ~FAssetItem() override = default;

		//~ Begin FMediaViewerLibraryItem
		virtual FName GetItemType() const override;
		virtual FText GetItemTypeDisplayName() const override;
		virtual TSharedPtr<FMediaImageViewer> CreateImageViewer() const override;
		virtual TSharedPtr<FMediaViewerLibraryItem> Clone() const override;
		//~ End FMediaViewerLibraryItem
	};

	struct FExternalItem : public FItem
	{
		FExternalItem(const FText& InName, const FText& InToolTip, const FString& InFilePath);

		FExternalItem(const FGuid& InId, const FText& InName, const FText& InToolTip, const FString& InFilePath);

		FExternalItem(FPrivateToken&& InPrivateToken, const FMediaViewerLibraryItem& InItem);

		virtual ~FExternalItem() override = default;

		//~ Begin FMediaViewerLibraryItem
		virtual FName GetItemType() const override;
		virtual FText GetItemTypeDisplayName() const override;
		virtual TSharedPtr<FMediaImageViewer> CreateImageViewer() const override;
		virtual TSharedPtr<FMediaViewerLibraryItem> Clone() const override;
		//~ End FMediaViewerLibraryItem
	};

	static const FLazyName ItemTypeName_Asset;
	static const FLazyName ItemTypeName_File;

	FMediaSourceImageViewer(TNotNull<UMediaSource*> InMediaSource, const FText& InDisplayName);
	FMediaSourceImageViewer(const FGuid& InId, TNotNull<UMediaSource*> InMediaSource, const FText& InDisplayName);

	virtual ~FMediaSourceImageViewer() override;

	UMediaStream* GetMediaStream() const;

	//~ Begin FMediaImageViewer
	virtual TSharedPtr<FMediaViewerLibraryItem> CreateLibraryItem() const override;
	virtual TOptional<TVariant<FColor, FLinearColor>> GetPixelColor(const FIntPoint& InPixelCoords, int32 InMipLevel) const override;
	virtual TSharedPtr<FStructOnScope> GetCustomSettingsOnScope() const override;
	virtual TSharedPtr<SWidget> GetOverlayWidget(EMediaImageViewerPosition InPosition, const TSharedPtr<SMediaViewerTab>& InViewerTab) override;
	virtual void ExtendStatusBar(UE::MediaViewer::FMediaImageStatusBarExtender& InOutStatusBarExtender) override;
	//~ End FMediaImageViewer

	//~ Begin FGCObject
	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject

protected:
	FMediaSourceImageViewerSettings MediaSourceSettings;
	TObjectPtr<UMediaStream> MediaStream;
	TSharedPtr<FTextureSampleCache> SampleCache;

	void AddPlayerName(const TSharedRef<SHorizontalBox>& InStatusBar);

	//~ Begin FMediaImageViewer
	virtual void PaintImage(FMediaImagePaintParams& InPaintParams, const FMediaImagePaintGeometry& InPaintGeometry) override;
	//~ End FMediaImageViewer
};

} // UE::MediaViewer::Private

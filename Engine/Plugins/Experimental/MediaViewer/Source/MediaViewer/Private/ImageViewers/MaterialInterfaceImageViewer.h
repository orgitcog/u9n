// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImageViewer/IMediaImageViewerFactory.h"
#include "ImageViewer/MediaImageViewer.h"
#include "Library/MediaViewerLibraryItem.h"
#include "UObject/GCObject.h"

#include "ImageViewers/TextureSampleCache.h"
#include "Templates/SharedPointer.h"

#include "MaterialInterfaceImageViewer.generated.h"

class UMaterialInterface;
class UTexture;
class UTextureRenderTarget2D;

USTRUCT()
struct FMaterialInterfaceImageViewerSettings 
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Material")
	TObjectPtr<UMaterialInterface> MaterialInterface = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Material")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	/** Will render the material every frame. */
	UPROPERTY(EditAnywhere, Category = "Material")
	bool bRealTime = false;

	UPROPERTY(EditAnywhere, Category = "Material", meta = (ClampMin = 32, UIMin = 32, ClampMax = 2048, UIMax = 2048))
	int32 RenderTargetSize = 256;
};

namespace UE::MediaViewer::Private
{

class FMaterialInterfaceImageViewer : public FMediaImageViewer, public FGCObject
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
		TObjectPtr<UTextureRenderTarget2D> RenderTarget;

		FItem(const FText& InName, const FText& InToolTip, bool bInTransient, TNotNull<UMaterialInterface*> InMaterial);

		FItem(const FGuid& InId, const FText& InName, const FText& InToolTip, bool bInTransient, TNotNull<UMaterialInterface*> InMaterial);

		FItem(FPrivateToken&& InPrivateToken, const FMediaViewerLibraryItem& InItem);

		virtual ~FItem() override = default;

		void CreateRenderTargetThumbnail();

		//~ Begin FMediaViewerLibraryItem
		virtual FName GetItemType() const override;
		virtual FText GetItemTypeDisplayName() const override;
		virtual TSharedPtr<FSlateBrush> CreateThumbnail() override;
		virtual TSharedPtr<FMediaImageViewer> CreateImageViewer() const override;
		virtual TSharedPtr<FMediaViewerLibraryItem> Clone() const override;
		//~ End FMediaViewerLibraryItem

		//~ Begin FGCObject
		virtual FString GetReferencerName() const override;
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		//~ End FGCObject
	};

	static const FLazyName ItemTypeName;

	FMaterialInterfaceImageViewer(TNotNull<UMaterialInterface*> InMaterialInterface);
	FMaterialInterfaceImageViewer(const FGuid& InId, TNotNull<UMaterialInterface*> InMaterialInterface);

	virtual ~FMaterialInterfaceImageViewer() override;

	//~ Begin FMediaImageViewer
	virtual TSharedPtr<FMediaViewerLibraryItem> CreateLibraryItem() const override;
	virtual TOptional<TVariant<FColor, FLinearColor>> GetPixelColor(const FIntPoint& InPixelCoords, int32 InMipLevel) const override;
	virtual TSharedPtr<FStructOnScope> GetCustomSettingsOnScope() const override;
	virtual void PaintImage(FMediaImagePaintParams& InPaintParams, const FMediaImagePaintGeometry& InPaintGeometry) override;
	//~ End FMediaImageViewer

	//~ Begin FNotifyHook
	virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged) override;
	//~ End FNotifyHook

	//~ Begin FGCObject
	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject

protected:
	FMaterialInterfaceImageViewerSettings MaterialSettings;
	TSharedPtr<FTextureSampleCache> SampleCache;

	void CreateBrush();

	void RenderMaterial();

	void OnMaterialCompiled(UMaterialInterface* InMaterialInterface);
};

} // UE::MediaViewer::Private

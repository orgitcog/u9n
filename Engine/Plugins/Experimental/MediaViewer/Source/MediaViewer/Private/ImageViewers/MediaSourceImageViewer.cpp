// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageViewers/MediaSourceImageViewer.h"

#include "AssetRegistry/AssetData.h"
#include "Brushes/SlateImageBrush.h"
#include "DetailLayoutBuilder.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "IMediaStreamPlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/Vector2D.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaStream.h"
#include "MediaStreamSourceBlueprintLibrary.h"
#include "MediaTexture.h"
#include "MediaViewerUtils.h"
#include "Misc/Optional.h"
#include "Misc/TVariant.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/MediaImageStatusBarExtender.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SMediaSourceOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MediaSourceImageViewer"

namespace UE::MediaViewer::Private
{

constexpr const TCHAR* MediaSourceThumbnailTexturePath = TEXT("/Script/Engine.Texture2D'/Engine/EditorResources/SceneManager.SceneManager'");

UTexture2D* GetMediaSourceThumbnailTexture()
{
	return LoadObject<UTexture2D>(GetTransientPackage(), MediaSourceThumbnailTexturePath);
}

const FLazyName FMediaSourceImageViewer::ItemTypeName_Asset = TEXT("MediaAsset");
const FLazyName FMediaSourceImageViewer::ItemTypeName_File = TEXT("MediaFile");

bool FMediaSourceImageViewer::FFactory::SupportsAsset(const FAssetData& InAssetData) const
{
	if (UClass* Class = InAssetData.GetClass(EResolveClass::Yes))
	{
		return Class->IsChildOf<UMediaSource>();
	}

	return false;
}

TSharedPtr<FMediaImageViewer> FMediaSourceImageViewer::FFactory::CreateImageViewer(const FAssetData& InAssetData) const
{
	if (UObject* Object = InAssetData.GetAsset())
	{
		return CreateImageViewer(TNotNull<UObject*>(Object));
	}

	return nullptr;
}

TSharedPtr<FMediaViewerLibraryItem> FMediaSourceImageViewer::FFactory::CreateLibraryItem(const FAssetData& InAssetData) const
{
	if (UObject* Object = InAssetData.GetAsset())
	{
		return CreateLibraryItem(TNotNull<UObject*>(Object));
	}

	return nullptr;
}

bool FMediaSourceImageViewer::FFactory::SupportsObject(TNotNull<UObject*> InObject) const
{
	return InObject->IsA<UMediaSource>();
}

TSharedPtr<FMediaImageViewer> FMediaSourceImageViewer::FFactory::CreateImageViewer(TNotNull<UObject*> InObject) const
{
	if (UMediaSource* MediaSource = Cast<UMediaSource>(InObject))
	{
		return MakeShared<FMediaSourceImageViewer>(MediaSource, FMediaImageViewer::GetObjectDisplayName(MediaSource));
	}

	return nullptr;
}

TSharedPtr<FMediaViewerLibraryItem> FMediaSourceImageViewer::FFactory::CreateLibraryItem(TNotNull<UObject*> InObject) const
{
	if (UMediaSource* MediaSource = Cast<UMediaSource>(InObject))
	{
		return MakeShared<FMediaSourceImageViewer::FAssetItem>(
			FMediaImageViewer::GetObjectDisplayName(MediaSource),
			FText::Format(LOCTEXT("FactoryToolTipFormat", "{0} [Media Source]"), FText::FromString(MediaSource->GetPathName())),
			MediaSource->HasAnyFlags(EObjectFlags::RF_Transient) || MediaSource->IsIn(GetTransientPackage()),
			MediaSource
		);
	}

	return nullptr;	
}

bool FMediaSourceImageViewer::FFactory::SupportsItemType(FName InItemType) const
{
	return InItemType == FMediaSourceImageViewer::ItemTypeName_Asset;
}

TSharedPtr<FMediaViewerLibraryItem> FMediaSourceImageViewer::FFactory::CreateLibraryItem(const FMediaViewerLibraryItem& InSavedItem) const
{
	return MakeShared<FMediaSourceImageViewer::FAssetItem>(FPrivateToken(), InSavedItem);
}

FMediaSourceImageViewer::FItem::FItem(const FText& InName, const FText& InToolTip, bool bInTransient, const FString& InStringValue)
	: FMediaSourceImageViewer::FItem::FItem(FGuid::NewGuid(), InName, InToolTip, bInTransient, InStringValue)
{

}

FMediaSourceImageViewer::FItem::FItem(const FGuid& InId, const FText& InName, const FText& InToolTip, bool bInTransient, const FString& InStringValue)
	: FMediaViewerLibraryItem(InId, InName, InToolTip, bInTransient, InStringValue)
{

}

TSharedPtr<FSlateBrush> FMediaSourceImageViewer::FItem::CreateThumbnail()
{
	// Loading the media to create a thumbnail is not a good idea.
	TSharedRef<FSlateImageBrush> ThumbnailBrush = MakeShared<FSlateImageBrush>(
		GetMediaSourceThumbnailTexture(),
		FVector2D(64.0, 64.0)
	);

	return ThumbnailBrush;
}

FString FMediaSourceImageViewer::FItem::GetReferencerName() const
{
	static const FString ReferencerName = TEXT("FMediaSourceImageViewer::FItem");
	return ReferencerName;
}

void FMediaSourceImageViewer::FItem::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (Texture)
	{
		Collector.AddReferencedObject(Texture);
	}
}

FMediaSourceImageViewer::FAssetItem::FAssetItem(const FText& InName, const FText& InToolTip, bool bInTransient, TNotNull<UMediaSource*> InMediaSource)
	: FMediaSourceImageViewer::FAssetItem(FGuid::NewGuid(), InName, InToolTip, bInTransient, InMediaSource)
{
}

FMediaSourceImageViewer::FAssetItem::FAssetItem(const FGuid& InId, const FText& InName, const FText& InToolTip, bool bInTransient,
	TNotNull<UMediaSource*> InMediaSource)
	: FItem(InId, InName, InToolTip, bInTransient, InMediaSource->GetPathName())
{
}

FMediaSourceImageViewer::FAssetItem::FAssetItem(FPrivateToken&& InPrivateToken, const FMediaViewerLibraryItem& InItem)
	: FMediaSourceImageViewer::FItem(InItem.GetId(), InItem.Name, InItem.ToolTip, InItem.IsTransient(), InItem.GetStringValue())
{
}

FName FMediaSourceImageViewer::FAssetItem::GetItemType() const
{
	return FMediaSourceImageViewer::ItemTypeName_Asset;
}

FText FMediaSourceImageViewer::FAssetItem::GetItemTypeDisplayName() const
{
	return LOCTEXT("MediaSource", "Media Asset");
}

TSharedPtr<FMediaImageViewer> FMediaSourceImageViewer::FAssetItem::CreateImageViewer() const
{
	UMediaSource* MediaSource = LoadAssetFromString<UMediaSource>(StringValue);

	if (!MediaSource)
	{
		return nullptr;
	}

	const FText DisplayName = FMediaImageViewer::GetObjectDisplayName(MediaSource);

	if (Id.IsValid())
	{
		return MakeShared<FMediaSourceImageViewer>(Id, MediaSource, DisplayName);
	}

	return MakeShared<FMediaSourceImageViewer>(MediaSource, DisplayName);
}

TSharedPtr<FMediaViewerLibraryItem> FMediaSourceImageViewer::FAssetItem::Clone() const
{
	if (StringValue.IsEmpty())
	{
		return nullptr;
	}

	return MakeShared<FMediaSourceImageViewer::FAssetItem>(FPrivateToken(), *this);
}

FMediaSourceImageViewer::FExternalItem::FExternalItem(const FText& InName, const FText& InToolTip, const FString& InFilePath)
	: FMediaSourceImageViewer::FExternalItem(FGuid::NewGuid(), InName, InToolTip, InFilePath)
{
}

FMediaSourceImageViewer::FExternalItem::FExternalItem(const FGuid& InId, const FText& InName, const FText& InToolTip, const FString& InFilePath)
	: FMediaSourceImageViewer::FItem(InId, InName, InToolTip, /* Transient */ false, InFilePath)
{
}

FMediaSourceImageViewer::FExternalItem::FExternalItem(FPrivateToken&& InPrivateToken, const FMediaViewerLibraryItem& InItem)
	: FMediaSourceImageViewer::FItem(InItem.GetId(), InItem.Name, InItem.ToolTip, InItem.IsTransient(), InItem.GetStringValue())
{
}

FName FMediaSourceImageViewer::FExternalItem::GetItemType() const
{
	return FMediaSourceImageViewer::ItemTypeName_File;
}

FText FMediaSourceImageViewer::FExternalItem::GetItemTypeDisplayName() const
{
	return LOCTEXT("MediaFile", "Media File");
}

TSharedPtr<FMediaImageViewer> FMediaSourceImageViewer::FExternalItem::CreateImageViewer() const
{
	UFileMediaSource* FileMediaSource = NewObject<UFileMediaSource>(GetTransientPackage());
	FileMediaSource->SetFilePath(StringValue);

	const FText DisplayName = FText::FromString(StringValue);

	if (Id.IsValid())
	{
		return MakeShared<FMediaSourceImageViewer>(Id, FileMediaSource, DisplayName);
	}

	return MakeShared<FMediaSourceImageViewer>(FileMediaSource, DisplayName);
}

TSharedPtr<FMediaViewerLibraryItem> FMediaSourceImageViewer::FExternalItem::Clone() const
{
	if (StringValue.IsEmpty())
	{
		return nullptr;
	}

	return MakeShared<FMediaSourceImageViewer::FExternalItem>(FPrivateToken(), *this);
}

FMediaSourceImageViewer::FMediaSourceImageViewer(TNotNull<UMediaSource*> InMediaSource, const FText& InDisplayName)
	: FMediaSourceImageViewer(FGuid::NewGuid(), InMediaSource, InDisplayName)
{
}

FMediaSourceImageViewer::FMediaSourceImageViewer(const FGuid& InId, TNotNull<UMediaSource*> InMediaSource, const FText& InDisplayName)
	: FMediaImageViewer({
		InId,
		FIntPoint(1, 1),
		1,
		InDisplayName
	})
{
	MediaSourceSettings.MediaSource = InMediaSource;

	MediaStream = NewObject<UMediaStream>(GetTransientPackage());

	TSoftObjectPtr<UObject> SoftPtr(InMediaSource);
	FMediaStreamSource Source = UMediaStreamSourceBlueprintLibrary::MakeMediaSourceFromAsset(MediaStream, SoftPtr);

	if (UMediaStreamSourceBlueprintLibrary::IsValidMediaSource(Source))
	{
		MediaStream->SetSource(Source);

		if (TScriptInterface<IMediaStreamPlayer> Player = MediaStream->GetPlayer())
		{
			if (UMediaTexture* MediaTexture = Player->GetMediaTexture())
			{
				ImageInfo.Size.X = MediaTexture->GetSurfaceWidth();
				ImageInfo.Size.Y = MediaTexture->GetSurfaceHeight();

				FMediaStreamPlayerConfig Config = Player->GetPlayerConfig();
				Config.bPlayOnOpen = true;
				Config.bLooping = true;
				Player->SetPlayerConfig(Config);

				Brush = MakeShared<FSlateImageBrush>(MediaTexture, FVector2D(ImageInfo.Size.X, ImageInfo.Size.Y));

				// Make sure it doesn't display in the media texture list
				MediaTexture->AddAssetUserData(NewObject<UMediaViewerUserData>(MediaTexture));

				// We can't know the pixel format of the media texture at this point.
				SampleCache = MakeShared<FTextureSampleCache>(MediaTexture, PF_Unknown);
			}
		}
	}
}

FMediaSourceImageViewer::~FMediaSourceImageViewer()
{
	if (UObjectInitialized() && MediaStream)
	{
		MediaStream->Close();
	}
}

UMediaStream* FMediaSourceImageViewer::GetMediaStream() const
{
	return MediaStream;
}

TSharedPtr<FMediaViewerLibraryItem> FMediaSourceImageViewer::CreateLibraryItem() const
{
	UMediaSource* MediaSource = MediaSourceSettings.MediaSource;

	if (!MediaSource)
	{
		return nullptr;
	}

	const bool bIsTransient = MediaSource->HasAnyFlags(EObjectFlags::RF_Transient) || MediaSource->IsIn(GetTransientPackage());
	UFileMediaSource* FileMediaSource = Cast<UFileMediaSource>(MediaSource);

	if (!bIsTransient || !FileMediaSource)
	{
		return MakeShared<FAssetItem>(
			ImageInfo.Id,
			FMediaImageViewer::GetObjectDisplayName(MediaSource),
			FText::Format(LOCTEXT("LibraryToolTipFormat", "{0} [Media Texture - {1}x{2}]"), FText::FromString(MediaSource->GetPathName()), FText::AsNumber(ImageInfo.Size.X), FText::AsNumber(ImageInfo.Size.Y)),
			bIsTransient,
			MediaSource
		);
	}

	return MakeShared<FExternalItem>(
		ImageInfo.Id,
		FText::FromString(FileMediaSource->GetFilePath()),
		FText::Format(LOCTEXT("LibraryToolTipFormat", "{0} [Media Texture - {1}x{2}]"), FText::FromString(FileMediaSource->GetFilePath()), FText::AsNumber(ImageInfo.Size.X), FText::AsNumber(ImageInfo.Size.Y)),
		FileMediaSource->GetFilePath()
	);
}

TOptional<TVariant<FColor, FLinearColor>> FMediaSourceImageViewer::GetPixelColor(const FIntPoint& InPixelCoords, int32 InMipLevel) const
{
	if (!SampleCache.IsValid() || !SampleCache->IsValid())
	{
		return {};
	}
	
	if (InPixelCoords.X < 0 || InPixelCoords.Y < 0)
	{
		SampleCache->Invalidate();
		return {};
	}

	if (InPixelCoords.X >= ImageInfo.Size.X || InPixelCoords.Y >= ImageInfo.Size.Y)
	{
		SampleCache->Invalidate();
		return {};
	}

	if (!MediaStream)
	{
		SampleCache->Invalidate();
		return {};
	}

	IMediaStreamPlayer* MediaStreamPlayer = MediaStream->GetPlayer().GetInterface();

	if (!MediaStreamPlayer)
	{
		SampleCache->Invalidate();
		return {};
	}

	if (UMediaPlayer* Player = MediaStreamPlayer->GetPlayer())
	{
		const FTimespan PlayerTime = Player->GetTime();

		if (const FLinearColor* PixelColor = SampleCache->GetPixelColor(InPixelCoords, PlayerTime))
		{
			TVariant<FColor, FLinearColor> PixelColorVariant;
			PixelColorVariant.Set<FLinearColor>(*PixelColor);

			return PixelColorVariant;
		}
	}

	return {};
}

TSharedPtr<FStructOnScope> FMediaSourceImageViewer::GetCustomSettingsOnScope() const
{
	return MakeShared<FStructOnScope>(
		FMediaSourceImageViewerSettings::StaticStruct(),
		reinterpret_cast<uint8*>(&const_cast<FMediaSourceImageViewerSettings&>(MediaSourceSettings))
	);
}

void FMediaSourceImageViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddPropertyReferencesWithStructARO(
		FMediaSourceImageViewerSettings::StaticStruct(),
		&MediaSourceSettings
	);

	if (MediaStream)
	{
		Collector.AddReferencedObject(MediaStream);
	}
}

TSharedPtr<SWidget> FMediaSourceImageViewer::GetOverlayWidget(EMediaImageViewerPosition InPosition, const TSharedPtr<SMediaViewerTab>& InViewerTab)
{
	return SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(5.f)
		[
			SNew(SMediaSourceOverlay, SharedThis(this), InViewerTab)
		];
}

void FMediaSourceImageViewer::ExtendStatusBar(UE::MediaViewer::FMediaImageStatusBarExtender& InOutStatusBarExtender)
{
	InOutStatusBarExtender.AddExtension(
		UE::MediaViewer::StatusBarSections::StatusBarCenter,
		EExtensionHook::Before,
		nullptr,
		UE::MediaViewer::FMediaImageStatusBarExtension::FDelegate::CreateSP(this, &FMediaSourceImageViewer::AddPlayerName)
	);
}

void FMediaSourceImageViewer::AddPlayerName(const TSharedRef<SHorizontalBox>& InStatusBar)
{
	if (!MediaStream)
	{
		return;
	}

	IMediaStreamPlayer* MediaStreamPlayer = MediaStream->GetPlayer().GetInterface();

	if (!MediaStreamPlayer)
	{
		return;
	}

	UMediaPlayer* MediaPlayer = MediaStreamPlayer->GetPlayer();

	if (!MediaPlayer)
	{
		return;
	}

	const FName PlayerName = MediaPlayer->GetPlayerName();

	if (PlayerName.IsNone())
	{
		return;
	}

	InStatusBar->AddSlot()
		.AutoWidth()
		.Padding(2.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromName(PlayerName))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

FString FMediaSourceImageViewer::GetReferencerName() const
{
	static const FString ReferencerName = TEXT("FMediaSourceImageViewer");
	return ReferencerName;
}

void FMediaSourceImageViewer::PaintImage(FMediaImagePaintParams& InPaintParams, const FMediaImagePaintGeometry& InPaintGeometry)
{
	if (TScriptInterface<IMediaStreamPlayer> Player = MediaStream->GetPlayer())
	{
		if (UMediaTexture* MediaTexture = Player->GetMediaTexture())
		{
			ImageInfo.Size.X = MediaTexture->GetSurfaceWidth();
			ImageInfo.Size.Y = MediaTexture->GetSurfaceHeight();
		}
	}

	FMediaImageViewer::PaintImage(InPaintParams, InPaintGeometry);
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE

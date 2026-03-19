// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageViewers/LevelEditorViewportImageViewer.h"

#include "Engine/TextureRenderTarget2D.h"
#include "LevelEditor.h"
#include "MediaViewerUtils.h"
#include "Modules/ModuleManager.h"
#include "Slate/SceneViewport.h"
#include "SLevelViewport.h"

#define LOCTEXT_NAMESPACE "LevelEditorViewportImageViewer"

namespace UE::MediaViewer::Private
{

/** Can take either a string value (Viewport0..3) or a config key and return the string value (Viewport0..3) */
FString ConfigKeyToStringValue(const FString& InConfigKey)
{
	int32 DotIndex;
	InConfigKey.FindLastChar('.', DotIndex);

	if (DotIndex < 0 || DotIndex >= (InConfigKey.Len() - 1))
	{
		return InConfigKey;
	}

	return InConfigKey.Mid(DotIndex + 1);
}

/** Could be a config key or a direct value (Viewport0..3) */
FText GetViewportDisplayName(const FString& InViewportValue)
{
	return FText::FromString(ConfigKeyToStringValue(InViewportValue));
}

FText GetViewportTooltip(const FString& InViewportValue)
{
	return FText::GetEmpty();
}

const FLazyName FLevelEditorViewportImageViewer::ItemTypeName = TEXT("LevelEditorViewport");

bool FLevelEditorViewportImageViewer::FFactory::SupportsItemType(FName InItemType) const
{
	return InItemType == ItemTypeName;
}

TSharedPtr<FMediaViewerLibraryItem> FLevelEditorViewportImageViewer::FFactory::CreateLibraryItem(const FMediaViewerLibraryItem& InSavedItem) const
{
	return MakeShared<FLevelEditorViewportImageViewer::FItem>(FPrivateToken(), InSavedItem);
}

FGuid FLevelEditorViewportImageViewer::FItem::GetIdForViewport(const FString& InConfigKey, bool bCreateIfInvalid)
{
	if (!InConfigKey.IsEmpty())
	{
		const TCHAR LastChar = InConfigKey[InConfigKey.Len() - 1];

		switch (LastChar)
		{
			case '0':
				return FGuid(0xFDD736C4, 0xFD7A98CB, 0x7C28F833, 0xD00B4BB3);

			case '1':
				return FGuid(0xB7929682, 0x749CED27, 0x357D74D5, 0xC5E19053);

			case '2':
				return FGuid(0xD82E7C42, 0xB038A8A1, 0x9C608700, 0x9C3B589C);

			case '3':
				return FGuid(0xF0FE7988, 0xE5DE498B, 0x4AC6026B, 0x255B3BEC);
		}
	}

	if (bCreateIfInvalid)
	{
		return FGuid::NewGuid();
	}

	return FGuid();
}

TSharedPtr<FSceneViewport> FLevelEditorViewportImageViewer::FItem::GetViewportFromConfigKey(const FString& InConfigKey)
{
	return GetViewportFromStringValue(ConfigKeyToStringValue(InConfigKey));
}

TSharedPtr<FSceneViewport> FLevelEditorViewportImageViewer::FItem::GetViewportFromStringValue(const FString& InStringValue)
{
	if (InStringValue.IsEmpty())
	{
		return nullptr;
	}

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");

	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (!LevelEditor.IsValid())
	{
		return nullptr;
	}

	TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();

	for (const TSharedPtr<SLevelViewport>& Viewport : Viewports)
	{
		const FString ViewportValue = ConfigKeyToStringValue(Viewport->GetConfigKey().ToString());

		if (ViewportValue == InStringValue)
		{
			return Viewport->GetSharedActiveViewport();
		}
	}

	return nullptr;
}

FLevelEditorViewportImageViewer::FItem::FItem(const FString& InConfigKey)
	: FItem(GetIdForViewport(InConfigKey, /* Create id if invalid */ true), InConfigKey)
{
}

FLevelEditorViewportImageViewer::FItem::FItem(const FGuid& InId, const FString& InConfigKey)
	: FMediaViewerLibraryItem(InId, GetViewportDisplayName(InConfigKey), GetViewportTooltip(InConfigKey), /* Transient */ false, ConfigKeyToStringValue(InConfigKey))
{
}

FLevelEditorViewportImageViewer::FItem::FItem(FPrivateToken&& InPrivateToken, const FMediaViewerLibraryItem& InItem)
	: FMediaViewerLibraryItem(InItem.GetId(), InItem.Name, InItem.ToolTip, /* Transient */ false, InItem.GetStringValue())
{
}

FName FLevelEditorViewportImageViewer::FItem::GetItemType() const
{
	return FLevelEditorViewportImageViewer::ItemTypeName;
}

FText FLevelEditorViewportImageViewer::FItem::GetItemTypeDisplayName() const
{
	return LOCTEXT("LevelEditorViewport", "Level Editor Viewport");
}

TSharedPtr<FSlateBrush> FLevelEditorViewportImageViewer::FItem::CreateThumbnail()
{
	if (StringValue.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<FSceneViewport> Viewport = GetViewportFromStringValue(StringValue);

	if (!Viewport.IsValid())
	{
		return nullptr;
	}

	const FIntPoint RenderTargetSize = {32, 32};
	RenderTarget = FMediaViewerUtils::CreateRenderTarget(RenderTargetSize, /* Transparent */ false);

	RenderViewport(
		TNotNull<FViewport*>(Viewport.Get()), 
		TNotNull<UTextureRenderTarget2D*>(RenderTarget), 
		FRenderComplete(),
		/* Resize render target */ false
	);

	TSharedRef<FSlateImageBrush> ThumbnailBrush = MakeShared<FSlateImageBrush>(RenderTarget, FVector2D(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceDepth()));

	return ThumbnailBrush;
}

TSharedPtr<FMediaImageViewer> FLevelEditorViewportImageViewer::FItem::CreateImageViewer() const
{
	if (StringValue.IsEmpty())
	{
		return nullptr;
	}

	if (Id.IsValid())
	{
		return MakeShared<FLevelEditorViewportImageViewer>(Id, StringValue);
	}

	return MakeShared<FLevelEditorViewportImageViewer>(StringValue);
}

TSharedPtr<FMediaViewerLibraryItem> FLevelEditorViewportImageViewer::FItem::Clone() const
{
	if (StringValue.IsEmpty())
	{
		return nullptr;
	}

	return MakeShared<FLevelEditorViewportImageViewer::FItem>(FPrivateToken(), *this);
}

FString FLevelEditorViewportImageViewer::FItem::GetReferencerName() const
{
	static const FString ReferencerName = TEXT("FLevelEditorViewportImageViewer");
	return ReferencerName;
}

void FLevelEditorViewportImageViewer::FItem::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (RenderTarget)
	{
		Collector.AddReferencedObject(RenderTarget);
	}
}

FLevelEditorViewportImageViewer::FLevelEditorViewportImageViewer(const FString& InViewportValue)
	: FLevelEditorViewportImageViewer(FItem::GetIdForViewport(InViewportValue, /* Create id if invalid */ true), InViewportValue)
{
}

FLevelEditorViewportImageViewer::FLevelEditorViewportImageViewer(const FGuid& InId, const FString& InViewportValue)
	: FSceneViewportImageViewer(InId, FItem::GetViewportFromConfigKey(InViewportValue), GetViewportDisplayName(InViewportValue))
	, StringValue(ConfigKeyToStringValue(InViewportValue))
{
}

TSharedPtr<FMediaViewerLibraryItem> FLevelEditorViewportImageViewer::CreateLibraryItem() const
{
	return MakeShared<FItem>(ImageInfo.Id, StringValue);
}

void FLevelEditorViewportImageViewer::PaintImage(FMediaImagePaintParams& InPaintParams, const FMediaImagePaintGeometry& InPaintGeometry)
{
	if (ViewportSettings.bRealTime && !ViewportWeak.IsValid())
	{
		if (StringValue.IsEmpty())
		{
			return;
		}

		TSharedPtr<FSceneViewport> NewViewport = FItem::GetViewportFromStringValue(StringValue);

		if (!NewViewport.IsValid())
		{
			return;
		}

		ViewportWeak = NewViewport;
	}

	FSceneViewportImageViewer::PaintImage(InPaintParams, InPaintGeometry);
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE

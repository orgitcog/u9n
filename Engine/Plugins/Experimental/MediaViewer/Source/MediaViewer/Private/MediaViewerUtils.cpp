// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaViewerUtils.h"

#include "CanvasTypes.h"
#include "DetailsViewArgs.h"
#include "EditorClassUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IStructureDetailsView.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Math/IntPoint.h"
#include "Misc/TVariant.h"
#include "Modules/ModuleManager.h"
#include "PixelFormat.h"
#include "PropertyEditorModule.h"
#include "Slate/WidgetRenderer.h"
#include "SlateMaterialBrush.h"
#include "TextureResource.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"

namespace UE::MediaViewer::Private
{

UMaterialInterface* FMediaViewerUtils::GetTextureRenderMaterial()
{
	constexpr const TCHAR* MaterialPath = TEXT("/Script/Engine.Material'/MediaViewer/TextureRenderer.TextureRenderer'");

	return LoadObject<UMaterial>(GetTransientPackage(), MaterialPath);
}

TOptional<TVariant<FColor, FLinearColor>> FMediaViewerUtils::GetPixelColor(TArrayView64<uint8> InPixelData,
	EPixelFormat InPixelFormat, const FIntPoint& InTextureSize, const FIntPoint& InPixelCoords, int32 InMipLevel)
{
	// Ensure at least 1 byte per pixel...
	if (InPixelData.Num() < (InTextureSize.X * InTextureSize.Y))
	{
		return {};
	}

	TVariant<FColor, FLinearColor> ColorVariant;

	const int32 MipSize = InPixelData.Num();
	const int32 StripeSize = MipSize / InTextureSize.Y;
	const int32 StripeOffset = StripeSize * InPixelCoords.Y;

	switch (InPixelFormat)
	{
		case EPixelFormat::PF_R8:
		{
			const int32 PixelOffset = StripeOffset + InPixelCoords.X;
			FColor Color;
			Color.R = InPixelData[PixelOffset];
			Color.G = 0;
			Color.B = 0;
			Color.A = 255;
			ColorVariant.Set<FColor>(Color);
			break;
		}

		case EPixelFormat::PF_G8:
		{
			const int32 PixelOffset = StripeOffset + InPixelCoords.X;
			FColor Color;
			Color.R = 0;
			Color.G = InPixelData[PixelOffset];
			Color.B = 0;
			Color.A = 255;
			ColorVariant.Set<FColor>(Color);
			break;
		}

		case EPixelFormat::PF_A8:
		{
			const int32 PixelOffset = StripeOffset + InPixelCoords.X;
			FColor Color;
			Color.R = 0;
			Color.G = 0;
			Color.B = 0;
			Color.A = InPixelData[PixelOffset];
			ColorVariant.Set<FColor>(Color);
			break;
		}

		case EPixelFormat::PF_R8G8:
		{
			const int32 PixelOffset = StripeOffset + (InPixelCoords.X * 2);
			FColor Color;
			Color.R = InPixelData[PixelOffset];
			Color.G = InPixelData[PixelOffset + 1];
			Color.B = 0;
			Color.A = 255;
			ColorVariant.Set<FColor>(Color);
			break;
		}

		case EPixelFormat::PF_R8G8B8:
		{
			const int32 PixelOffset = StripeOffset + (InPixelCoords.X * 3);
			FColor Color;
			Color.R = InPixelData[PixelOffset];
			Color.G = InPixelData[PixelOffset + 1];
			Color.B = InPixelData[PixelOffset + 2];
			Color.A = 255;
			ColorVariant.Set<FColor>(Color);
			break;
		}

		case EPixelFormat::PF_R8G8B8A8:
		{
			const int32 PixelOffset = StripeOffset + (InPixelCoords.X * 4);
			FColor Color;
			Color.R = InPixelData[PixelOffset];
			Color.G = InPixelData[PixelOffset + 1];
			Color.B = InPixelData[PixelOffset + 2];
			Color.A = InPixelData[PixelOffset + 3];
			ColorVariant.Set<FColor>(Color);
			break;
		}

		case EPixelFormat::PF_B8G8R8A8:
		{
			const int32 PixelOffset = StripeOffset + (InPixelCoords.X * 4);
			FColor Color;
			Color.R = InPixelData[PixelOffset + 2];
			Color.G = InPixelData[PixelOffset + 1];
			Color.B = InPixelData[PixelOffset];
			Color.A = InPixelData[PixelOffset + 3];
			ColorVariant.Set<FColor>(Color);
			break;
		}

		case EPixelFormat::PF_A8R8G8B8:
		{
			const int32 PixelOffset = StripeOffset + (InPixelCoords.X * 4);
			FColor Color;
			Color.R = InPixelData[PixelOffset + 1];
			Color.G = InPixelData[PixelOffset + 2];
			Color.B = InPixelData[PixelOffset + 3];
			Color.A = InPixelData[PixelOffset];
			ColorVariant.Set<FColor>(Color);
			break;
		}

		default:
			return {};
	}

	return ColorVariant;
}

UTextureRenderTarget2D* FMediaViewerUtils::CreateRenderTarget(const FIntPoint& InSize, bool bInTransparent)
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	check(RenderTarget);
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->bCanCreateUAV = false;
	RenderTarget->ClearColor = bInTransparent ? FLinearColor::Transparent : FLinearColor::Black;
	RenderTarget->InitAutoFormat(InSize.X, InSize.Y);
	RenderTarget->UpdateResourceImmediate(true);
	RenderTarget->AddAssetUserData(NewObject<UMediaViewerUserData>(RenderTarget));

	return RenderTarget;
}

UTextureRenderTarget2D* FMediaViewerUtils::RenderMaterial(TNotNull<UMaterialInterface*> InMaterial)
{
	UTextureRenderTarget2D* RenderTarget = CreateRenderTarget(FIntPoint(256, 256), InMaterial->IsUIMaterial());

	RenderMaterial(InMaterial, TNotNull<UTextureRenderTarget2D*>(RenderTarget));

	return RenderTarget;
}

void FMediaViewerUtils::RenderMaterial(TNotNull<UMaterialInterface*> InMaterial, TNotNull<UTextureRenderTarget2D*> InRenderTarget)
{
	if (!InMaterial->IsUIMaterial())
	{
		return;
	}

	// Taken from UMaterialInstanceThumbnailRenderer - but with the background checkerboard texture removed.
	const bool bUseGammaCorrection = true;
	FWidgetRenderer WidgetRenderer(bUseGammaCorrection);

	const FVector2D DrawSize = FVector2D(InRenderTarget->GetSurfaceWidth(), InRenderTarget->GetSurfaceHeight());

	FSlateMaterialBrush UIMaterialBrush(DrawSize);
	UIMaterialBrush.SetMaterial(InMaterial);

	TSharedRef<SImage> Image = SNew(SImage)
		.Image(&UIMaterialBrush);

	constexpr float DeltaTime = 0.f;
	WidgetRenderer.DrawWidget(InRenderTarget, Image, DrawSize, DeltaTime);
}

TSharedRef<IStructureDetailsView> FMediaViewerUtils::CreateStructDetailsView(TSharedRef<FStructOnScope> InStructOnScope,
	const FText& InCustomName, FNotifyHook* InNotifyHook)
{
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bAllowFavoriteSystem = false;
	DetailsArgs.bAllowMultipleTopLevelObjects = false;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bCustomFilterAreaLocation = false;
	DetailsArgs.bCustomNameAreaLocation = false;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowObjectLabel = false;
	DetailsArgs.bShowPropertyMatrixButton = false;
	DetailsArgs.bShowScrollBar = false;
	DetailsArgs.bShowSectionSelector = false;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.NotifyHook = InNotifyHook;

	FStructureDetailsViewArgs StructArgs;
	StructArgs.bShowAssets = true;
	StructArgs.bShowClasses = true;
	StructArgs.bShowInterfaces = true;
	StructArgs.bShowObjects = true;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	return PropertyEditorModule.CreateStructureDetailView(DetailsArgs, StructArgs, InStructOnScope, InCustomName);
}

} // UE::MediaViewer::Private

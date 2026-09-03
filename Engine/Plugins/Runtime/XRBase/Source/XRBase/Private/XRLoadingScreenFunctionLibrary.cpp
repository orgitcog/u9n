// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRLoadingScreenFunctionLibrary.h"
#include "EngineGlobals.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(XRLoadingScreenFunctionLibrary)

static IXRLoadingScreen* GetLoadingScreen()
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		return GEngine->XRSystem->GetLoadingScreen();
	}
	
	return nullptr;
}

UXRLoadingScreenFunctionLibrary::UXRLoadingScreenFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UXRLoadingScreenFunctionLibrary::SetLoadingScreen(class UTexture* Texture, FVector2D Scale, FVector Offset, bool bShowLoadingMovie, bool bShowOnSet)
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen && Texture)
	{
		LoadingScreen->ClearSplashes();
		const bool bIsExternal = Texture->GetMaterialType() == MCT_TextureExternal;
		IXRLoadingScreen::FSplashDesc Splash;
		Splash.Transform = FTransform(Offset);
		Splash.QuadSize = Scale;
		Splash.bIsDynamic = bShowLoadingMovie || bIsExternal;
		Splash.bIsExternal = bIsExternal;
		Splash.TextureObj = Texture;
		LoadingScreen->AddSplash(Splash);

		if (bShowOnSet)
		{
			LoadingScreen->ShowLoadingScreen();
		}
	}
}

void UXRLoadingScreenFunctionLibrary::ClearLoadingScreenSplashes()
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen)
	{
		LoadingScreen->ClearSplashes();
	}

}

void UXRLoadingScreenFunctionLibrary::AddLoadingScreenSplash(class UTexture* Texture, FVector Translation, FRotator Rotation, FVector2D Size, FRotator DeltaRotation, bool bClearBeforeAdd)
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen && Texture)
	{
		if (bClearBeforeAdd)
		{
			LoadingScreen->ClearSplashes();
		}

		IXRLoadingScreen::FSplashDesc Splash;
		Splash.TextureObj = Texture;
		Splash.QuadSize = Size;
		Splash.Transform = FTransform(Rotation, Translation);
		Splash.DeltaRotation = FQuat(DeltaRotation);
		LoadingScreen->AddSplash(Splash);
	}
}

void UXRLoadingScreenFunctionLibrary::ShowLoadingScreen()
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen)
	{
		LoadingScreen->ShowLoadingScreen();
	}
}

void UXRLoadingScreenFunctionLibrary::HideLoadingScreen()
{
	IXRLoadingScreen* LoadingScreen = GetLoadingScreen();
	if (LoadingScreen)
	{
		LoadingScreen->HideLoadingScreen();
	}
}


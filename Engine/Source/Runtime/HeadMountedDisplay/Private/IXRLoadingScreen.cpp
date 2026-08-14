// Copyright Epic Games, Inc. All Rights Reserved.

#include "IXRLoadingScreen.h"
#include "IXRTrackingSystem.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"


void IXRLoadingScreen::ShowLoadingScreen_Compat(bool bShow, FTextureRHIRef Texture, const FVector& Offset, const FVector2D& Scale)
{
	// Backwards compatibility with IStereoLayers::ShowSplashScreen
	IXRLoadingScreen* LoadingScreen = GEngine && GEngine->XRSystem.IsValid()? GEngine->XRSystem->GetLoadingScreen() : nullptr;
	if (LoadingScreen)
	{
		if (bShow)
		{
			LoadingScreen->ShowLoadingScreen();
		}
		else
		{
			LoadingScreen->HideLoadingScreen();
		}
	}

}

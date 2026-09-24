// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkDeviceStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"


TSharedPtr<FSlateStyleSet> FLiveLinkDeviceStyle::StyleSet;


void FLiveLinkDeviceStyle::Initialize()
{
	if (!ensureMsgf(!StyleSet, TEXT("FLiveLinkDeviceStyle already initialized")))
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());

	const FString PluginContentRoot =
		IPluginManager::Get().FindPlugin("LiveLinkDevice")->GetBaseDir() / TEXT("Resources");
	StyleSet->SetContentRoot(PluginContentRoot);

	static const FVector2D Icon16x16(16.0f, 16.0f);
	static const FVector2D Icon20x20(20.0f, 20.0f);
	StyleSet->Set("Record", new FSlateVectorImageBrush(PluginContentRoot / "PlayControlsRecord.svg", Icon16x16));
	StyleSet->Set("Record.Monochrome", new FSlateVectorImageBrush(PluginContentRoot / "PlayControlsRecord_Monochrome.svg", Icon16x16));

	StyleSet->Set("LiveLinkHub.Devices.Icon", new FSlateVectorImageBrush(PluginContentRoot / "Devices.svg", Icon16x16));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}


void FLiveLinkDeviceStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}


TSharedPtr<ISlateStyle> FLiveLinkDeviceStyle::Get()
{
	return StyleSet;
}

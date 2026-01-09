// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "ILauncherTask.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"

namespace PlatformInfo
{
	struct FTargetPlatformInfo;
};

class FProjectLauncherStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static const FName& GetStyleSetName();
	static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return StyleSet->GetBrush(PropertyName, Specifier);
	}

	static const FSlateBrush* GetBrushForTask(ILauncherTaskPtr Task);
	static const FSlateBrush* GetProfileBrushForPlatform(const PlatformInfo::FTargetPlatformInfo* PlatformInfo, EPlatformIconSize IconSize);

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;	
};

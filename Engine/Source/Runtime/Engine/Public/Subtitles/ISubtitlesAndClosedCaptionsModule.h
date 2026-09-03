// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Features/IModularFeature.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SubclassOf.h"

class UAssetUserData;

class ISubtitlesAndClosedCaptionsModule : public IModuleInterface, public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static const FName FeatureName = FName(TEXT("SubtitlesAndClosedCaptions"));
		return FeatureName;
	}

	virtual TSubclassOf<UAssetUserData> GetAssetUserDataClass() const = 0;
};

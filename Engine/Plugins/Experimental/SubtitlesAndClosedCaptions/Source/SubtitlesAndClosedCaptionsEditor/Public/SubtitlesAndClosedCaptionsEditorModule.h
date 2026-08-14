// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "Modules/ModuleInterface.h"

class ISubtitlesAndClosedCaptionsEditorModule : public IModuleInterface
{
public:
	static FText GetAssetTypeCategory()
	{
		return AssetTypeCategory;
	}
protected:
	static FText AssetTypeCategory;
};

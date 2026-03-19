// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"

class MEGASCANSPLUGIN_API FAssetsImportController
{
private:
	FAssetsImportController() = default;
	static TSharedPtr<FAssetsImportController> AssetsImportController;
	TArray<FString> SupportedAssetTypes = {
		TEXT("3d"),
		TEXT("3dplant"),
		TEXT("atlas"),
		TEXT("surface")
	};

public:	
	static TSharedPtr<FAssetsImportController> Get();
	void DataReceived(const FString DataFromBridge);
};

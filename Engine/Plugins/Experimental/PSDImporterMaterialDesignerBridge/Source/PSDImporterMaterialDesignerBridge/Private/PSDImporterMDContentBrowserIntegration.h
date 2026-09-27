// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"

class FExtender;
class FMenuBuilder;
enum class EPSDImporterMaterialDesignerType : uint8;

class FPSDImporterMaterialDesignerContentBrowserIntegration
{
public:
	static FPSDImporterMaterialDesignerContentBrowserIntegration& Get();

	void Integrate();

	void Disintegrate();

protected:
	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& InSelectedAssets);

	void CreateMenuEntries(FMenuBuilder& InMenuBuilder, TArray<FAssetData> InSelectedAssets);

	void CreatePSDMaterialMaterialDesigner(TArray<FAssetData> InSelectedAssets);

	void CreatePSDQuadsMaterialDesigner(TArray<FAssetData> InSelectedAssets, EPSDImporterMaterialDesignerType InType);

	FDelegateHandle ContentBrowserHandle;
};

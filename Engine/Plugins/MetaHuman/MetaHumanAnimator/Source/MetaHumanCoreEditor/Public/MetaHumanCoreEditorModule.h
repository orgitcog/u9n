// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Misc/AssetCategoryPath.h"

class METAHUMANCOREEDITOR_API IMetaHumanCoreEditorModule
	: public IModuleInterface
{
public:
	virtual TConstArrayView<FAssetCategoryPath> GetMetaHumanAssetCategoryPath() const = 0;
	virtual TConstArrayView<FAssetCategoryPath> GetMetaHumanAdvancedAssetCategoryPath() const = 0;
};
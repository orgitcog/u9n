// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EditorFramework/AssetImportData.h"
#include "GroomAssetImportData.generated.h"

UCLASS(MinimalAPI)
class UGroomAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	TObjectPtr<class UGroomImportOptions> ImportOptions;

	UPROPERTY()
	TObjectPtr<class UGroomCreateStrandsTexturesOptions> HairStrandsTexturesOptions;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

/**
 * The asset actions for the URigMapperLinkedDefinitions data asset class and link to its asset editor toolkit
 */
class RIGMAPPEREDITOR_API FRigMapperLinkedDefinitionsAssetTypeActions : public FAssetTypeActions_Base
{
public:
	virtual UClass* GetSupportedClass() const override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "DataLinkGraphFactory.generated.h"

UCLASS()
class UDataLinkGraphFactory : public UFactory
{
	GENERATED_BODY()

public:
	UDataLinkGraphFactory();

	//~ Begin UFactory
	virtual FText GetDisplayName() const override;
	virtual FString GetDefaultNewAssetName() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
	//~ End UFactory
};

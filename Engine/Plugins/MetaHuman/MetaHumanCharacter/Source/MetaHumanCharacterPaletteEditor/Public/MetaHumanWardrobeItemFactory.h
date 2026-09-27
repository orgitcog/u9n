// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "MetaHumanWardrobeItemFactory.generated.h"

UCLASS()
class METAHUMANCHARACTERPALETTEEDITOR_API UMetaHumanWardrobeItemFactory : public UFactory
{
	GENERATED_BODY()

public:

	UMetaHumanWardrobeItemFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* Context, FFeedbackContext* Warn) override;
};

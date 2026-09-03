// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "MetaHumanCharacterFactoryNew.generated.h"

UCLASS()
class METAHUMANCHARACTEREDITOR_API UMetaHumanCharacterFactoryNew : public UFactory
{
	GENERATED_BODY()

public:

	UMetaHumanCharacterFactoryNew();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* Context, FFeedbackContext* Warn) override;
};
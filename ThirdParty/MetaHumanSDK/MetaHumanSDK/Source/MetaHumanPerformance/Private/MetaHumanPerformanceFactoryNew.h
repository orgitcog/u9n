// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "MetaHumanPerformanceFactoryNew.generated.h"

//////////////////////////////////////////////////////////////////////////
// UMetaHumanPerformanceFactoryNew

UCLASS(hideCategories = Object)
class UMetaHumanPerformanceFactoryNew
	: public UFactory
{
	GENERATED_BODY()

public:
	//~ UFactory Interface
	UMetaHumanPerformanceFactoryNew();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "UObject/ObjectMacros.h"

#include "MetaHumanCaptureSourceFactoryNew.generated.h"

UCLASS(HideCategories=Object)
class UMetaHumanCaptureSourceFactoryNew
	: public UFactory
{
	GENERATED_BODY()

public:
	UMetaHumanCaptureSourceFactoryNew();

	//~ UFactory interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
};

UCLASS(HideCategories = Object)
class UMetaHumanCaptureSourceSyncFactoryNew
	: public UFactory
{
	GENERATED_BODY()

public:
	UMetaHumanCaptureSourceSyncFactoryNew();

	//~ UFactory interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
};
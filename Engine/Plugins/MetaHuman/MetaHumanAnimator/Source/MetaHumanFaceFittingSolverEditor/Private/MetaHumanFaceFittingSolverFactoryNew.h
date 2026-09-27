// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "MetaHumanFaceFittingSolverFactoryNew.generated.h"


//////////////////////////////////////////////////////////////////////////
// UMetaHumanFaceFittingSolverFactoryNew

UCLASS(hidecategories=Object)
class UMetaHumanFaceFittingSolverFactoryNew
	: public UFactory
{
	GENERATED_BODY()

public:

	UMetaHumanFaceFittingSolverFactoryNew();

	//~Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
	//~End UFactory Interface
};

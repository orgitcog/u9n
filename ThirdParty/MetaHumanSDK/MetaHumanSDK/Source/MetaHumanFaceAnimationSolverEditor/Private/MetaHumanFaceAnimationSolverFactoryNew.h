// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "MetaHumanFaceAnimationSolverFactoryNew.generated.h"


//////////////////////////////////////////////////////////////////////////
// UMetaHumanFaceAnimationSolverFactoryNew

UCLASS(hidecategories=Object)
class UMetaHumanFaceAnimationSolverFactoryNew
	: public UFactory
{
	GENERATED_BODY()

public:

	UMetaHumanFaceAnimationSolverFactoryNew();

	//~Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
	//~End UFactory Interface
};

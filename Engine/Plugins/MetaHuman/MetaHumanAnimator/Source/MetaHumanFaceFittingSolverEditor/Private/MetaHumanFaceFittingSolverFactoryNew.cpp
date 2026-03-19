// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFaceFittingSolverFactoryNew.h"
#include "MetaHumanFaceFittingSolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanFaceFittingSolverFactoryNew)

//////////////////////////////////////////////////////////////////////////
// UMetaHumanFaceFittingSolverFactoryNew

UMetaHumanFaceFittingSolverFactoryNew::UMetaHumanFaceFittingSolverFactoryNew()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanFaceFittingSolver::StaticClass();
}

UObject* UMetaHumanFaceFittingSolverFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	return NewObject<UMetaHumanFaceFittingSolver>(InParent, InClass, InName, InFlags);
}

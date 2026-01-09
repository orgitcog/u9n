// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFaceAnimationSolverFactoryNew.h"
#include "MetaHumanFaceAnimationSolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanFaceAnimationSolverFactoryNew)

//////////////////////////////////////////////////////////////////////////
// UMetaHumanFaceAnimationSolverFactoryNew

UMetaHumanFaceAnimationSolverFactoryNew::UMetaHumanFaceAnimationSolverFactoryNew()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanFaceAnimationSolver::StaticClass();
}

UObject* UMetaHumanFaceAnimationSolverFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	return NewObject<UMetaHumanFaceAnimationSolver>(InParent, InClass, InName, InFlags);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceFactoryNew.h"
#include "MetaHumanPerformance.h"

//////////////////////////////////////////////////////////////////////////
// UMetaHumanPerformanceFactoryNew

UMetaHumanPerformanceFactoryNew::UMetaHumanPerformanceFactoryNew()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanPerformance::StaticClass();
}

UObject* UMetaHumanPerformanceFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	return NewObject<UMetaHumanPerformance>(InParent, InClass, InName, InFlags);
}
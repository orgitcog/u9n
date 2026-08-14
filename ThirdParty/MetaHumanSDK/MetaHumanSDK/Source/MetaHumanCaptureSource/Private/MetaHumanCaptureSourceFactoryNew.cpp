// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCaptureSourceFactoryNew.h"
#include "MetaHumanCaptureSource.h"
#include "MetaHumanCaptureSourceSync.h"

UMetaHumanCaptureSourceFactoryNew::UMetaHumanCaptureSourceFactoryNew()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanCaptureSource::StaticClass();
}

UObject* UMetaHumanCaptureSourceFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	return NewObject<UMetaHumanCaptureSource>(InParent, InClass, InName, InFlags);
}

UMetaHumanCaptureSourceSyncFactoryNew::UMetaHumanCaptureSourceSyncFactoryNew()
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanCaptureSourceSync::StaticClass();
}

UObject* UMetaHumanCaptureSourceSyncFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	return NewObject<UMetaHumanCaptureSourceSync>(InParent, InClass, InName, InFlags);
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureDataEditorModule.h"

#include "DeferredObjectDirtier.h"

#include "Modules/ModuleManager.h"

void FCaptureDataEditorModule::StartupModule()
{

}

void FCaptureDataEditorModule::ShutdownModule()
{

}

void FCaptureDataEditorModule::DeferMarkDirty(TWeakObjectPtr<UObject> InObject)
{
	using namespace UE::CaptureManager;
	
	FDeferredObjectDirtier& ObjectDirtier = FDeferredObjectDirtier::Get();
	ObjectDirtier.Enqueue(MoveTemp(InObject));
}

IMPLEMENT_MODULE(FCaptureDataEditorModule, CaptureDataEditor)
// Copyright Epic Games, Inc. All Rights Reserved.

#include "CogEngineEditor.h"

#define LOCTEXT_NAMESPACE "FCogEngineEditorModule"

void FCogEngineEditorModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngineEditor: Starting Editor Integration"));
	RegisterMenuExtensions();
}

void FCogEngineEditorModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngineEditor: Shutting down Editor Integration"));
	UnregisterMenuExtensions();
}

void FCogEngineEditorModule::RegisterMenuExtensions()
{
	// Register editor menu extensions for CogEngine
	UE_LOG(LogTemp, Log, TEXT("CogEngineEditor: Registered menu extensions"));
}

void FCogEngineEditorModule::UnregisterMenuExtensions()
{
	// Unregister editor menu extensions
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCogEngineEditorModule, CogEngineEditor)

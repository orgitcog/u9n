// Copyright Epic Games, Inc. All Rights Reserved.

#include "AphroditeEngine.h"

#define LOCTEXT_NAMESPACE "FAphroditeEngineModule"

void FAphroditeEngineModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AphroditeEngine: Starting LLM Inference Over Network System"));
}

void FAphroditeEngineModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("AphroditeEngine: Shutting down LLM Inference Engine"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAphroditeEngineModule, AphroditeEngine)

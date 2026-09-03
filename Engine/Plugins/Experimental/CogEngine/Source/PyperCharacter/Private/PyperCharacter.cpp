// Copyright Epic Games, Inc. All Rights Reserved.

#include "PyperCharacter.h"

#define LOCTEXT_NAMESPACE "FPyperCharacterModule"

void FPyperCharacterModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("PyperCharacter: Module started - Echo State Pyper initialized"));
}

void FPyperCharacterModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("PyperCharacter: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPyperCharacterModule, PyperCharacter)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "KoboldAIStoryweaver.h"

#define LOCTEXT_NAMESPACE "FKoboldAIStoryweaverModule"

void FKoboldAIStoryweaverModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("KoboldAIStoryweaver: Starting Autognostic World-Building System"));
}

void FKoboldAIStoryweaverModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("KoboldAIStoryweaver: Shutting down Storyweaver"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FKoboldAIStoryweaverModule, KoboldAIStoryweaver)

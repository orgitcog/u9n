// Copyright Epic Games, Inc. All Rights Reserved.

#include "CogEngine.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FCogEngineModule"

void FCogEngineModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngine: Starting Cognitive Multi-Agent Orchestration System"));
	InitializeCognitiveSubsystems();
}

void FCogEngineModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngine: Shutting down Cognitive Multi-Agent Orchestration System"));
	ShutdownCognitiveSubsystems();
}

void FCogEngineModule::InitializeCognitiveSubsystems()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngine: Initializing cognitive subsystems"));
	
	// Load dependent modules
	FModuleManager::Get().LoadModule(TEXT("OpenCogCore"));
	FModuleManager::Get().LoadModule(TEXT("AgentZeroKernel"));
	FModuleManager::Get().LoadModule(TEXT("KoboldAIStoryweaver"));
	FModuleManager::Get().LoadModule(TEXT("DeepTreeEcho"));
	FModuleManager::Get().LoadModule(TEXT("AphroditeEngine"));
	FModuleManager::Get().LoadModule(TEXT("MardukLab"));
	
	UE_LOG(LogTemp, Log, TEXT("CogEngine: All cognitive subsystems initialized"));
}

void FCogEngineModule::ShutdownCognitiveSubsystems()
{
	UE_LOG(LogTemp, Log, TEXT("CogEngine: Shutting down cognitive subsystems"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCogEngineModule, CogEngine)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuroFusion.h"

#define LOCTEXT_NAMESPACE "FNeuroFusionModule"

void FNeuroFusionModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("NeuroFusion: Module started - Superhot Pyper vs Neuro system initialized"));
}

void FNeuroFusionModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("NeuroFusion: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNeuroFusionModule, NeuroFusion)

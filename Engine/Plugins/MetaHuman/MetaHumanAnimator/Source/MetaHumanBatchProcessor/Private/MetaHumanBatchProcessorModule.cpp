// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanBatchProcessorModule.h"
#include "MetaHumanBatchMenuExtensions.h"

void FMetaHumanBatchProcessorModule::StartupModule()
{
	// Register our menu extensions
	MetaHumanBatchMenuExtensions = MakeUnique<FMetaHumanBatchMenuExtensions>();
	MetaHumanBatchMenuExtensions->RegisterMenuExtensions();
}

void FMetaHumanBatchProcessorModule::ShutdownModule()
{
	MetaHumanBatchMenuExtensions->UnregisterMenuExtensions();
}

IMPLEMENT_MODULE(FMetaHumanBatchProcessorModule, MetaHumanBatchProcessor)

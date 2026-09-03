// Copyright Epic Games, Inc. All Rights Reserved.

#include "MardukLab.h"

#define LOCTEXT_NAMESPACE "FMardukLabModule"

void FMardukLabModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("MardukLab: Starting ROS and Home-Assistant IoT Framework for Robotics"));
}

void FMardukLabModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("MardukLab: Shutting down ROS IoT Framework"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMardukLabModule, MardukLab)

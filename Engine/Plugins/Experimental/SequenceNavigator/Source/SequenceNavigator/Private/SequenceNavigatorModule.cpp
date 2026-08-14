// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequenceNavigatorModule.h"
#include "NavigationToolCommands.h"
#include "SequenceNavigatorLog.h"

DEFINE_LOG_CATEGORY(LogSequenceNavigator);

void FSequenceNavigatorModule::StartupModule()
{
	using namespace UE::SequenceNavigator;

	FNavigationToolCommands::Register();
}

void FSequenceNavigatorModule::ShutdownModule()
{
	using namespace UE::SequenceNavigator;

	FNavigationToolCommands::Unregister();
}

IMPLEMENT_MODULE(FSequenceNavigatorModule, SequenceNavigator)

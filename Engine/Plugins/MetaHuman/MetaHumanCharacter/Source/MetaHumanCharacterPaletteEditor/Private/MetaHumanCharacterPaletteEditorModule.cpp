// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterPaletteEditorModule.h"

#include "PaletteEditor/MetaHumanCharacterPaletteEditorCommands.h"
#include "MetaHumanCharacterPaletteEditorLog.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMetaHumanCharacterPaletteEditor);

#define LOCTEXT_NAMESPACE "MetaHumanCharacterPaletteEditor"

namespace UE::MetaHuman
{
const FName MessageLogName("MetaHuman");
}

FMetaHumanCharacterPaletteEditorModule& FMetaHumanCharacterPaletteEditorModule::GetChecked()
{
	return FModuleManager::GetModuleChecked<FMetaHumanCharacterPaletteEditorModule>(UE_MODULE_NAME);
}

void FMetaHumanCharacterPaletteEditorModule::StartupModule()
{
	FMetaHumanCharacterPaletteEditorCommands::Register();

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing(UE::MetaHuman::MessageLogName, LOCTEXT("MessageLogChannelName", "MetaHuman"));
}

void FMetaHumanCharacterPaletteEditorModule::ShutdownModule()
{
	FMetaHumanCharacterPaletteEditorCommands::Unregister();
}

IMPLEMENT_MODULE(FMetaHumanCharacterPaletteEditorModule, MetaHumanCharacterPaletteEditor);

#undef LOCTEXT_NAMESPACE

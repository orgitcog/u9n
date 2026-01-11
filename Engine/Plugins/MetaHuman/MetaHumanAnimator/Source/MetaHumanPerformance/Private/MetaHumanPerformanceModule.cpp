// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceModule.h"
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceExportUtils.h"
#include "Customizations/MetaHumanPerformanceCustomizations.h"
#include "UI/MetaHumanPerformanceStyle.h"
#include "Sequencer/MetaHumanPerformanceMediaTrackEditor.h"
#include "Sequencer/MetaHumanPerformanceAudioTrackEditor.h"
#include "ISequencerModule.h"
#include "PropertyEditorModule.h"


void FMetaHumanPerformanceModule::StartupModule()
{
	FMetaHumanPerformanceStyle::Register();

	// Registers the MetaHumanPerformanceTrackEditor to be used in Sequencer
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	MediaTrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FMetaHumanPerformanceMediaTrackEditor>();
	AudioTrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FMetaHumanPerformanceAudioTrackEditor>();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassToUnregisterOnShutdown = UMetaHumanPerformance::StaticClass()->GetFName();
	PropertyEditorModule.RegisterCustomClassLayout(ClassToUnregisterOnShutdown, FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanPerformanceCustomization::MakeInstance));
}

void FMetaHumanPerformanceModule::ShutdownModule()
{
	// Unregisters the MetaHumanPerformanceTrackEditor from Sequencer
	if (ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer"))
	{
		SequencerModulePtr->UnRegisterTrackEditor(MediaTrackEditorBindingHandle);
		SequencerModulePtr->UnRegisterTrackEditor(AudioTrackEditorBindingHandle);
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomClassLayout(ClassToUnregisterOnShutdown);
	}

	FMetaHumanPerformanceStyle::Unregister();
}

IMPLEMENT_MODULE(FMetaHumanPerformanceModule, MetaHumanPerformance)
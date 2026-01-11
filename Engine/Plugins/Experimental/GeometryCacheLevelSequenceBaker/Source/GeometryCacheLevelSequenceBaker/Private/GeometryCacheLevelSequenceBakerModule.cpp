// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheLevelSequenceBakerModule.h"

#include "FGeometryCacheLevelSequenceBakerCustomization.h"
#include "GeometryCacheLevelSequenceBakerCommands.h"
#include "GeometryCacheLevelSequenceBakerStyle.h"
#include "LevelSequenceEditorModule.h"
#include "SequencerCustomizationManager.h"

DEFINE_LOG_CATEGORY(LogGeometryCacheLevelSequenceBaker);

#define LOCTEXT_NAMESPACE "FGeometryCacheLevelSequenceBakerModule"

void FGeometryCacheLevelSequenceBakerModule::StartupModule()
{
	FGeometryCacheLevelSequenceBakerStyle::Register();	
	FGeometryCacheLevelSequenceBakerCommands::Register();
	
	ILevelSequenceEditorModule& LevelSequenceEditorModule = FModuleManager::Get().LoadModuleChecked<ILevelSequenceEditorModule>("LevelSequenceEditor");
	CustomizationHandle = LevelSequenceEditorModule.RegisterAdditionalLevelSequenceEditorCustomization(FOnGetSequencerCustomizationInstance::CreateLambda([]()
	{
		return new FGeometryCacheLevelSequenceBakerCustomization();
	}));
}

void FGeometryCacheLevelSequenceBakerModule::ShutdownModule()
{
	ILevelSequenceEditorModule& LevelSequenceEditorModule = FModuleManager::Get().LoadModuleChecked<ILevelSequenceEditorModule>("LevelSequenceEditor");
	LevelSequenceEditorModule.UnregisterAdditionalLevelSequenceEditorCustomization(CustomizationHandle);
}





#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGeometryCacheLevelSequenceBakerModule, GeometryCacheLevelSequenceBaker)
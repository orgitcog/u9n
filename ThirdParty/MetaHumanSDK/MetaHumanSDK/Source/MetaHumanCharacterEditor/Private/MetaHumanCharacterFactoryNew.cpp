// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterFactoryNew.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterAnalytics.h"
#include "Editor/EditorEngine.h"

extern UNREALED_API UEditorEngine* GEditor;

UMetaHumanCharacterFactoryNew::UMetaHumanCharacterFactoryNew()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanCharacter::StaticClass();
}

UObject* UMetaHumanCharacterFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* Context, FFeedbackContext* Warn)
{
	UMetaHumanCharacter* NewMetaHumanCharacter = NewObject<UMetaHumanCharacter>(InParent, InClass, InName, InFlags | RF_Transactional);

	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
	MetaHumanCharacterEditorSubsystem->InitializeMetaHumanCharacter(NewMetaHumanCharacter);

	check(NewMetaHumanCharacter->IsCharacterValid());
	UE::MetaHuman::Analytics::RecordNewCharacterEvent(NewMetaHumanCharacter);

	return NewMetaHumanCharacter;
}
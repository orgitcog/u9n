// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultEditorPipeline.h"

#include "MetaHumanCharacterActorInterface.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "MetaHumanDefaultPipeline.h"

#include "BlueprintCompilationManager.h"
#include "Misc/UObjectToken.h"
#include "Logging/MessageLog.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "MetaHumanDefaultEditorPipeline"

UBlueprint* UMetaHumanDefaultEditorPipeline::WriteActorBlueprint(const FString& InBlueprintPath) const
{
	const TSubclassOf<AActor> ActorClass = GetRuntimePipeline()->GetActorClass();

	if (!ActorClass)
	{
		return nullptr;
	}

	if (!ActorClass->ImplementsInterface(UMetaHumanCharacterActorInterface::StaticClass()))
	{
		FFormatNamedArguments FormatArguments;
		FormatArguments.Add(TEXT("BaseActorClass"), FText::FromString(ActorClass->GetPathName()));

		FText Message = FText::Format(
			LOCTEXT("ActorClassInterfaceError", "The actor class specified on the MetaHuman Character Pipeline, {BaseActorClass}, doesn't implement MetaHumanCharacterActorInterface."),
			FormatArguments);

		FMessageLog(UE::MetaHuman::MessageLogName).Error(Message)
			->AddToken(FUObjectToken::Create(ActorClass));

		return nullptr;
	}

	return WriteActorBlueprintHelper(
		ActorClass,
		InBlueprintPath,
		// Check if the existing blueprint is compatible
		[this, ActorClass](UBlueprint* InBlueprint) -> bool
		{
			return InBlueprint->ParentClass->IsChildOf(ActorClass);
		},
		// Generate a new one
		[&](UPackage* BPPackage) -> UBlueprint*
		{
			const FString BlueprintShortName = FPackageName::GetShortName(InBlueprintPath);
			return FKismetEditorUtilities::CreateBlueprint(
				ActorClass,
				BPPackage,
				FName(BlueprintShortName),
				BPTYPE_Normal,
				FName(TEXT("UMetaHumanDefaultEditorPipeline::WriteActorBlueprint")));
		});
}

bool UMetaHumanDefaultEditorPipeline::UpdateActorBlueprint(const UMetaHumanCharacterInstance* InCharacterInstance, UBlueprint* InBlueprint) const
{
	// Note that this will only work for TObjectPtr properties, not TSoftObjectPtr, etc.
	// 
	// We could add cases here for other property types if needed.
	if (FObjectProperty* CharacterProperty = CastField<FObjectProperty>(InBlueprint->GeneratedClass->FindPropertyByName(FName(TEXT("CharacterInstance")))))
	{
		void* PropertyAddress = CharacterProperty->ContainerPtrToValuePtr<void>(InBlueprint->GeneratedClass->GetDefaultObject(false));

		if (CharacterProperty->GetObjectPropertyValue(PropertyAddress) != InCharacterInstance)
		{
			CharacterProperty->SetObjectPropertyValue(PropertyAddress, const_cast<UMetaHumanCharacterInstance*>(InCharacterInstance));

			const FBPCompileRequest Request(InBlueprint, EBlueprintCompileOptions::None, nullptr);
			FBlueprintCompilationManager::CompileSynchronously(Request);

			// If needed, add LODSync component configuration here for different export qualities.

			InBlueprint->MarkPackageDirty();

			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

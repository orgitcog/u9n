// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneStateExtension.h"
#include "AvaSceneStateActor.h"
#include "AvaSceneStateBlueprint.h"
#include "BlueprintActionDatabase.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SceneStateActor.h"
#include "SceneStateBlueprintFactory.h"
#include "SceneStateObject.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "AvaSceneStateExtension"

void FAvaSceneStateExtension::ExtendToolbarMenu(UToolMenu& InMenu)
{
	FToolMenuSection& Section = InMenu.FindOrAddSection(DefaultSectionName);

	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("SceneStateButton")
		, FExecuteAction::CreateSP(this, &FAvaSceneStateExtension::OpenSceneStateBlueprintEditor)
		, LOCTEXT("SceneStateLabel", "Scene State")
		, LOCTEXT("SceneStateTooltip", "Opens the Scene State Editor for the given Motion Design Scene")
		, FSlateIconFinder::FindCustomIconForClass(UAvaSceneStateBlueprint::StaticClass(), TEXT("ClassThumbnail"))));

	Entry.StyleNameOverride = TEXT("CalloutToolbar");
}

void FAvaSceneStateExtension::Cleanup()
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	FBlueprintActionDatabase* BlueprintActionDatabase = FBlueprintActionDatabase::TryGet();
	if (!BlueprintActionDatabase)
	{
		return;
	}

	for (AAvaSceneStateActor* SceneStateActor : TActorRange<AAvaSceneStateActor>(World))
	{
		if (SceneStateActor->SceneStateBlueprint)
		{
			BlueprintActionDatabase->ClearAssetActions(SceneStateActor->SceneStateBlueprint.Get());
		}
	}
}

AAvaSceneStateActor* FAvaSceneStateExtension::FindOrSpawnActor() const
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (AAvaSceneStateActor* SceneStateActor : TActorRange<AAvaSceneStateActor>(World))
	{
		return SceneStateActor;
	}

	return World->SpawnActor<AAvaSceneStateActor>();
}

USceneStateBlueprint* FAvaSceneStateExtension::CreateSceneStateBlueprint(AAvaSceneStateActor* InSceneStateActor) const
{
	check(InSceneStateActor);

	UFactory* SceneStateBlueprintFactory = NewObject<USceneStateBlueprintFactory>(GetTransientPackage());
	check(SceneStateBlueprintFactory);

	USceneStateBlueprint* NewSceneStateBlueprint = CastChecked<USceneStateBlueprint>(SceneStateBlueprintFactory->FactoryCreateNew(UAvaSceneStateBlueprint::StaticClass()
		, InSceneStateActor
		, TEXT("SceneStateBlueprint")
		, RF_Transactional
		, nullptr
		, GWarn));

	// Clear Standalone flags as this Blueprint will be under the Scene State Actor
	NewSceneStateBlueprint->ClearFlags(RF_Standalone);

	InSceneStateActor->SetSceneStateBlueprint(NewSceneStateBlueprint);
	InSceneStateActor->UpdateSceneStateClass();

	return NewSceneStateBlueprint;
}

void FAvaSceneStateExtension::OpenSceneStateBlueprintEditor()
{
	AAvaSceneStateActor* SceneStateActor = FindOrSpawnActor();
	if (!SceneStateActor)
	{
		return;
	}

	if (!SceneStateActor->SceneStateBlueprint)
	{
		CreateSceneStateBlueprint(SceneStateActor);
	}
	check(SceneStateActor->SceneStateBlueprint);

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	check(AssetEditorSubsystem);
	AssetEditorSubsystem->OpenEditorForAsset(SceneStateActor->SceneStateBlueprint);
}

#undef LOCTEXT_NAMESPACE

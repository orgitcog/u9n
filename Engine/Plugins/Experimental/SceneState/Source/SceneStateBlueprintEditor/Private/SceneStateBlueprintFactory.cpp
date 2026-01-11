// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBlueprintFactory.h"
#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/MessageDialog.h"
#include "SceneStateBlueprint.h"
#include "SceneStateBlueprintEditor.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateMachineGraph.h"
#include "SceneStateMachineGraphSchema.h"
#include "SceneStateObject.h"

#define LOCTEXT_NAMESPACE "SceneStateBlueprintFactory"

USceneStateBlueprintFactory::USceneStateBlueprintFactory()
{
	ParentClass = USceneStateObject::StaticClass();
	SupportedClass = USceneStateBlueprint::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UEdGraph* USceneStateBlueprintFactory::AddStateMachine(USceneStateBlueprint* InBlueprint)
{
	check(InBlueprint);

	const FName GraphName = FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint, TEXT("State Machine"));

	UEdGraph* NewStateMachineGraph = FBlueprintEditorUtils::CreateNewGraph(InBlueprint
		, GraphName
		, USceneStateMachineGraph::StaticClass()
		, USceneStateMachineGraphSchema::StaticClass());

	// Allocate Default State Machine Nodes (i.e. Entry node)
	const UEdGraphSchema* Schema = NewStateMachineGraph->GetSchema();
	check(Schema);
	Schema->CreateDefaultNodesForGraph(*NewStateMachineGraph);

	InBlueprint->StateMachineGraphs.Add(NewStateMachineGraph);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);

	return NewStateMachineGraph;
}

FText USceneStateBlueprintFactory::GetDisplayName() const
{
	return SupportedClass ? SupportedClass->GetDisplayNameText() : Super::GetDisplayName();
}

FString USceneStateBlueprintFactory::GetDefaultNewAssetName() const
{
	// Short name removing "Motion Design" and "SceneState" prefix for new assets
	return TEXT("NewBlueprint");
}

uint32 USceneStateBlueprintFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	return AssetTools.FindAdvancedAssetCategory("MotionDesignCategory");
}

UObject* USceneStateBlueprintFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	return FactoryCreateNew(InClass, InParent, InName, InFlags, InContext, InWarn, NAME_None);
}

UObject* USceneStateBlueprintFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn, FName InCallingContext)
{
	check(InClass && InClass->IsChildOf<USceneStateBlueprint>());

	if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf<USceneStateObject>())
	{
		FMessageDialog::Open(EAppMsgType::Ok
			, FText::Format(LOCTEXT("InvalidParentClassMessage", "Unable to create Scene State Blueprint with parent class '{0}'.")
			, FText::FromString(GetNameSafe(ParentClass))));
		return nullptr;
	}

	// Create Blueprint
	USceneStateBlueprint* Blueprint = CastChecked<USceneStateBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass
		, InParent
		, InName
		, EBlueprintType::BPTYPE_Normal
		, InClass
		, USceneStateGeneratedClass::StaticClass()
		, InCallingContext));

	check(Blueprint && Blueprint->GeneratedClass);

	checkf(Cast<USceneStateGeneratedClass>(Blueprint->GeneratedClass) != nullptr
		, TEXT("Scene State Blueprint generated class is not properly set up for %s.\n"
			"Ensure that this Scene State Blueprint class has a Scene State compiler registered via ISceneStateBlueprintEditorModule")
		, *GetNameSafe(Blueprint->GetClass()));

	UEdGraph* StateMachineGraph = USceneStateBlueprintFactory::AddStateMachine(Blueprint);

	Blueprint->LastEditedDocuments.AddUnique(StateMachineGraph);

	return Blueprint;
}

#undef LOCTEXT_NAMESPACE

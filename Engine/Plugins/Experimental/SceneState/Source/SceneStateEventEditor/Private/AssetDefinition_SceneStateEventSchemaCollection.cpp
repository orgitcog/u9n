// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_SceneStateEventSchemaCollection.h"
#include "BlueprintEditorModule.h"
#include "Kismet2/StructureEditorUtils.h"
#include "SceneStateEventSchemaCollection.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_SceneStateEventSchemaCollection"

FText UAssetDefinition_SceneStateEventSchemaCollection::GetAssetDisplayName() const
{
	return LOCTEXT("AssetDisplayName", "Scene State Event Schema Collection");
}

FLinearColor UAssetDefinition_SceneStateEventSchemaCollection::GetAssetColor() const
{
	return FLinearColor(FColor(152, 251, 152));
}

TSoftClassPtr<UObject> UAssetDefinition_SceneStateEventSchemaCollection::GetAssetClass() const
{
	return USceneStateEventSchemaCollection::StaticClass();
}

#undef LOCTEXT_NAMESPACE
   
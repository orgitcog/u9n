// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaEditorExtension.h"

class AAvaSceneStateActor;
class USceneStateBlueprint;

class FAvaSceneStateExtension : public FAvaEditorExtension
{
public:
	UE_AVA_INHERITS(FAvaSceneStateExtension, FAvaEditorExtension);

	//~ Begin IAvaEditorExtension
	virtual void ExtendToolbarMenu(UToolMenu& InMenu) override;
	virtual void Cleanup() override;
	//~ End IAvaEditorExtension

private:
	AAvaSceneStateActor* FindOrSpawnActor() const;

	USceneStateBlueprint* CreateSceneStateBlueprint(AAvaSceneStateActor* InSceneStateActor) const;

	void OpenSceneStateBlueprintEditor();
};

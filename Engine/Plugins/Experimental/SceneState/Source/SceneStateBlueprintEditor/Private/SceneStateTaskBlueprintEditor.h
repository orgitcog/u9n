// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"

class USceneStateTaskBlueprint;

namespace UE::SceneState::Editor
{

class FSceneStateTaskBlueprintEditor : public FBlueprintEditor
{
public:
	using Super = FBlueprintEditor;

	void Init(USceneStateTaskBlueprint* InBlueprint, const FAssetOpenArgs& InOpenArgs);

protected:
	//~ Begin IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	//~ End IToolkit Interface
};

} // UE::SceneState::Editor

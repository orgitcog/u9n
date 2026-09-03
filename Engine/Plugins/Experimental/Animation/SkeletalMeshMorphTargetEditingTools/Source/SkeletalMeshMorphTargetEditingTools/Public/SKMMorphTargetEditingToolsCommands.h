// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FSkeletalMeshMorphTargetEditingToolsCommands : public TCommands<FSkeletalMeshMorphTargetEditingToolsCommands>
{
public:
	FSkeletalMeshMorphTargetEditingToolsCommands();


	TSharedPtr<FUICommandInfo> BeginMorphTargetTool;
	TSharedPtr<FUICommandInfo> BeginMorphTargetSculptTool;

	virtual void RegisterCommands() override;
};

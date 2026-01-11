// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITraitStackEditor.h"

namespace UE::AnimNext::Editor
{

// Interface used to talk to the trait stack editor embedded in a workspace
class FTraitStackEditor : public ITraitStackEditor
{
	// ITraitStackEditor interface
	virtual void SetTraitData(const TSharedRef<UE::Workspace::IWorkspaceEditor>& InWorkspaceEditor, const FTraitStackData& InTraitStackData) override;
};

}
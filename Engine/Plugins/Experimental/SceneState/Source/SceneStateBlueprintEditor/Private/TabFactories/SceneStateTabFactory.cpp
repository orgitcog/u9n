// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateTabFactory.h"
#include "SceneStateBlueprintEditor.h"

namespace UE::SceneState::Editor
{

FTabFactory::FTabFactory(FName InTabId, const TSharedRef<FSceneStateBlueprintEditor>& InEditor)
	: FWorkflowTabFactory(InTabId, InEditor)
{
}

TSharedPtr<FSceneStateBlueprintEditor> FTabFactory::GetEditor() const
{
	return StaticCastSharedPtr<FSceneStateBlueprintEditor>(HostingApp.Pin());
}

} // UE::SceneState::Editor

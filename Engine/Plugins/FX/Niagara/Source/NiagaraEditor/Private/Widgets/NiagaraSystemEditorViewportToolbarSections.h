// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"
#include "ToolMenuEntry.h"

class SNiagaraSystemViewport;

namespace UE::NiagaraSystemEditor
{
FToolMenuEntry CreateShowSubmenu();
void FillShowSubmenu(UToolMenu* InMenu, bool bInShowViewportStatsToggle = true);

TSharedRef<SWidget> CreateMotionMenuWidget(const TSharedRef<SNiagaraSystemViewport>& InNiagaraSystemEditorViewport);
void FillSettingsSubmenu(UToolMenu* InMenu);
void AddMotionSettingsToSection(FToolMenuSection& InSection);
void ExtendPreviewSceneSettingsSubmenu(FName InSubmenuName);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

// Dummy module, currently only used by Viewport Toolbar to show/hide the "Use Experimental Gizmos" entry.
class FGizmoSettingsModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FGizmoSettingsModule, GizmoSettings)

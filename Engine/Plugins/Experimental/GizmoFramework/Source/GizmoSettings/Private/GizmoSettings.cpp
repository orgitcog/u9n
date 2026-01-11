// Copyright Epic Games, Inc. All Rights Reserved.

#include "GizmoSettings.h"
#include "EditorInteractiveGizmoManager.h"

void UGizmoSettings::PostInitProperties()
{
	Super::PostInitProperties();

	bool bConfigNeedsUpdate = false;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (bEnableNewGizmos_DEPRECATED)
	{
		UEditorInteractiveGizmoManager::SetUsesNewTRSGizmos(bEnableNewGizmos_DEPRECATED);
		bEnableNewGizmos_DEPRECATED = false;

		bConfigNeedsUpdate = true;
	}

	FGizmosParameters DefaultGizmosParameters;
	if (GizmoParameters_DEPRECATED.bCtrlMiddleDoesY != DefaultGizmosParameters.bCtrlMiddleDoesY ||
		GizmoParameters_DEPRECATED.bEnableExplicit != DefaultGizmosParameters.bEnableExplicit ||
		GizmoParameters_DEPRECATED.RotateMode != DefaultGizmosParameters.RotateMode)
	{
		UEditorInteractiveGizmoManager::SetGizmosParameters(GizmoParameters_DEPRECATED);

		bConfigNeedsUpdate = true;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (bConfigNeedsUpdate)
	{
		SaveConfig();
	}
}

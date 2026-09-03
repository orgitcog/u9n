// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransformGizmoEditorSettings.h"
#include "Editor.h"
#include "EditorInteractiveGizmoManager.h"
#include "Settings/LevelEditorViewportSettings.h"

UTransformGizmoEditorSettings::UTransformGizmoEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (ULevelEditorViewportSettings* const ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>())
	{
		bEnableArcballRotate = ViewportSettings->bAllowArcballRotate;
		bEnableScreenRotate = ViewportSettings->bAllowScreenRotate;
		bEnableAxisDrawing = ViewportSettings->bAllowEditWidgetAxisDisplay;
		bEnableCombinedTranslateRotate = ViewportSettings->bAllowTranslateRotateZWidget;

		ViewportSettings->OnSettingChanged().AddUObject(this, &UTransformGizmoEditorSettings::OnLegacySettingChanged);
	}
}

#if WITH_EDITOR
void UTransformGizmoEditorSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedEvent);

	if (InPropertyChangedEvent.PropertyChain.GetHead())
	{
		if (const FProperty* const Property = InPropertyChangedEvent.PropertyChain.GetHead()->GetValue())
		{
			const FName ChangedPropertyName = Property->GetFName();
			static const FName GizmosParametersName = GET_MEMBER_NAME_CHECKED(UTransformGizmoEditorSettings, GizmosParameters);

			if (GizmosParametersName == ChangedPropertyName)
			{
				BroadcastGizmosParametersChange();
			}
		}
	}
}

void UTransformGizmoEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	static const FName UseExperimentalGizmoName = GET_MEMBER_NAME_CHECKED(UTransformGizmoEditorSettings, bUseExperimentalGizmo);
	static const FName EnableArcballRotateName = GET_MEMBER_NAME_CHECKED(UTransformGizmoEditorSettings, bEnableArcballRotate);
	static const FName EnableScreenRotateName = GET_MEMBER_NAME_CHECKED(UTransformGizmoEditorSettings, bEnableScreenRotate);
	static const FName EnableAxisDrawingName = GET_MEMBER_NAME_CHECKED(UTransformGizmoEditorSettings, bEnableAxisDrawing);
	static const FName EnableCombinedTranslateRotateName = GET_MEMBER_NAME_CHECKED(UTransformGizmoEditorSettings, bEnableCombinedTranslateRotate);

	if (!InPropertyChangedEvent.Property)
	{
		return;
	}

	const FName ChangedPropertyName = InPropertyChangedEvent.Property->GetFName();

	if (ChangedPropertyName == UseExperimentalGizmoName)
	{
		BroadcastNewTRSGizmoChange();
	}
	else if (ChangedPropertyName == EnableArcballRotateName)
	{
		if (ULevelEditorViewportSettings* const ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>())
		{
			ViewportSettings->bAllowArcballRotate = bEnableArcballRotate;
		}
	}
	else if (ChangedPropertyName == EnableScreenRotateName)
	{
		if (ULevelEditorViewportSettings* const ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>())
		{
			ViewportSettings->bAllowScreenRotate = bEnableScreenRotate;
		}
	}
	else if (ChangedPropertyName == EnableAxisDrawingName)
	{
		if (ULevelEditorViewportSettings* const ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>())
		{
			ViewportSettings->bAllowEditWidgetAxisDisplay = bEnableAxisDrawing;
		}
	}
	else if (ChangedPropertyName == EnableCombinedTranslateRotateName)
	{
		if (ULevelEditorViewportSettings* const ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>())
		{
			ViewportSettings->bAllowTranslateRotateZWidget = bEnableCombinedTranslateRotate;
		}
	}
}
#endif // WITH_EDITOR

void UTransformGizmoEditorSettings::SetUseExperimentalGizmo(bool bInUseExperimentalGizmo)
{
	if (bUseExperimentalGizmo != bInUseExperimentalGizmo)
	{
		bUseExperimentalGizmo = bInUseExperimentalGizmo;
		SaveConfig();

		BroadcastNewTRSGizmoChange();

		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}
	}
}

bool UTransformGizmoEditorSettings::UsesLegacyGizmo() const
{
	return !UsesNewTRSGizmo();
}

bool UTransformGizmoEditorSettings::UsesNewTRSGizmo() const
{
	return bUseExperimentalGizmo;
}

void UTransformGizmoEditorSettings::SetGizmosParameters(const FGizmosParameters& InGizmosParameters)
{
	GizmosParameters = InGizmosParameters;
	SaveConfig();

	BroadcastGizmosParametersChange();
}

void UTransformGizmoEditorSettings::SetTransformGizmoSize(float InTransformGizmoSize)
{
	if (TransformGizmoSize != InTransformGizmoSize)
	{
		TransformGizmoSize = InTransformGizmoSize;
		SaveConfig();

		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}
	}
}

void UTransformGizmoEditorSettings::BroadcastNewTRSGizmoChange() const
{
	UEditorInteractiveGizmoManager::OnUsesNewTRSGizmosChangedDelegate().Broadcast(UsesNewTRSGizmo());
}

void UTransformGizmoEditorSettings::BroadcastGizmosParametersChange() const
{
	UEditorInteractiveGizmoManager::OnGizmosParametersChangedDelegate().Broadcast(GizmosParameters);
}

void UTransformGizmoEditorSettings::OnLegacySettingChanged(FName InPropertyName)
{
	// Retrieving names from ULevelEditorViewportSettings
	static const FName AllowArcballRotateName = GET_MEMBER_NAME_CHECKED(ULevelEditorViewportSettings, bAllowArcballRotate);
	static const FName AllowScreenRotateName = GET_MEMBER_NAME_CHECKED(ULevelEditorViewportSettings, bAllowScreenRotate);
	static const FName AllowEditWidgetAxisDisplayName = GET_MEMBER_NAME_CHECKED(ULevelEditorViewportSettings, bAllowEditWidgetAxisDisplay);
	static const FName AllowTranslateRotateZWidgetName = GET_MEMBER_NAME_CHECKED(ULevelEditorViewportSettings, bAllowTranslateRotateZWidget);

	if (InPropertyName == AllowArcballRotateName)
	{
		if (const ULevelEditorViewportSettings* const ViewportSettings = GetDefault<ULevelEditorViewportSettings>())
		{
			bEnableArcballRotate = ViewportSettings->bAllowArcballRotate;
		}
	}
	else if (InPropertyName == AllowScreenRotateName)
	{
		if (const ULevelEditorViewportSettings* const ViewportSettings = GetDefault<ULevelEditorViewportSettings>())
		{
			bEnableScreenRotate = ViewportSettings->bAllowScreenRotate;
		}
	}
	else if (InPropertyName == AllowEditWidgetAxisDisplayName)
	{
		if (const ULevelEditorViewportSettings* const ViewportSettings = GetDefault<ULevelEditorViewportSettings>())
		{
			bEnableAxisDrawing = ViewportSettings->bAllowEditWidgetAxisDisplay;
		}
	}
	else if (InPropertyName == AllowTranslateRotateZWidgetName)
	{
		if (const ULevelEditorViewportSettings* const ViewportSettings = GetDefault<ULevelEditorViewportSettings>())
		{
			bEnableCombinedTranslateRotate = ViewportSettings->bAllowTranslateRotateZWidget;
		}
	}
}

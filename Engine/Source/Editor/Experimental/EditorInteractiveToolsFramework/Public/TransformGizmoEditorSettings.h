// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "EditorGizmos/TransformGizmo.h"

#include "TransformGizmoEditorSettings.generated.h"

UCLASS(BlueprintType, Config = EditorPerProjectUserSettings, meta = (DisplayName = "Transform Gizmo"))
class EDITORINTERACTIVETOOLSFRAMEWORK_API UTransformGizmoEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTransformGizmoEditorSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif

	UPROPERTY(Config, EditAnywhere, Category = "Transform Gizmo", DisplayName = "Transform Gizmo Size", meta = (ClampMin = "-10.0", ClampMax = "150.0", EditCondition = "!bUseExperimentalGizmo"))
	float TransformGizmoSize = 0.0f;

	/**
	 * Allow arcball rotation with rotate widget
	 * (updates the setting with the same name found in Level Editor Viewport Settings)
	 */
	UPROPERTY(EditAnywhere, Category = "Transform Gizmo", DisplayName = "Enable Arcball Rotate", meta = (EditCondition = "!bUseExperimentalGizmo"))
	bool bEnableArcballRotate;

	/**
	 * Allow screen rotation with rotate widget
	 * (updates the setting with the same name found in Level Editor Viewport Settings)
	 */
	UPROPERTY(EditAnywhere, Category = "Transform Gizmo", DisplayName = "Enable Screen Rotate", meta = (EditCondition = "!bUseExperimentalGizmo"))
	bool bEnableScreenRotate;

	/**
	 * If true, the Edit widget of a transform will display the axis
	 * (updates the setting with the same name found in Level Editor Viewport Settings)
	 */
	UPROPERTY(EditAnywhere, Category = "Transform Gizmo", DisplayName = "Enable Axis drawing for Transform Edit Widget", meta = (EditCondition = "!bUseExperimentalGizmo"))
	bool bEnableAxisDrawing;

	/**
	 * Allow translate/rotate widget
	 * (updates the setting with the same name found in Level Editor Viewport Settings)
	 */
	UPROPERTY(EditAnywhere, Category = "Transform Gizmo", DisplayName = "Enable Combined Translate/Rotate Widget", meta = (EditCondition = "!bUseExperimentalGizmo"))
	bool bEnableCombinedTranslateRotate;
	//~ End Transform Gizmo Category

	//~ Begin Experimental Gizmo Category
	/** If true, the new TRS gizmos will be used. */
	UPROPERTY(Config, EditAnywhere, Category = "Experimental", DisplayName = "Enable Experimental Gizmo")
	bool bUseExperimentalGizmo = false;

	UPROPERTY(Config, EditAnywhere, Category = "Experimental", meta = (ShowOnlyInnerProperties),  meta = (EditCondition = "bUseExperimentalGizmo"))
	FGizmosParameters GizmosParameters;
	//~ End Experimental Gizmo Category

	void SetUseExperimentalGizmo(bool bInUseExperimentalGizmo);
	bool UsesLegacyGizmo() const;
	bool UsesNewTRSGizmo() const;

	void SetGizmosParameters(const FGizmosParameters& InGizmosParameters);
	void SetTransformGizmoSize(float InTransformGizmoSize);

private:
	void BroadcastNewTRSGizmoChange() const;
	void BroadcastGizmosParametersChange() const;

	void OnLegacySettingChanged(FName InPropertyName);
};

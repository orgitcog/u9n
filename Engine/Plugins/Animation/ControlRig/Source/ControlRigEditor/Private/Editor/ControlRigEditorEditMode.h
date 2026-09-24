// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditMode/ControlRigEditMode.h"

class UPersonaOptions;

class FControlRigEditorEditMode : public FControlRigEditMode
{
public:
	static inline const FLazyName ModeName = FLazyName(TEXT("EditMode.ControlRigEditor"));
	
	virtual bool IsInLevelEditor() const override { return false; }
	virtual bool AreEditingControlRigDirectly() const override { return true; }

	// FEdMode interface
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	/* IPersonaEditMode interface */
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override;

	/* FControlRigEditMode interface */
	virtual bool HandleBeginTransform(const FEditorViewportClient* InViewportClient) override;

	/** If set to true the edit mode will additionally render all bones */
	bool bDrawHierarchyBones = false;

	/** Drawing options */
	UPersonaOptions* ConfigOption = nullptr;

	// Elements which have been modified (e.g. controls moved)
	TArray<FRigElementKey> ModifiedRigElements;
};

class FModularRigEditorEditMode : public FControlRigEditorEditMode
{
public:

	static inline const FLazyName ModeName = FLazyName(TEXT("EditMode.ModularRigEditor"));
};

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EditorViewportClient.h"
#include "InputBehaviorSet.h"

struct FDataflowBaseElement;
class FEditorModeTools;
class FPreviewScene;
class SEditorViewport;
class HHitProxy;
class FDataflowPreviewSceneBase;

class DATAFLOWEDITOR_API FDataflowEditorViewportClientBase : public FEditorViewportClient, public IInputBehaviorSource
{
public:
	using Super = FEditorViewportClient;

	FDataflowEditorViewportClientBase(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene,  const bool bCouldTickScene, const TWeakPtr<SEditorViewport> InEditorViewportWidget = nullptr);
	
	virtual ~FDataflowEditorViewportClientBase();

protected:

	virtual void OnViewportClicked(HHitProxy* HitProxy) = 0;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	//~ Begin FEditorViewportClient interface
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	//~ End FEditorViewportClient interface

	/** Dataflow preview scene from the toolkit */
	FDataflowPreviewSceneBase* DataflowPreviewScene = nullptr;

	/** Behaviors defined by this base class */
	TArray<TObjectPtr<UInputBehavior>> BaseBehaviors;

	/** All behaviors available to the current viewport (subclasses can add to this set) */
	TObjectPtr<UInputBehaviorSet> BehaviorSet;

protected :

	/** Get all the scene selected elements */
	void GetSelectedElements(HHitProxy* HitProxy, TArray<FDataflowBaseElement*>& SelectedElements) const;

private:
	/** Handle the focus request onto the bounding box */
	void HandleFocusRequest(const FBox& BoundingBox);

	// IInputBehaviorSource
	virtual const UInputBehaviorSet* GetInputBehaviors() const override;

	/** Called from constructor/destructor to manage callbacks when settings change */
	void RegisterDelegates();
	void DeregisterDelegates();

	/** Handle for callback when preview scene profile settings change */
	FDelegateHandle OnAssetViewerSettingsChangedDelegateHandle;

	/** Handle for callback when preview scene focus request is triggered */
	FDelegateHandle OnFocusRequestDelegateHandle;
};

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowEditorViewportClientBase.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowComponentSelectionState.h"
#include "Dataflow/DataflowContent.h"
#include "Delegates/Delegate.h"
#include "InputBehaviorSet.h"


class FDataflowEditorToolkit;
class ADataflowActor;
class UTransformProxy;
class UCombinedTransformGizmo;
class FTransformGizmoDataBinder;
class FDataflowPreviewSceneBase;
class UInputBehaviorSet;
class USelection;
class FDataflowConstructionViewportClient;
class IClickDragBehaviorTarget;
namespace UE::Dataflow
{
	class IDataflowConstructionViewMode;
}

class DATAFLOWEDITOR_API FDataflowConstructionViewportClient : public FDataflowEditorViewportClientBase
{
public:
	using Super = FDataflowEditorViewportClientBase;

	FDataflowConstructionViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene,  const bool bCouldTickScene,
								  const TWeakPtr<SEditorViewport> InEditorViewportWidget = nullptr);

	virtual ~FDataflowConstructionViewportClient();

	void SetConstructionViewMode(const UE::Dataflow::IDataflowConstructionViewMode* InViewMode);

	// IInputBehaviorSource
	// virtual const UInputBehaviorSet* GetInputBehaviors() const override;
	USelection* GetSelectedComponents();

	/** Set the data flow toolkit used to create the client*/
	void SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> DataflowToolkit);
	
	/** Get the data flow toolkit  */
	const TWeakPtr<FDataflowEditorToolkit>& GetDataflowEditorToolkit() const { return DataflowEditorToolkitPtr; }

	/** Set the tool command list */
	void SetToolCommandList(TWeakPtr<FUICommandList> ToolCommandList);

	// FGCObject Interface
	virtual FString GetReferencerName() const override { return TEXT("FDataflowConstructionViewportClient"); }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSelectionChangedMulticast, const TArray<UPrimitiveComponent*>&, const TArray<FDataflowBaseElement*>&)
	FOnSelectionChangedMulticast OnSelectionChangedMulticast;

	FString GetOverlayString() const;

protected:

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:

	// FDataflowEditorViewportClientBase
	virtual void OnViewportClicked(HHitProxy* HitProxy) override;

	// FEditorViewportClient interface
	virtual void Tick(float DeltaSeconds) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;
	virtual float GetMinimumOrthoZoom() const override;

	TUniquePtr<IClickDragBehaviorTarget> OrthoScrollBehaviorTarget;
	TArray<TObjectPtr<UInputBehavior>> BehaviorsFor2DMode;

	/** Toolkit used to create the viewport client */
	TWeakPtr<FDataflowEditorToolkit> DataflowEditorToolkitPtr = nullptr;

	/** Dataflow preview scene from the toolkit */
	FDataflowPreviewSceneBase* PreviewScene = nullptr;

	// @todo(brice) : Is this needed?
	TWeakPtr<FUICommandList> ToolCommandList;

	/** Construction view mode */
	const UE::Dataflow::IDataflowConstructionViewMode* ConstructionViewMode = nullptr;

	/** Flag to enable scene ticking from the client */
	bool bEnableSceneTicking = false;

	// Saved view transforms for the currently inactive view modes (e.g. store the 3D camera here while in 2D mode and vice-versa)
	TMap<FName, FViewportCameraTransform> SavedInactiveViewTransforms;
};

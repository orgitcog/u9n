// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/MeshSurfacePointTool.h"
#include "BaseTools/SingleClickTool.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshOctree3.h"
#include "Engine/HitResult.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorToolCommandChange.h"

#include "MetaHumanCharacterEditorMeshEditingTools.generated.h"

class UStaticMeshComponent;

UINTERFACE(MinimalAPI)
class UMeshStateChangeTransactorInterface : public UInterface
{
	GENERATED_BODY()
};
	 
class IMeshStateChangeTransactorInterface
{
	GENERATED_BODY()
	 
public:
	virtual FSimpleMulticastDelegate& GetStateChangedDelegate(UMetaHumanCharacter* InMetaHumanCharacter) = 0;

	virtual void CommitShutdownState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, EToolShutdownType InShutdownType, const FText& InCommandChangeDescription) = 0;
	
	virtual void StoreBeginDragState(UMetaHumanCharacter* InMetaHumanCharacter) = 0;
	virtual void CommitEndDragState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, const FText& InCommandChangeDescription) = 0;
};

/**
 * Face Tool Command change for undo/redo transactions.
 */
class FMetaHumanCharacterEditorFaceToolCommandChange : public FMetaHumanCharacterEditorToolCommandChange
{
public:
	FMetaHumanCharacterEditorFaceToolCommandChange(
		TSharedRef<const FMetaHumanCharacterIdentity::FState> InOldState,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<UInteractiveToolManager*> InToolManager);

	//~Begin FToolCommandChange interface
	virtual void Apply(UObject* InObject) override;
	virtual void Revert(UObject* InObject) override;
	//~End FToolCommandChange interface

private:
	TSharedRef<const FMetaHumanCharacterIdentity::FState> OldState;
	TSharedRef<const FMetaHumanCharacterIdentity::FState> NewState;
};

UCLASS()
class UMetaHumanCharacterEditorMeshEditingToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	/** Size of manipulator */
	UPROPERTY(EditAnywhere, Category = "Manipulators", meta = (UIMin = "0", UIMax = "5", ClampMin = "0", ClampMax = "5"))
	float Size = 1.0f;

	/** Mouse interaction speed of manipulator */
	UPROPERTY(EditAnywhere, Category = "Manipulators", meta = (UIMin = "0.01", UIMax = "1", ClampMin = "0.01", ClampMax = "1"))
	float Speed = 0.2f;

	/** Hide other manipulators while dragging */
	UPROPERTY(EditAnywhere, Category = "Manipulators")
	bool bHideWhileDragging = true;

	/** Toggle whether modeling is applied symmetrically */
	UPROPERTY(EditAnywhere, Category = "Manipulators")
	bool bSymmetricModeling = true;
};

UCLASS(Abstract)
class UMetaHumanCharacterEditorMeshEditingTool : public UMeshSurfacePointTool
{
	GENERATED_BODY()

public:
	//~Begin UMeshSurfacePointTool interface
	virtual bool HitTest(const FRay& InRay, FHitResult& OutHit) override;
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	virtual void OnTick(float InDeltaTime) override;
	virtual void OnClickPress(const FInputDeviceRay& InClickPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override;
	virtual void OnBeginDrag(const FRay& InRay) override;
	virtual void OnUpdateDrag(const FRay& InRay) override;
	virtual void OnEndDrag(const FRay& InRay) override;
	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return true; }
	//~End UMeshSurfacePointTool interface

	UMetaHumanCharacterEditorMeshEditingToolProperties* GetMeshEditingToolProperties() const { return MeshEditingToolProperties; }

protected:
	virtual void SetManipulatorDragState(int32 InManipulatorIndex, bool bInIsDragging);
	virtual void SetManipulatorHoverState(int32 InManipulatorIndex, bool bInIsHovering);
	virtual void SetManipulatorMarkedState(int32 InManipulatorIndex, bool bInIsMarked);
	virtual void UpdateManipulatorsScale();
	virtual const FText GetDescription() const;
	virtual const FText GetCommandChangeDescription() const PURE_VIRTUAL(UMetaHumanCharacterEditorMeshEditingTool::GetCommandChangeDescription, return FText(););
	virtual const FText GetCommandChangeIntermediateDescription() const PURE_VIRTUAL(UMetaHumanCharacterEditorMeshEditingTool::GetCommandChangeIntermediateDescription, return FText(););
	virtual class UStaticMesh* GetManipulatorMesh() const PURE_VIRTUAL(UMetaHumanCharacterEditorMeshEditingTool::GetManipulatorMesh, return {};);
	virtual class UMaterialInterface* GetManipulatorMaterial() const PURE_VIRTUAL(UMetaHumanCharacterEditorMeshEditingTool::GetManipulatorMaterial, return {};);
	virtual float GetManipulatorScale() const PURE_VIRTUAL(UMetaHumanCharacterEditorMeshEditingTool::GetManipulatorScale, return 0.0f;)
	virtual TArray<FVector3f> GetManipulatorPositions() const  PURE_VIRTUAL(UMetaHumanCharacterEditorMeshEditingTool::GetManipulatorPositions, return TArray<FVector3f>(););
	virtual TArray<FVector3f> TranslateManipulator(int32 InManipulatorIndex, const FVector3f& InDelta)
		PURE_VIRTUAL(UMetaHumanCharacterEditorFaceEditingTool::TranslateManipulator, return TArray<FVector3f>(););
	virtual void InitStateChangeTransactor() PURE_VIRTUAL(UMetaHumanCharacterEditorMeshEditingTool::InitStateChangeTransactor,);

	/**
	 * Create new manipulator in the given position and stores it to be referenced later
	 */
	virtual UStaticMeshComponent* CreateManipulator(const FVector3f& InPosition);

	/**
	 * Recreates manipulators actor and components based on positions array.
	 */
	void RecreateManipulators(const TArray<FVector3f>& InManipulatorPositions);

	/**
	 * Updates the positions of the manipulator components
	 */
	void UpdateManipulatorPositions(const TArray<FVector3f>& InPositions);

	/**
	 * Updates the positions of the manipulator components by calling GetManipulatorPositions() 
	 */
	void UpdateManipulatorPositions();

	/**
	 * Utility function to get a scene view of the current viewport
	 * The lifetime of FSceneViews objects is tied to the view family
	 * content in which it is created and are deleted when the view
	 * family goes out of scope so the scene view can only be used
	 * in the scoped of the given callback function
	 */
	void WithSceneView(TFunction<void(class FSceneView*)> InCallback) const;

	/**
	 * Checks if the manipulator is occluded by the mesh in ray direction
	 */
	bool IsManipulatorOccluded(const FRay& InRay) const;

protected:
	// Reference to the MetaHumanCharacter being edited
	UPROPERTY()
	TObjectPtr<class UMetaHumanCharacter> MetaHumanCharacter;

	// Mesh editing property set 
	UPROPERTY()
	TObjectPtr<class UMetaHumanCharacterEditorMeshEditingToolProperties> MeshEditingToolProperties;

	// An array of static mesh components that represents the manipulators in the viewport
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ManipulatorComponents;

	// An actor used to hold the manipulator components that the user can interact with
	UPROPERTY()
	TObjectPtr<class AInternalToolFrameworkActor> ManipulatorsActor = nullptr;

	UPROPERTY()
	TScriptInterface<IMeshStateChangeTransactorInterface> MeshStateChangeTransactor;

	// The previous pixel position used to calculate the manipulator's movement delta
	FVector2D OldPixelPos;

	// The current pixel position used to calculate the manipulator's movement delta
	FVector2D NewPixelPos;

	// Index of the selected manipulator
	int32 SelectedManipulator = INDEX_NONE;

	// Holds the movement to be applied in the next tick
	FVector3f PendingMoveDelta = FVector3f::ZeroVector;

	// Holds the movement starting from begin drag
	FVector3f BeginDragMoveDelta = FVector3f::ZeroVector;

	/** Delegate handle for the state change */
	FDelegateHandle DelegateHandle;
};


UCLASS()
class UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface

	/**
	* Delegate that executes on EPropertyChangeType::ValueSet property change event, i.e. when a property
	* value has finished being updated
	*/
	DECLARE_DELEGATE_OneParam(FOnPropertyValueSetDelegate, const FPropertyChangedEvent&);
	FOnPropertyValueSetDelegate OnPropertyValueSetDelegate;

	void CopyFrom(const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings);
	void CopyTo(FMetaHumanCharacterFaceEvaluationSettings& OutFaceEvaluationSettings);

public:
	/** Scale of vertex delta no represented by the head model */
	UPROPERTY(EditAnywhere, Category = "Head Parameters", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float GlobalDelta = 1.0f;

	/** Scale of the head relative to the body */
	UPROPERTY(EditAnywhere, Category = "Head Parameters", meta = (UIMin = "0.8", UIMax = "1.3", ClampMin = "0.8", ClampMax = "1.3"))
	float HeadScale = 1.0f;
};


UCLASS(Abstract)
class UMetaHumanCharacterEditorFaceTool : public UMetaHumanCharacterEditorMeshEditingTool
{
	GENERATED_BODY()

public:
	UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties* GetFaceToolHeadParameterProperties() const { return FaceToolHeadParameterProperties; }

	void ResetFace();

	void ResetFaceNeck();

protected:
	//~Begin UMetaHumanCharacterEditorMeshEditingTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	//~End UMetaHumanCharacterEditorMeshEditingTool interface

protected:
	UPROPERTY()
	TObjectPtr<class UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties> FaceToolHeadParameterProperties;

	void UpdateFaceToolHeadParameterProperties(TWeakObjectPtr<UInteractiveToolManager> ToolManager, const FMetaHumanCharacterFaceEvaluationSettings& FaceEvaluationSettings);

private:
	/** Keep track of previously set face evaluation settings */
	FMetaHumanCharacterFaceEvaluationSettings PreviousFaceEvaluationSettings;
};

UCLASS()
class UMetaHumanCharacterEditorMeshBlendToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
};

UCLASS(Abstract)
class UMetaHumanCharacterEditorMeshBlendTool : public UMetaHumanCharacterEditorFaceTool
{
	GENERATED_BODY()

public:
	/** Get the Blend Tool properties. */
	UMetaHumanCharacterEditorMeshBlendToolProperties* GetBlendToolProperties() const { return BlendProperties; }

	virtual void AddMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset, int32 InItemIndex) PURE_VIRTUAL(UMetaHumanCharacterEditorMeshBlendTool::AddMetaHumanCharacterPreset,);
	virtual void RemoveMetaHumanCharacterPreset(int32 InItemIndex) PURE_VIRTUAL(UMetaHumanCharacterEditorMeshBlendTool::RemoveMetaHumanCharacterPreset, );
	virtual void BlendToMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset) PURE_VIRTUAL(UMetaHumanCharacterEditorMeshBlendTool::BlendToMetaHumanCharacterPreset);

	//~Begin UMeshSurfacePointTool interface
	virtual void Setup() override;
	virtual void OnBeginDrag(const FRay& InRay) override;
	virtual void OnEndDrag(const FRay& InRay) override;
	virtual void OnTick(float InDeltaTime) override;
	virtual void OnClickPress(const FInputDeviceRay& InClickPos) override;
	//~End UMeshSurfacePointTool interface
	
	/** Properties of the Blend Tool. These are displayed in the details panel when the tool is activated. */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorMeshBlendToolProperties> BlendProperties;

protected:
	//~Begin UMetaHumanCharacterEditorMeshEditingTool interface
	virtual class UStaticMesh* GetManipulatorMesh() const override;
	virtual class UMaterialInterface* GetManipulatorMaterial() const override;
	virtual float GetManipulatorScale() const override;
	virtual TArray<FVector3f> TranslateManipulator(int32 InManipulatorIndex, const FVector3f& InDelta) override;
	//~End UMetaHumanCharacterEditorMeshEditingTool interface

	virtual TArray<FVector3f> BlendPresets(int32 InManipulatorIndex, const TArray<float>& Weights) PURE_VIRTUAL(UMetaHumanCharacterEditorMeshBlendTool::BlendPresets, return {};);

	/** Getting the radius of an ancestry circle */
	virtual float GetAncestryCircleRadius() const;

private:
	/** An array of static mesh components that represents the preset widgets for every manipulator in the viewport. */
	TArray<TObjectPtr<UStaticMeshComponent>> PresetItemComponents;

	/** Positions of the active gizmo preset widgets */
	TArray<FVector> PresetItemPositions;

	/** Creates preset manipulator widget for the selected manipulator in the viewport. */
	void CreatePresetItem(UStaticMesh* ManipulatorMesh, const float GizmoScale, const FVector WidgetPosition, UMaterialInterface* ManipulatorMaterial);

	/** Uses RBF function to calculate weights for presets on a given region. */
	static bool CalculateWeights(const FVector& InputPosition, const TArray<FVector>& Targets, TArray<float>& OutResult);

	/** Mesh used for Drag state of the tool */
	UStaticMesh* GetManipulatorDragHandleMesh() const;

	/** Material used for Drag state of the tool */
	UMaterialInterface* GetManipulatorDragHandleMaterial() const;

	/** Change material weight property based on handle position*/
	void SetWeightOnPresetMaterials(const TArray<float>& Weights);

	TObjectPtr<UStaticMeshComponent> AncestryCircleComponent;
};
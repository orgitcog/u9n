// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorSubTools.h"
#include "MetaHumanCharacterEditorMeshEditingTools.h"
#include "MetaHumanCharacterIdentity.h"
#include "Engine/HitResult.h"
#include "RBF/RBFSolver.h"
#include "InteractiveGizmo.h"

#include "MetaHumanCharacterEditorFaceEditingTools.generated.h"

UENUM()
enum class EMetaHumanCharacterFaceEditingTool : uint8
{
	Move,
	Sculpt,
	Blend,
};

UCLASS()
class UMetaHumanCharacterEditorFaceEditingToolBuilder : public UMetaHumanCharacterEditorToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

	UPROPERTY()
	EMetaHumanCharacterFaceEditingTool ToolType = EMetaHumanCharacterFaceEditingTool::Move;

protected:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};


UCLASS()
class UFaceStateChangeTransactor : public UObject, public IMeshStateChangeTransactorInterface
{
	GENERATED_BODY()

public:
	virtual FSimpleMulticastDelegate& GetStateChangedDelegate(UMetaHumanCharacter* InMetaHumanCharacter) override;

	virtual void CommitShutdownState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, EToolShutdownType InShutdownType, const FText& InCommandChangeDescription) override;
	
	virtual void StoreBeginDragState(UMetaHumanCharacter* InMetaHumanCharacter) override;
	virtual void CommitEndDragState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, const FText& InCommandChangeDescription) override;

	TSharedRef<FMetaHumanCharacterIdentity::FState> GetBeginDragState() const;

	bool IsDragStateValid() const;
	
protected:

	// Hold the state of the character when a dragging operation begins so it can be undone while the tool is active
	TSharedPtr<FMetaHumanCharacterIdentity::FState> BeginDragState;
};


UENUM()
enum class EMetaHumanCharacterMoveToolManipulationGizmos : uint8
{
	ScreenSpace,
	Translate,
	Rotate,
	UniformScale,
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterMoveToolManipulationGizmos, EMetaHumanCharacterMoveToolManipulationGizmos::Count);

UCLASS()
class UMetaHumanCharacterEditorFaceMoveToolProperties : public UInteractiveToolPropertySet
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

	UPROPERTY(EditAnywhere, Category = "Manipulators")
	EMetaHumanCharacterMoveToolManipulationGizmos GizmoType = EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace;
};

struct FGizmoBoundaryConstraintFunctions
{
	FVector3f BeginDragGizmoPosition;
	FVector3f MinGizmoPosition;
	FVector3f MaxGizmoPosition;
	const float BBoxReduction = 0.2f;
	const bool bExpandToCurrent = true;
	const float BBoxSoftBound = 0.2f;

	FVector3f BeginDragGizmoRotation;
	FVector3f MinGizmoRotation;
	FVector3f MaxGizmoRotation;

	float BeginDragGizmoScale;
	float MinGizmoScale;
	float MaxGizmoScale;

	FVector3f GizmoTranslationFunction(const FVector3f& Delta) const;
	FVector3f GizmoRotationFunction(const FVector3f& Delta) const;
};

UCLASS()
class UMetaHumanCharacterEditorFaceMoveTool : public UMetaHumanCharacterEditorFaceTool
{
	GENERATED_BODY()

public:
	UMetaHumanCharacterEditorFaceMoveToolProperties* GetFaceMoveToolProperties() const { return MoveProperties; }

	void SetGizmoType(EMetaHumanCharacterMoveToolManipulationGizmos InSelection);

protected:
	//~Begin UMeshSurfacePointTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	virtual void OnTick(float InDeltaTime) override;
	virtual void OnClickPress(const FInputDeviceRay& InClickPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override;
	virtual void OnBeginDrag(const FRay& InRay) override;
	virtual void OnUpdateDrag(const FRay& InRay) override;
	virtual void OnEndDrag(const FRay& InRay) override;
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	//~End UMeshSurfacePointTool interface	

	//~Begin UMetaHumanCharacterEditorMeshEditingTool interface
	virtual void InitStateChangeTransactor() override;
	virtual const FText GetDescription() const override;
	virtual const FText GetCommandChangeDescription() const override;
	virtual const FText GetCommandChangeIntermediateDescription() const override;
	virtual UStaticMesh* GetManipulatorMesh() const override;
	virtual UMaterialInterface* GetManipulatorMaterial() const override;
	virtual float GetManipulatorScale() const override;
	virtual TArray<FVector3f> GetManipulatorPositions() const override;
	virtual TArray<FVector3f> TranslateManipulator(int32 InManipulatorIndex, const FVector3f& InDelta) override;
	//~End UMetaHumanCharacterEditorMeshEditingTool interface

	/** Properties of the Move Tool. These are displayed in the details panel when the tool is activated. */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorFaceMoveToolProperties> MoveProperties;

private:
	/** Gizmo and it's proxy used for UE like manipulations */
	UPROPERTY()
	TObjectPtr<class UTransformProxy> TransformProxy;
	
	UPROPERTY()
	TObjectPtr<class UCombinedTransformGizmo> TransformGizmo;

	// Index of the manipulator that the gizmo is assigned to
	int32 SelectedGizmoManipulator = INDEX_NONE;

	// Transform at the start of the drag
	TOptional<FTransform> BeginDragTransform;

	// Current drag transform
	TOptional<FTransform> CurrentDragTransform;

	// Gizmo contraints data
	FGizmoBoundaryConstraintFunctions GizmoConstraints;

	// Records which elements have been modified during drag
	ETransformGizmoSubElements DraggedGizmoElements;
};

UCLASS()
class UMetaHumanCharacterEditorFaceSculptTool : public UMetaHumanCharacterEditorFaceTool
{
	GENERATED_BODY()

protected:
	//~Begin UMetaHumanCharacterEditorMeshEditingTool interface
	virtual void InitStateChangeTransactor() override;
	virtual const FText GetDescription() const override;
	virtual const FText GetCommandChangeDescription() const override;
	virtual const FText GetCommandChangeIntermediateDescription() const override;
	virtual UStaticMesh* GetManipulatorMesh() const override;
	virtual UMaterialInterface* GetManipulatorMaterial() const override;
	virtual float GetManipulatorScale() const override;
	virtual TArray<FVector3f> GetManipulatorPositions() const override;
	virtual TArray<FVector3f> TranslateManipulator(int32 InManipulatorIndex, const FVector3f& InDelta) override;
	virtual void Render(IToolsContextRenderAPI* InRenderAPI) override;
	virtual bool HitTest(const FRay& InRay, FHitResult& OutHit) override;
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	virtual void OnTick(float InDeltaTime) override;
	virtual void OnBeginDrag(const FRay& InRay) override;
	virtual void OnUpdateDrag(const FRay& InRay) override;
	virtual void OnEndDrag(const FRay& InRay) override;
	virtual void OnCancelDrag() override;
	//~End UMetaHumanCharacterEditorMeshEditingTool interface

private:
	// Saves the current hit if a ray intersects the base mesh
	int32 HitVertexID = -1;
	FVector HitVertex;
	FVector HitNormal;

	// Flag whether Ctrl was pressed during when a dragging operation started
	// This is used to enter the add/remove landmark mode
	bool CtrlToggledOnBeginDrag;

	TArray<FVector3f> DebugVertices;

	/** Keep track of previously set face evaluation settings */
	FMetaHumanCharacterFaceEvaluationSettings PreviousFaceEvaluationSettings;
};

UCLASS()
class UMetaHumanCharacterEditorFaceBlendToolProperties : public UMetaHumanCharacterEditorMeshBlendToolProperties
{
	GENERATED_BODY()

public:
	/** Blend facial features, proportions, or both */
	UPROPERTY(EditAnywhere, Category = "BlendTool", meta = (ShowOnlyInnerProperties))
	EBlendOptions BlendOptions = EBlendOptions::Both;
};

UCLASS()
class UMetaHumanCharacterEditorFaceBlendTool : public UMetaHumanCharacterEditorMeshBlendTool
{
	GENERATED_BODY()

public:
	//~Begin UMetaHumanCharacterEditorMeshBlendTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	virtual void AddMetaHumanCharacterPreset(class UMetaHumanCharacter* InCharacterPreset, int32 InItemIndex) override;
	virtual void RemoveMetaHumanCharacterPreset(int32 InItemIndex) override;
	virtual void BlendToMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset) override;
	//~End UMetaHumanCharacterEditorMeshBlendTool interface

protected:
	//~Begin UMetaHumanCharacterEditorMeshEditingTool interface
	virtual void InitStateChangeTransactor() override;
	virtual const FText GetDescription() const override;
	virtual const FText GetCommandChangeDescription() const override;
	virtual const FText GetCommandChangeIntermediateDescription() const override;
	virtual TArray<FVector3f> GetManipulatorPositions() const override;
	//~End UMetaHumanCharacterEditorMeshEditingTool interface

	//~Begin UMetaHumanCharacterEditorMeshBlendTool interface
	virtual TArray<FVector3f> BlendPresets(int32 InManipulatorIndex, const TArray<float>& Weights);
	//~End UMetaHumanCharacterEditorMeshBlendTool interface

private:
	//void UpdateFaceBlendToolProperties(TWeakObjectPtr<UInteractiveToolManager> ToolManager, const FMetaHumanCharacterFaceEvaluationSettings& FaceEvaluationSettings);

private:
	/** Holds the face states of the presets. */
	TArray<TSharedPtr<const FMetaHumanCharacterIdentity::FState>> PresetStates;

	/** Keep track of previously set face evaluation settings */
	FMetaHumanCharacterFaceEvaluationSettings PreviousFaceEvaluationSettings;
};
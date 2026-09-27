// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "MetaHumanCharacterEditorMeshEditingTools.h"
#include "MetaHumanCharacterEditorSubTools.h"
#include "BaseTools/SingleTargetWithSelectionTool.h"
#include "MetaHumanCharacterBodyIdentity.h"

#include "MetaHumanCharacterEditorBodyEditingTools.generated.h"

enum class EMetaHumanClothingVisibilityState : uint8;

UENUM()
enum class EMetaHumanCharacterBodyEditingTool : uint8
{
	Model,
	Blend
};

UCLASS()
class UMetaHumanCharacterEditorBodyToolBuilder : public UMetaHumanCharacterEditorToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

	UPROPERTY()
	EMetaHumanCharacterBodyEditingTool ToolType = EMetaHumanCharacterBodyEditingTool::Blend;

protected:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};

class FMetaHumanCharacterClothVisibilityBase
{
protected:
	/** Storage for the last preview material set */
	TOptional<EMetaHumanCharacterSkinPreviewMaterial> SavedPreviewMaterial;

	/** Helper to update the visibility of the input character if needed */
	void UpdateClothVisibility(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, EMetaHumanClothingVisibilityState InVisibilityState, bool bUpdateMaterialHiddenFaces = true);
};

UCLASS(Abstract)
class UMetaHumanCharacterBodyModelSubToolBase : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	virtual void SetEnabled(bool bInIsEnabled)
	{
		bSubToolActive = bInIsEnabled;
	};

	UPROPERTY(Transient)
	bool bSubToolActive = true;
};


struct FMetaHumanCharacterBodyConstraintItem
{
	FName Name;
	bool bIsActive = false;
	float TargetMeasurement = 100.0f;
	float ActualMeasurement = 100.0f;
	float MinMeasurement = 0.0f;
	float MaxMeasurement = 200.0f;
};

using FMetaHumanCharacterBodyConstraintItemPtr = TSharedPtr<FMetaHumanCharacterBodyConstraintItem>;


UCLASS()
class UMetaHumanCharacterParametricBodyProperties : public UMetaHumanCharacterBodyModelSubToolBase, public FMetaHumanCharacterClothVisibilityBase
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface
	
	bool IsFixedBodyType() const;

	void OnBeginConstraintEditing();
	void OnConstraintItemsChanged(bool bInCommitChange);
	void ResetConstraints();
	void PerformParametricFit();

	TArray<FMetaHumanCharacterBodyConstraintItemPtr> GetConstraintItems(const TArray<FName>& ConstraintNames);
	void OnBodyStateChanged();
	void UpdateMeasurements();

	UPROPERTY()
	bool bShowMeasurements = true;

	TArray<FMetaHumanCharacterBodyConstraintItemPtr> BodyConstraintItems;
	TArray<TArray<FVector>> ActiveContours;
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> PreviousBodyState;
};

UENUM()
enum class EMetaHumanCharacterFixedBodyToolHeight : uint8
{
	Short,
	Average,
	Tall
};

UCLASS()
class UMetaHumanCharacterFixedCompatibilityBodyProperties : public UMetaHumanCharacterBodyModelSubToolBase
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, Category = "Body")
	EMetaHumanCharacterFixedBodyToolHeight Height = EMetaHumanCharacterFixedBodyToolHeight::Average;

	UPROPERTY(EditAnywhere, Category = "Body")
	EMetaHumanBodyType MetaHumanBodyType = EMetaHumanBodyType::BlendableBody;

	int32 GetHeightIndex() const { return static_cast<int32>(Height); }
	void UpdateHeightFromBodyType();

	void OnBodyStateChanged();
	void OnMetaHumanBodyTypeChanged();
};

UCLASS()
class UMetaHumanCharacterEditorBodyParameterProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface

	void OnPostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent);

	FSimpleMulticastDelegate OnBodyParameterChangedDelegate;

	void OnBodyStateChanged();
	void ResetBody();

	/** Scale of vertex and joint delta represented by the body model */
	UPROPERTY(EditAnywhere, Category = "Body Parameters", meta = (UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "1.0"))
	float GlobalDelta = 1.0f;

	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> PreviousBodyState;
};

UCLASS()
class UMetaHumanCharacterEditorBodyModelTool : public UMetaHumanCharacterEditorToolWithSubTools
{
	GENERATED_BODY()

public:
	//~Begin UInteractiveTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	//~End UInteractiveTool interface

	void SetEnabledSubTool(UMetaHumanCharacterBodyModelSubToolBase* InSubTool, bool bInEnabled);

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterParametricBodyProperties> ParametricBodyProperties;
	
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterFixedCompatibilityBodyProperties> FixedCompatibilityBodyProperties;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorBodyParameterProperties> BodyParameterProperties;
};


UCLASS()
class UBodyStateChangeTransactor : public UObject, public IMeshStateChangeTransactorInterface, public FMetaHumanCharacterClothVisibilityBase
{
	GENERATED_BODY()

public:
	virtual FSimpleMulticastDelegate& GetStateChangedDelegate(UMetaHumanCharacter* InMetaHumanCharacter) override;

	virtual void CommitShutdownState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, EToolShutdownType InShutdownType, const FText& InCommandChangeDescription) override;
	
	virtual void StoreBeginDragState(UMetaHumanCharacter* InMetaHumanCharacter) override;
	virtual void CommitEndDragState(UInteractiveToolManager* InToolManager, UMetaHumanCharacter* InMetaHumanCharacter, const FText& InCommandChangeDescription) override;

	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> GetBeginDragState() const;
	
protected:

	// Hold the state of the character when a dragging operation begins so it can be undone while the tool is active
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BeginDragState;
};

UCLASS()
class UMetaHumanCharacterEditorBodyBlendToolProperties : public UMetaHumanCharacterEditorMeshBlendToolProperties
{
	GENERATED_BODY()

public:
	bool IsFixedBodyType() const;

	void PerformParametricFit() const;

	/** Blend shape, skeleton, or both */
	UPROPERTY(EditAnywhere, Category = "BlendTool")
	EBodyBlendOptions BlendOptions = EBodyBlendOptions::Both;
};

UCLASS()
class UMetaHumanCharacterEditorBodyBlendTool : public UMetaHumanCharacterEditorMeshBlendTool
{
	GENERATED_BODY()

public:
	//~Begin UMetaHumanCharacterEditorMeshBlendTool interface
	virtual void Setup() override;
	virtual void AddMetaHumanCharacterPreset(class UMetaHumanCharacter* InCharacterPreset, int32 InItemIndex) override;
	virtual void RemoveMetaHumanCharacterPreset(int32 InItemIndex) override;
	virtual void BlendToMetaHumanCharacterPreset(UMetaHumanCharacter* InCharacterPreset) override;
	//~End UMetaHumanCharacterEditorMeshBlendTool interface

	UMetaHumanCharacterEditorBodyParameterProperties* GetBodyParameterProperties() const { return BodyParameterProperties; }

protected:
	//~Begin UMetaHumanCharacterEditorMeshEditingTool interface
	virtual void InitStateChangeTransactor() override;
	virtual const FText GetDescription() const override;
	virtual const FText GetCommandChangeDescription() const override;
	virtual const FText GetCommandChangeIntermediateDescription() const override;
	virtual float GetManipulatorScale() const override;
	virtual TArray<FVector3f> GetManipulatorPositions() const override;
	//~End UMetaHumanCharacterEditorMeshEditingTool interface

	//~Begin UMetaHumanCharacterEditorMeshBlendTool interface
	virtual TArray<FVector3f> BlendPresets(int32 InManipulatorIndex, const TArray<float>& Weights) override;
	virtual float GetAncestryCircleRadius() const override;
	//~End UMetaHumanCharacterEditorMeshBlendTool interface

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorBodyParameterProperties> BodyParameterProperties;

private:
	/** Holds the face states of the presets. */
	TArray<TSharedPtr<const FMetaHumanCharacterBodyIdentity::FState>> PresetStates;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorSubTools.h"
#include "MetaHumanCharacterIdentity.h"
#include "MetaHumanCharacter.h"

#include "MetaHumanCharacterEditorHeadModelTool.generated.h"

UENUM()
enum class EMetaHumanCharacterHeadModelTool : uint8
{
	Model,
	Materials,
	Grooms,	
};

UCLASS()
class UMetaHumanCharacterEditorHeadModelToolBuilder : public UMetaHumanCharacterEditorToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

	UPROPERTY()
	EMetaHumanCharacterHeadModelTool ToolType = EMetaHumanCharacterHeadModelTool::Model;

protected:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};

UCLASS(Abstract)
class UMetaHumanCharacterHeadModelSubToolBase : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	/** Utility functions for copying to & from MetaHuman Character Head Model Settings and Head Model Tool Properties */
	virtual void CopyTo(FMetaHumanCharacterHeadModelSettings& OutHeadModelSettings) PURE_VIRTUAL(UMetaHumanCharacterHeadModelSubToolBase::CopyTo(), {});
	virtual void CopyFrom(const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings) PURE_VIRTUAL(UMetaHumanCharacterHeadModelSubToolBase::CopyFrom(), {});

	virtual void SetEnabled(bool bInIsEnabled) {};
};

UCLASS()
class UMetaHumanCharacterHeadModelEyelashesProperties : public UMetaHumanCharacterHeadModelSubToolBase
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface

	//~Begin UMetaHumanCharacterHeadModelSubToolBase interface
	virtual void CopyTo(FMetaHumanCharacterHeadModelSettings& OutHeadModelSettings) override;
	virtual void CopyFrom(const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings) override;
	//~End UMetaHumanCharacterHeadModelSubToolBase interface

	/**
	* Delegate that executes on EPropertyChangeType::ValueSet property change event, i.e. when a property
	* value has finished being updated
	*/
	DECLARE_DELEGATE_OneParam(FOnEyelashesPropertyValueSetDelegate, bool);
	FOnEyelashesPropertyValueSetDelegate OnEyelashesPropertyValueSetDelegate;

public:
	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyelashesProperties Eyelashes;
};

UENUM()
enum class EMetaHumanCharacterTeethPropertyType : uint8
{
	ToothLength,
	ToothSpacing,
	UpperShift,
	LowerShift,
	Overbite,
	Overjet,
	WornDown,
	Polycanine,
	RecedingGums,
	Narrowness
};

UCLASS()
class UMetaHumanCharacterHeadModelTeethProperties : public UMetaHumanCharacterHeadModelSubToolBase
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface

	//~Begin UMetaHumanCharacterHeadModelSubToolBase interface
	virtual void CopyTo(FMetaHumanCharacterHeadModelSettings& OutHeadModelSettings) override;
	virtual void CopyFrom(const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings) override;
	//~End UMetaHumanCharacterHeadModelSubToolBase interface

	virtual void SetEnabled(bool bInIsEnabled) override;


	/**
	* Delegate that executes on EPropertyChangeType::ValueSet property change event, i.e. when a property
	* value has finished being updated
	*/
	DECLARE_DELEGATE_OneParam(FOnTeethPropertyValueSetDelegate, bool);
	FOnTeethPropertyValueSetDelegate OnTeethPropertyValueSetDelegate;

public:
	UPROPERTY(EditAnywhere, Category = "Teeth")
	EMetaHumanCharacterTeethPropertyType EditableProperty = EMetaHumanCharacterTeethPropertyType::ToothLength;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterTeethProperties Teeth;
};

UCLASS()
class UMetaHumanCharacterEditorHeadModelTool : public UMetaHumanCharacterEditorToolWithSubTools
{
	GENERATED_BODY()

public:
	//~Begin UMetaHumanCharacterEditorToolWithSubTools interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	virtual void OnTick(float InDeltaTime);
	//~End UMetaHumanCharacterEditorToolWithSubTools interface

	void SetEnabledSubTool(UMetaHumanCharacterHeadModelSubToolBase* InSubTool, bool bInEnabled);

protected:

	virtual void RegisterSubTools();
	virtual const FText GetDescription() const;

	/** Properties of the Head Model Tool. These are displayed in the details panel when the tool is activated. */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterHeadModelEyelashesProperties> EyelashesProperties;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterHeadModelTeethProperties> TeethProperties;

private:
	friend class FMetaHumanCharacterEditorHeadModelToolCommandChange;

	/** Keep track of previously set head model settings */
	FMetaHumanCharacterHeadModelSettings PreviousHeadModelSettings;
	FMetaHumanCharacterHeadModelSettings OriginalHeadModelSettings;

	/** Keep track of whether the tool applied any changes */
	bool bEyelashesVariantWasModified = false;
	bool bTeethVariantWasModified = false;
	bool bTeethVariantWasCommitted = false;

	/** 
	* The face state of the actor when the tool was activated
	* This is needed because Eyelashes and Teeth Type changes face geometry through FaceState.
	*/
	TSharedPtr<FMetaHumanCharacterIdentity::FState> FaceState;

	void UpdateHeadModelState(bool bInCommitChange) const;
	void ProcessPending();
};

UCLASS()
class UMetaHumanCharacterEditorHeadMaterialsTool : public UMetaHumanCharacterEditorHeadModelTool
{
	GENERATED_BODY()

public:
	//~Begin UMetaHumanCharacterEditorToolWithSubTools interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	virtual void OnTick(float InDeltaTime);
	//~End	 UMetaHumanCharacterEditorToolWithSubTools interface

protected:
	virtual void RegisterSubTools() override;
	virtual const FText GetDescription() const override;

};

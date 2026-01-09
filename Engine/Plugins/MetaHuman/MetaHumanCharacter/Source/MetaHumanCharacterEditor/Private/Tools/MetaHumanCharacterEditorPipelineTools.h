// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/SingleTargetWithSelectionTool.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "MetaHumanCharacterPipeline.h"
#include "MetaHumanTypes.h"
#include "Misc/Optional.h"
#include "Subsystem/MetaHumanCharacterBuild.h"
#include "DCC/MetaHumanCharacterDCCExport.h"

#include "MetaHumanCharacterEditorPipelineTools.generated.h"

class UMetaHumanCharacter;
class UMetaHumanCollection;

UENUM()
enum class EMetaHumanCharacterPipelineEditingTool : uint8
{
	Pipeline
};

UCLASS()
class UMetaHumanCharacterEditorPipelineToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual bool CanBuildTool(const FToolBuilderState& InSceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

	UPROPERTY()
	EMetaHumanCharacterPipelineEditingTool ToolType = EMetaHumanCharacterPipelineEditingTool::Pipeline;

protected:
	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};

// Default pipelines for selection in the tool, should be in sync with the pipelines in UMetaHumanCharacterPaletteProjectSettings
UENUM()
enum class EMetaHumanDefaultPipelineType : uint8
{
	Cinematic UMETA(DisplayName = "UE Cine (Complete)"),
	Optimized UMETA(DisplayName = "UE Optimized"),
	UEFN UMETA(DisplayName = "UEFN Export"),
	DCC UMETA(DisplayName = "DCC Export")
};

/**
 * Properties used to customize the pipeline UI and generate the parameters for a pipeline assembly
 * NOTE: this is transient and will reset when the tool closes, it is a temporary solution until we find a better solution
 */
UCLASS(Transient)
class UMetaHumanCharacterEditorPipelineToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject Interface

public:

	/** Selected type of pipeline to run the assembly */
	UPROPERTY(EditAnywhere, Category = "Assembly Selection", meta = (DisplayName = "Assembly"))
	EMetaHumanDefaultPipelineType PipelineType = EMetaHumanDefaultPipelineType::Cinematic;

	/** Quality setting for the pipeline */
	UPROPERTY(EditAnywhere, Category = "Assembly Selection", meta = (DisplayName = "Quality", InvalidEnumValues = "Cinematic", EditConditionHides, EditCondition = "PipelineType == EMetaHumanDefaultPipelineType::Optimized || PipelineType == EMetaHumanDefaultPipelineType::UEFN"))
	EMetaHumanQualityLevel PipelineQuality = EMetaHumanQualityLevel::High;

	/** Path to the Root directory where the assembled assets will be placed so that the final structure is <RootDirectory>/<Name> */
	UPROPERTY(EditAnywhere, Category = "Targets", meta = (ContentDir, PipelineDisplay = "Targets", EditConditionHides, EditCondition = "(PipelineType == EMetaHumanDefaultPipelineType::Cinematic || PipelineType == EMetaHumanDefaultPipelineType::Optimized)"))
	FDirectoryPath RootDirectory;

	/** Path to a project directory where assets shared by assembled MetaHumans are place. If referenced assets are missing, they will be populated as needed. */
	UPROPERTY(EditAnywhere, Category = "Advanced Options", meta = (ContentDir, PipelineDisplay = "Targets", EditConditionHides, EditCondition = "(PipelineType == EMetaHumanDefaultPipelineType::Cinematic || PipelineType == EMetaHumanDefaultPipelineType::Optimized)"))
	FDirectoryPath CommonDirectory{ TEXT("/Game/MetaHumans/Common") };

	/** Character name to use for the generated assets. */
	UPROPERTY(EditAnywhere, Category = "Targets", meta = (DisplayName = "Name", EditConditionHides, EditCondition = "(PipelineType == EMetaHumanDefaultPipelineType::Cinematic || PipelineType == EMetaHumanDefaultPipelineType::Optimized)"))
	FString NameOverride;

	/** Folder path for the generated zip archive with the assets packaged for DCC tools */
	UPROPERTY(EditAnywhere, Category = "Targets", meta = (PipelineDisplay = "Targets", EditConditionHides, EditCondition = "(PipelineType == EMetaHumanDefaultPipelineType::DCC)"))
	FDirectoryPath OutputFolder;

	/** Whether or not to bake the makeup into the generated face textures */
	UPROPERTY(EditAnywhere, Category = "Targets", meta = (PipelineDisplay = "Targets", EditConditionHides, EditCondition = "PipelineType == EMetaHumanDefaultPipelineType::DCC"))
	bool bBakeMakeup = true;

	/** Whether or not to export files in ZIP archive */
	UPROPERTY(EditAnywhere, Category = "Targets", meta = (PipelineDisplay = "Targets", EditConditionHides, EditCondition = "PipelineType == EMetaHumanDefaultPipelineType::DCC"))
	bool bExportZipFile = false;

	/** Optional name for the output archive, if empty the character asset name will be used */
	UPROPERTY(EditAnywhere, Category = "Targets", meta = (PipelineDisplay = "Targets", EditConditionHides, 
		EditCondition = "(PipelineType == EMetaHumanDefaultPipelineType::DCC && bExportZipFile)"))
	FString ArchiveName;

	// Trigger when either PipelineType or Quality are modified
	FSimpleDelegate OnPipelineSelectionChanged;

	// Returns the class of the currently selected pipeline
	[[nodiscard]] TSoftClassPtr<class UMetaHumanCollectionPipeline> GetSelectedPipelineClass() const;

	// Helpers to get an UObject pointer to the instance of the currently selected pipeline from the character data
	[[nodiscard]] TObjectPtr<class UMetaHumanCollectionPipeline> GetSelectedPipeline() const;
	[[nodiscard]] TObjectPtr<class UMetaHumanCharacterEditorPipeline> GetSelectedEditorPipeline() const;
	
	// Updates the character data with the selected pipeline
	void UpdateSelectedPipeline();

	// Generates the Build params to set in the tool for passing to FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter()
	[[nodiscard]] FMetaHumanCharacterEditorBuildParameters InitParametersForCollectionPipeline() const;
	[[nodiscard]] FMetaHumanCharacterEditorDCCExportParameters InitParametersForDCCPipeline() const;
};

/**
 * Tool for manipulating the build pipeline.
 */
UCLASS()
class UMetaHumanCharacterEditorPipelineTool : public USingleTargetWithSelectionTool
{
	GENERATED_BODY()

public:
	//~Begin UInteractiveTool interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	
	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }
	//~End UInteractiveTool interface

	UMetaHumanCharacterEditorPipelineToolProperties* GetPipelineProperty() const { return PropertyObject; }

	bool CanBuild(FText& OutErrorMsg) const;
	void Build() const;

protected:

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorPipelineToolProperties> PropertyObject;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorConformTool.h"
#include "MetaHumanCharacterEditorSubTools.h"
#include "MetaHumanCharacterEditorSubsystem.h"

#include "MetaHumanCharacterEditorBodyConformTool.generated.h"

UCLASS()
class UMetaHumanCharacterEditorBodyConformToolBuilder : public UMetaHumanCharacterEditorToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& InSceneState) const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface

protected:

	//~Begin UInteractiveToolWithToolTargetsBuilder interface
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	//~End UInteractiveToolWithToolTargetsBuilder interface
};


UCLASS()
class UMetaHumanCharacterImportBodyDNAProperties : public UMetaHumanCharacterImportSubToolBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "File", meta = (FilePathFilter = "DNA file (*.dna)|*.dna"))
	FFilePath DNAFile;

	UPROPERTY(EditAnywhere, Category = "Import DNA Options", meta = (ShowOnlyInnerProperties))
	FImportBodyFromDNAParams ImportOptions;

public:

	//~Begin UMetaHumanCharacterImportSubToolBase interface
	virtual bool CanImport() const override;
	virtual void Import() override;
	//~End UMetaHumanCharacterImportSubToolBase interface
};

UCLASS()
class UMetaHumanCharacterImportBodyTemplateProperties : public UMetaHumanCharacterImportSubToolBase
{
	GENERATED_BODY()

public:

	// Static/skeletal mesh used as source for mesh/skeleton. Must be body only, using MetaHuman topology.
	UPROPERTY(EditAnywhere, Category = "Asset", meta = (AllowedClasses = "/Script/Engine.StaticMesh, /Script/Engine.SkeletalMesh"))
	TSoftObjectPtr<UObject> Mesh;

	// Provides options for constructing the skeleton
	UPROPERTY(EditAnywhere, Category = "Import Template Options")
	EMetaHumanCharacterBodyFitOptions BodyFitOptions = EMetaHumanCharacterBodyFitOptions::FitFromMeshAndSkeleton;

public:

	//~Begin UMetaHumanCharacterImportTemplateProperties interface
	virtual bool CanImport() const override;
	virtual void Import() override;
	//~End UMetaHumanCharacterImportTemplateProperties interface
};

UCLASS()
class UMetaHumanCharacterEditorBodyConformTool : public UMetaHumanCharacterEditorToolWithSubTools
{
	GENERATED_BODY()

public:

	//~Begin UMetaHumanCharacterEditorToolWithSubTools interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	//~End UMetaHumanCharacterEditorToolWithSubTools interface

	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> GetOriginalState() const;
	const TArray<uint8>& GetOriginalDNABuffer() const;
	void UpdateOriginalState();
	void UpdateOriginalDNABuffer();

private:

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterImportBodyDNAProperties> ImportDNAProperties;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterImportBodyTemplateProperties> ImportTemplateProperties;

	// Hold the original state of the character, used to undo changes on cancel
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> OriginalState;

	// Hold the original DNA buffer of the character, used to undo changes on cancel
	TArray<uint8> OriginalDNABuffer;
};
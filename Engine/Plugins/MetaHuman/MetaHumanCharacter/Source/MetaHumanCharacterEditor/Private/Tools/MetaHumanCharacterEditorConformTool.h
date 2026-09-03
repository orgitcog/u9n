// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "MetaHumanCharacterEditorSubTools.h"
#include "MetaHumanCharacterEditorSubsystem.h"

#include "MetaHumanCharacterEditorConformTool.generated.h"

UCLASS()
class UMetaHumanCharacterEditorConformToolBuilder : public UMetaHumanCharacterEditorToolWithToolTargetsBuilder
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

UCLASS(Abstract)
class UMetaHumanCharacterImportSubToolBase : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	/** Returns true if all the conditions for the import operation to happen are valid */
	virtual bool CanImport() const PURE_VIRTUAL(UMetaHumanCharacterImportSubToolBase::CanImport(), { return false; });

	/** Perform the import operation */
	virtual void Import() PURE_VIRTUAL(UMetaHumanCharacterImportSubToolBase::Import(), {});

	/* Display a conform error message */
	virtual void DisplayConformError(const FText& ErrorMessageText) const;
};

UCLASS()
class UMetaHumanCharacterImportDNAProperties : public UMetaHumanCharacterImportSubToolBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "File", meta = (FilePathFilter = "DNA file (*.dna)|*.dna"))
	FFilePath DNAFile;

	UPROPERTY(EditAnywhere, Category = "Import DNA Options", meta = (ShowOnlyInnerProperties))
	FImportFromDNAParams ImportOptions;

public:

	//~Begin UMetaHumanCharacterImportDNAProperties interface
	virtual bool CanImport() const override;
	virtual void Import() override;
	//~End UMetaHumanCharacterImportDNAProperties interface
};

UCLASS()
class UMetaHumanCharacterImportIdentityProperties : public UMetaHumanCharacterImportSubToolBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Asset")
	TSoftObjectPtr<class UMetaHumanIdentity> MetaHumanIdentity;

	UPROPERTY(EditAnywhere, Category = "Import Identity Options", meta = (ShowOnlyInnerProperties))
	FImportFromIdentityParams ImportOptions;

public:

	//~Begin UMetaHumanCharacterImportDNAProperties interface
	virtual bool CanImport() const override;
	virtual void Import() override;
	//~End UMetaHumanCharacterImportDNAProperties interface
};

UCLASS()
class UMetaHumanCharacterImportTemplateProperties : public UMetaHumanCharacterImportSubToolBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Asset", meta = (AllowedClasses = "/Script/Engine.StaticMesh, /Script/Engine.SkeletalMesh"))
	TSoftObjectPtr<UObject> Mesh;

	UPROPERTY(EditAnywhere, Category = "Asset", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UObject> LeftEyeMesh;

	UPROPERTY(EditAnywhere, Category = "Asset", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UObject> RightEyeMesh;

	UPROPERTY(EditAnywhere, Category = "Asset", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UObject> TeethMesh;

	UPROPERTY(EditAnywhere, Category = "Import Template Options", meta = (ShowOnlyInnerProperties))
	FImportFromTemplateParams ImportOptions;

public:

	//~Begin UMetaHumanCharacterImportTemplateProperties interface
	virtual bool CanImport() const override;
	virtual void Import() override;
	//~End UMetaHumanCharacterImportTemplateProperties interface
};

UCLASS()
class UMetaHumanCharacterEditorConformTool : public UMetaHumanCharacterEditorToolWithSubTools
{
	GENERATED_BODY()

public:

	//~Begin UMetaHumanCharacterEditorToolWithSubTools interface
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType InShutdownType) override;
	//~End UMetaHumanCharacterEditorToolWithSubTools interface

	void UpdateOriginalState();
	void UpdateOriginalDNABuffer();

	TSharedRef<const FMetaHumanCharacterIdentity::FState> GetOriginalState() const;
	const TArray<uint8>& GetOriginalDNABuffer() const;

private:

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterImportDNAProperties> ImportDNAProperties;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterImportIdentityProperties> ImportIdentityProperties;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterImportTemplateProperties> ImportTemplateProperties;

	// Hold the original state of the character, used to undo changes on cancel
	TSharedPtr<FMetaHumanCharacterIdentity::FState> OriginalState;

	// Hold the original DNA buffer of the character, used to undo changes on cancel
	TArray<uint8> OriginalDNABuffer;

};
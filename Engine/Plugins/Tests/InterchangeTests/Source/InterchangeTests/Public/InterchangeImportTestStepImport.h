// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "InterchangeImportTestPlan.h"
#include "InterchangeImportTestStepBase.h"
#include "InterchangeTestPlanPipelineSettings.h"
#include "UObject/StrongObjectPtr.h"

#include "InterchangeImportTestStepImport.generated.h"

class UInterchangeTranslatorBase;
class UInterchangePipelineBase;
class UInterchangeBaseNodeContainer;
class UWorld;

class UInterchangeImportTestStepImport;

enum class EImportStepDataChangeType : uint8
{
	Unknown,
	SourceFile,
	PipelineSettings,
	ImportIntoLevel,
};

struct FImportStepChangedData
{
public:
	EImportStepDataChangeType ChangeType;

	UInterchangeImportTestStepImport* ImportStep = nullptr;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnImportTestStepDataChanged, FImportStepChangedData)


UCLASS(BlueprintType, Meta = (DisplayName = "Import a file"))
class INTERCHANGETESTS_API UInterchangeImportTestStepImport : public UInterchangeImportTestStepBase
{
	GENERATED_BODY()

public:
	UInterchangeImportTestStepImport();

	/** The source file to import (path relative to the json script) */
	UPROPERTY(EditAnywhere, Category = General)
	FFilePath SourceFile;

	/** Whether the import should use the override pipeline stack */
	UPROPERTY(EditAnywhere, Category = General)
	bool bUseOverridePipelineStack = false;

	/** The pipeline stack to use when importing (an empty array will use the defaults) */
	UPROPERTY(EditAnywhere, Instanced, Category = General, meta=(DisplayName="Override Pipeline Stack", EditCondition = "bUseOverridePipelineStack", MaxPropertyDepth = 1))
	TArray<TObjectPtr<UInterchangePipelineBase>> PipelineStack;

	UPROPERTY(EditAnywhere, Category = General)
	FInterchangeTestPlanPipelineSettings PipelineSettings;

	/** Whether the destination folder should be emptied prior to import */
	UPROPERTY(EditAnywhere, Category = General)
	bool bEmptyDestinationFolderPriorToImport = true;

	/**  Whether we should use the import into level workflow */
	UPROPERTY(EditAnywhere, Category = General)
	bool bImportIntoLevel = false;

	/**  Whether screenshot would be taken at this stage. */
	UPROPERTY(EditAnywhere, Category = "Screenshot Comparison", meta=(EditCondition = "bImportIntoLevel"))
	bool bTakeScreenshot = false;

	/**  Screen Shot Settings */
	UPROPERTY(EditAnywhere, Category = "Screenshot Comparison", meta = (EditCondition = "bTakeScreenshot && bImportIntoLevel"))
	FInterchangeTestScreenshotParameters ScreenshotParameters;

	FOnImportTestStepDataChanged OnImportTestStepDataChanged;
public:
	// UInterchangeImportTestStepBase interface
	virtual TTuple<UE::Interchange::FAssetImportResultPtr, UE::Interchange::FSceneImportResultPtr>
		StartStep(FInterchangeImportTestData& Data) override;
	virtual FTestStepResults FinishStep(FInterchangeImportTestData& Data, FAutomationTestBase* CurrentTest) override;
	virtual FString GetContextString() const override;
	virtual bool HasScreenshotTest() const override;
	virtual FInterchangeTestScreenshotParameters GetScreenshotParameters() const override;

	virtual bool CanEditPipelineSettings() const override;
	virtual void EditPipelineSettings() override;
	virtual void ClearPipelineSettings() override;
	virtual bool IsUsingOverridePipelines(bool bCheckForValidPipelines) const override;

	bool ShouldImportIntoLevelChangeRequireMessageBox() const;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	
	TArray<TObjectPtr<UInterchangePipelineBase>> GetCurrentPipelinesOrDefault() const;
private:
	void BroadcastImportStepChangedEvent(EImportStepDataChangeType ChangeType);

private:
	UPROPERTY()
	FString LastSourceFileExtension;

};

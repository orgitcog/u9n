// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Dialogs/Dialogs.h"
#include "UObject/ObjectPtr.h"

#include "InterchangeImportTestPlan.generated.h"

class UInterchangeImportTestStepBase;
class UInterchangeImportTestStepImport;
class UInterchangeImportTestStepReimport;

/**
* Define a test plan
*/
UCLASS(BlueprintType, EditInlineNew, meta=(PrioritizeCategories = "Description Level Definition Import Reimport")) // @TODO: can't find a way to force Run to be below Definition
class INTERCHANGETESTS_API UInterchangeImportTestPlan : public UObject
{
	GENERATED_BODY()

public:

	/** Deserialize a test plan from Json */
	bool ReadFromJson(const FString& Filename);

	/* Serialize a test plan to Json */
	void WriteToJson(const FString& Filename);

public:
	UInterchangeImportTestPlan();

	/** Test Description */
	UPROPERTY(EditAnywhere, Category = "Description", Meta = (MultiLine = true))
	FString Description;

	/** Set of steps to perform to carry out this test plan */
	UPROPERTY(Instanced, meta = (DeprecatedProperty, DeprecationMessage = "Use the import and reimport sections to fill up the test steps."))
	TArray<TObjectPtr<UInterchangeImportTestStepBase>> Steps_DEPRECATED;
	
	/**  Level to use for taking Screenshot */
	UPROPERTY(EditAnywhere, Category = Level, meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath WorldPath;

	/** File Import Step */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import")
	TObjectPtr<UInterchangeImportTestStepImport> ImportStep;

	/** Set of re-imports that will follow the above import step */
	UPROPERTY(EditAnywhere, Instanced, Category = "Reimport", NoClear)
	TArray<TObjectPtr<UInterchangeImportTestStepReimport>> ReimportStack;


public:
	/** Click here to immediately run this single test through the automation framework */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Automation")
	void RunThisTest();	
	bool IsRunningSynchornously() const;

	void SetupLevelForImport();
	
	UWorld* GetCurrentWorld() const
	{	
		return TransientWorld.Get() ? TransientWorld.Get() : GWorld->GetWorld();
	}

	ULevel* GetCurrentLevel() const 
	{
		return GetCurrentWorld()->GetCurrentLevel();
	}

	void CleanupLevel();

	virtual void PostLoad() override;
	void OnAssetLoaded(UObject* Asset);

#if WITH_EDITOR
	static FSuppressableWarningDialog::EMode GetInterchangeTestPlanWarningDialogMode();

	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;

	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
	
private:
	TStrongObjectPtr<UWorld> TransientWorld;

	int32 PrevReimportStackSize = -1;
	bool bRunSynchronously = false;
	bool bChangeObjectOuters = false;
};


namespace UE::Interchange
{
	class INTERCHANGETESTS_API FInterchangeImportTestPlanStaticHelpers
	{
	public:
		static FString GetTestNameFromObjectPathString(const FString& InObjectPathString, bool bAddBeautifiedTestNamePrefix = false);
		static FString GetBeautifiedTestName();
		static FString GetInterchangeImportTestRootGameFolder();
	};
}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorReimportHandler.h"
#include "Factories/Factory.h"

#include "MetaHumanCameraCalibrationImporterFactory.generated.h"

UCLASS()
class METAHUMANCOREEDITOR_API UMetaHumanCameraCalibrationImporterFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UMetaHumanCameraCalibrationImporterFactory(const FObjectInitializer& InObjectInitializer);

protected:

	//~ UFactory Interface
	virtual FText GetToolTip() const override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, const FString& InFileName, const TCHAR* InParams, FFeedbackContext* InWarn, bool& bOutOperationCanceled);

	//~ FReimportHandler Interface
	virtual bool CanReimport(UObject* InObj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* InObj, const TArray<FString>& InNewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* InObj) override;

	virtual TObjectPtr<UObject>* GetFactoryObject() const override;
	mutable TObjectPtr<UObject> GCMark{ this };
};
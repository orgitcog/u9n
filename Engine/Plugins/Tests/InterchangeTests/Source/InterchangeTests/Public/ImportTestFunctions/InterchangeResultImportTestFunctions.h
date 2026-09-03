// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctionsBase.h"
#include "UObject/Package.h"
#include "InterchangeResultImportTestFunctions.generated.h"

class UInterchangeResult;
class UInterchangeResultsContainer;

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API UInterchangeResultImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the specified InterchangeResult was emitted during import */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckIfErrorOrWarningWasGenerated(UInterchangeResultsContainer* ResultsContainer, TSubclassOf<UInterchangeResult> ErrorOrWarningClass);
};

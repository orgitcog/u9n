// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Factories/Factory.h"
#include "GenePool.h"
#include "GenePoolAssetImportFactory.generated.h"


UCLASS()
class GENESPLICEREDITOR_API UGenePoolAssetImportFactory: public UFactory
{ 
	GENERATED_BODY()

public:
	UGenePoolAssetImportFactory();
	/** UFactory interface */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual void CleanUp() override {};
};

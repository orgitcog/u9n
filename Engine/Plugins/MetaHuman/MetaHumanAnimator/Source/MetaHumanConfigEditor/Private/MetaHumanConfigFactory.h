// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "MetaHumanConfigFactory.generated.h"


//////////////////////////////////////////////////////////////////////////
// UMetaHumanConfigFactory

UCLASS(hidecategories=Object)
class UMetaHumanConfigFactory
	: public UFactory
{
	GENERATED_BODY()

public:
	//~ UFactory Interface
	UMetaHumanConfigFactory();

	//~ Begin UFactory Interface
	virtual bool FactoryCanImport(const FString& InFilename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, const FString& InFilename, const TCHAR* InParms, FFeedbackContext* InWarn, bool& bOutOperationCanceled) override;
	//~ End UFactory Interface
};

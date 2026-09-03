// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Factories/Factory.h"
#include "MetaHumanPackageFactory.generated.h"

// Allow import of MetaHuman asset group packages
UCLASS()
class UMetaHumanPackageFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual FText GetToolTip() const override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* ImportObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, bool& OutCanceled) override;
	// End of UFactory interface
};

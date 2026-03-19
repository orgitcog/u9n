// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "PCGComputeSourceFactory.generated.h"

UCLASS()
class UPCGComputeSourceFactory : public UFactory
{
	GENERATED_BODY()

public:
	UPCGComputeSourceFactory();

	//~Begin UFactory interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override { return true; }
	//~End UFactory interface
};

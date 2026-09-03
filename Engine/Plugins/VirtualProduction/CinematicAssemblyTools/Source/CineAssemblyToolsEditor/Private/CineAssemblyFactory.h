// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "CineAssembly.h"

#include "CineAssemblyFactory.generated.h"

/** 
 * Factory class used to create new UCineAssembly objects
 * Before creating a new Cine Assembly, the factory will spawn a new window to configure the properties of the asset that is being created.
 */
UCLASS(hidecategories=Object)
class UCineAssemblyFactory : public UFactory
{
	GENERATED_BODY()

public:
	/** Takes a pre-configured, transient assembly, creates a valid package for it, and initializes it */
	static void CreateConfiguredAssembly(UCineAssembly* ConfiguredAssembly, const FString& CreateAssetPath);

	/** Recursively evaluates the default assembly name and path until a unique combination is found */
	static void MakeUniqueNameAndPath(UCineAssembly* ConfiguredAssembly, const FString& CreateAssetPath, FString& UniquePackageName, FString& UniqueAssetName);

protected:
	UCineAssemblyFactory();

	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override;
	virtual bool ConfigureProperties() override;
	// End UFactory Interface
};

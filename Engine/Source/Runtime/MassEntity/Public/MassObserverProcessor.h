// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassObserverProcessor.generated.h"

#define UE_API MASSENTITY_API


UCLASS(MinimalAPI, abstract)
class UMassObserverProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UE_API UMassObserverProcessor();

protected:
	UE_API virtual void PostInitProperties() override;

	/** 
	 * By default registers this class as Operation observer of ObservedType. Override to register for multiple 
	 * operations and/or types 
	 */
	UE_API virtual void Register();

protected:
	UPROPERTY(EditDefaultsOnly, Category = Processor, config)
	bool bAutoRegisterWithObserverRegistry = true;

	/** Determines which Fragment or Tag type this given UMassObserverProcessor will be observing */
	UPROPERTY()
	TObjectPtr<const UScriptStruct> ObservedType = nullptr;

	EMassObservedOperation Operation = EMassObservedOperation::MAX;
};

#undef UE_API

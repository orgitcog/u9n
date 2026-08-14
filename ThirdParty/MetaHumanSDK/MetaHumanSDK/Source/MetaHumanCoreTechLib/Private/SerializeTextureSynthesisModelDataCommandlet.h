// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "SerializeTextureSynthesisModelDataCommandlet.generated.h"

/**
 *	Serializes and compresses the texture synthesis model data into a UE archive
 */
UCLASS()
class USerializeTextureSynthesisModelDataCommandlet : public UCommandlet
{
	GENERATED_BODY()

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};

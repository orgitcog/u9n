// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralDaySequence.h"

#include "CelestialVaultSequence.generated.h"


/**
 * A procedural sequence that animates a sun in a physically accurate way based on geographic data.
 */
USTRUCT()
struct CELESTIALVAULT_API FCelestialVaultSequence : public FProceduralDaySequence
{
	GENERATED_BODY()

	virtual ~FCelestialVaultSequence() override
	{}

	UPROPERTY(EditAnywhere, Category = "Procedural Parameters")
	unsigned KeyCount = 24;
	
private:
	virtual void BuildSequence(UProceduralDaySequenceBuilder* InBuilder) override;
};

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NeuroChaosIntensityController.generated.h"

UCLASS()
class UNeuroChaosIntensityController : public UObject
{
	GENERATED_BODY()

public:
	UNeuroChaosIntensityController();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void SetChaosLevel(float Level);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	float ChaosLevel;
};

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EchoPyperCuriosityModule.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrigger);

UCLASS()
class UEchoPyperCuriosityModule : public UObject
{
	GENERATED_BODY()

public:
	UEchoPyperCuriosityModule();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Update(float DeltaTime);

	UPROPERTY(BlueprintAssignable, Category = "Neurochemical")
	FOnTrigger OnTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	float TriggerProbability;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	float ChaosFactor;

private:
	float TimeSinceLastTrigger;
};

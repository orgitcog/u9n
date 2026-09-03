#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EchoPyperCuriosityModule.h"
#include "HomeostasisRegulator.generated.h"

UCLASS()
class UHomeostasisRegulator : public UObject
{
	GENERATED_BODY()

public:
	UHomeostasisRegulator();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Regulate(UEchoPyperCuriosityModule* CuriosityModule, EAbundanceState AbundanceState);

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void RespondToScarcity(const FString& ResourceName);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	float DampeningFactor;
};

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EchoPulseResonanceChamber.generated.h"

UCLASS()
class UEchoPulseResonanceChamber : public UObject
{
	GENERATED_BODY()

public:
	UEchoPulseResonanceChamber();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void UpdateResonance(float Signal);

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Reset();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	float ResonanceLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	float IntensityFactor;
};

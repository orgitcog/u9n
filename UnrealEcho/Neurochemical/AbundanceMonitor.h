#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AbundanceMonitor.generated.h"

UENUM(BlueprintType)
enum class EAbundanceState : uint8
{
	Abundant,
	Scarce
};

UCLASS()
class UAbundanceMonitor : public UObject
{
	GENERATED_BODY()

public:
	UAbundanceMonitor();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Update(const TMap<FString, float>& ResourceLevels);

	UFUNCTION(BlueprintPure, Category = "Neurochemical")
	EAbundanceState GetAbundanceState() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	float AbundanceLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	float ScarcityThreshold;
};

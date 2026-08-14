#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ResourceTracker.h"
#include "ScarcityDetector.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScarcityDetected, FString, ResourceName);

UCLASS()
class UScarcityDetector : public UObject
{
	GENERATED_BODY()

public:
	UScarcityDetector();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void CheckForScarcity(const UResourceTracker* ResourceTracker);

	UPROPERTY(BlueprintAssignable, Category = "Neurochemical")
	FOnScarcityDetected OnScarcityDetected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	TMap<FString, float> ScarcityThresholds;
};

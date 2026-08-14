#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DegradationRecoverySystem.generated.h"

UENUM(BlueprintType)
enum class ESystemState : uint8
{
	Stable,
	Unstable,
	Recovering
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRecovery);

UCLASS()
class UDegradationRecoverySystem : public UObject
{
	GENERATED_BODY()

public:
	UDegradationRecoverySystem();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Monitor(float ChaosLevel);

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Recover();

	UPROPERTY(BlueprintAssignable, Category = "Neurochemical")
	FOnRecovery OnRecovery;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	ESystemState CurrentState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	float StabilityThreshold;
};

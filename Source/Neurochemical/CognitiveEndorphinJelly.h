#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CognitiveEndorphinJelly.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpasm);

UCLASS()
class UCognitiveEndorphinJelly : public UObject
{
	GENERATED_BODY()

public:
	UCognitiveEndorphinJelly();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Accumulate(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void Reset();

	UPROPERTY(BlueprintAssignable, Category = "Neurochemical")
	FOnSpasm OnSpasm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	float DopamineLevel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical")
	float SerotoninLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical")
	float SpasmThreshold;

private:
	void Release();
};

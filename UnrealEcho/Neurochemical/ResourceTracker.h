#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ResourceTracker.generated.h"

UCLASS()
class UResourceTracker : public UObject
{
	GENERATED_BODY()

public:
	UResourceTracker();

	UFUNCTION(BlueprintCallable, Category = "Neurochemical")
	void UpdateResource(const FString& Name, float Level);

	UFUNCTION(BlueprintPure, Category = "Neurochemical")
	float GetResourceLevel(const FString& Name) const;

	UFUNCTION(BlueprintPure, Category = "Neurochemical")
	const TMap<FString, float>& GetResourceLevels() const;

private:
	UPROPERTY()
	TMap<FString, float> ResourceLevels;
};

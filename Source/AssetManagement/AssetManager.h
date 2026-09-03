#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AssetManager.generated.h"

UCLASS()
class UAssetManager : public UObject
{
	GENERATED_BODY()

public:
	UAssetManager();

	UFUNCTION(BlueprintCallable, Category = "Asset Management")
	void LoadLive2DModel(const FString& ModelName);

	UFUNCTION(BlueprintCallable, Category = "Asset Management")
	UTexture* GetProfilePicture(const FString& VariantName);

private:
	void LoadAllAssets();

	UPROPERTY()
	TMap<FString, UObject*> Live2DModels;

	UPROPERTY()
	TMap<FString, UTexture*> ProfilePictures;
};

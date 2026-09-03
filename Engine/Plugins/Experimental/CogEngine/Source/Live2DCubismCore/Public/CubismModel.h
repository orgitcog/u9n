// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CubismModel.generated.h"

/**
 * Live2D Cubism Model Data
 * Represents a loaded Live2D model with its parameters and animations
 */
USTRUCT(BlueprintType)
struct FCubismModelData
{
	GENERATED_BODY()

	/** Model identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Live2D")
	FString ModelID;

	/** Model asset path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Live2D")
	FString ModelPath;

	/** Current animation state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Live2D")
	FString CurrentAnimation;

	/** Model parameters (blend shapes, transforms, etc.) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Live2D")
	TMap<FString, float> Parameters;

	FCubismModelData()
		: CurrentAnimation(TEXT("Idle"))
	{
	}
};

/**
 * Live2D Cubism Model Component
 * Component for rendering and animating Live2D characters
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LIVE2DCUBISMCORE_API UCubismModel : public USceneComponent
{
	GENERATED_BODY()

public:	
	UCubismModel();

	/** Model data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Live2D")
	FCubismModelData ModelData;

	/** Model scale */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Live2D")
	float ModelScale;

	/** Enable auto-update */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Live2D")
	bool bAutoUpdate;

	/** Load Live2D model from file */
	UFUNCTION(BlueprintCallable, Category = "Live2D")
	bool LoadModel(const FString& ModelPath);

	/** Set model parameter */
	UFUNCTION(BlueprintCallable, Category = "Live2D")
	void SetParameter(const FString& ParameterName, float Value);

	/** Get model parameter */
	UFUNCTION(BlueprintCallable, Category = "Live2D")
	float GetParameter(const FString& ParameterName) const;

	/** Play animation */
	UFUNCTION(BlueprintCallable, Category = "Live2D")
	void PlayAnimation(const FString& AnimationName);

	/** Update model */
	UFUNCTION(BlueprintCallable, Category = "Live2D")
	void UpdateModel(float DeltaTime);

	/** Reset model to default state */
	UFUNCTION(BlueprintCallable, Category = "Live2D")
	void ResetModel();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Update internal parameters */
	void UpdateParameters(float DeltaTime);

	/** Render model (internal) */
	void RenderModel();

	float AnimationTime;
};

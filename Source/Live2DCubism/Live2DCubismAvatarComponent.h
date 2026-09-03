#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Live2DCubismAvatarComponent.generated.h"

// Forward declarations
class UTexture2D;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API ULive2DCubismAvatarComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULive2DCubismAvatarComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Load a Live2D model from a .model3.json file */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void LoadLive2DModel(const FString& ModelPath);

    /** Update Live2D model parameters */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetParameterValue(const FName& ParameterName, float Value);

    /** Get Live2D model parameter value */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    float GetParameterValue(const FName& ParameterName) const;

private:
    // TODO: Add Live2D Cubism SDK specific members here

    /** The Live2D model asset */
    UPROPERTY(Transient)
    UObject* Live2DModel;

    /** The texture for rendering the Live2D model */
    UPROPERTY(Transient)
    UTexture2D* RenderTarget;

    /** The dynamic material instance for the Live2D model */
    UPROPERTY(Transient)
    UMaterialInstanceDynamic* DynamicMaterial;
};

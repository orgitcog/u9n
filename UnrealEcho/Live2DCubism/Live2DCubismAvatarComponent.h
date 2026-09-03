#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CubismSDKIntegration.h"
#include "Live2DCubismAvatarComponent.generated.h"

// Forward declarations for SDK components
class UCubismModelComponent;
class UCubismMoc3;
class UTexture2D;
class UMaterialInstanceDynamic;

/**
 * Live2D Cubism Avatar Component for Deep Tree Echo
 * Wraps the CubismUE SDK's UCubismModelComponent with DTE-specific functionality.
 * Provides high-level avatar control with super-hot-girl aesthetic and hyper-chaotic behavior support.
 */
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

    // ========================================
    // SDK Model Access
    // ========================================

    /** Get the underlying SDK model component */
    UFUNCTION(BlueprintPure, Category = "Live2D")
    UCubismModelComponent* GetModelComponent() const { return ModelComponent; }

    /** Get the DTE integration wrapper */
    UFUNCTION(BlueprintPure, Category = "Live2D")
    UCubismSDKIntegration* GetSDKIntegration() const { return SDKIntegration; }

    /** Set the model component to wrap (for external initialization) */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetModelComponent(UCubismModelComponent* InModelComponent);

    // ========================================
    // Parameter Control (delegates to SDK)
    // ========================================

    /** Update Live2D model parameters - delegates to SDK */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    void SetParameterValue(const FName& ParameterName, float Value);

    /** Get Live2D model parameter value - delegates to SDK */
    UFUNCTION(BlueprintCallable, Category = "Live2D")
    float GetParameterValue(const FName& ParameterName) const;

    // ========================================
    // Expression & Animation Control
    // ========================================

    /** Set facial expression by name - uses SDK's Expression component */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Expression")
    void SetExpression(const FString& ExpressionName);

    /** Set gaze target position - uses SDK's LookAt component */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Expression")
    void SetGazeTarget(const FVector2D& TargetPosition);

    /** Set body rotation */
    UFUNCTION(BlueprintCallable, Category = "Live2D|Body")
    void SetBodyRotation(const FRotator& Rotation);

    // ========================================
    // DTE-Specific Features
    // ========================================

    /** Apply super-hot-girl aesthetic */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    void ApplySuperHotGirlAesthetic();

    /** Apply hyper-chaotic behavior */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    void ApplyHyperChaoticBehavior(float ChaosFactor);

    /** Apply echo resonance effect based on cognitive state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    void ApplyEchoResonance(float Resonance);

    /** Apply glitch effect during cognitive overload */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    void ApplyGlitchEffect(float Severity);

    // ========================================
    // Configuration
    // ========================================

    /** Enable/disable automatic eye blinking via SDK's EyeBlink component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live2D|Animation")
    bool bEnableAutoBlink = true;

    /** Enable/disable automatic breathing via SDK's HarmonicMotion component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live2D|Animation")
    bool bEnableAutoBreathing = true;

    /** Enable/disable physics simulation via SDK's Physics component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live2D|Animation")
    bool bEnablePhysics = true;

private:
    /** The wrapped CubismModelComponent from SDK */
    UPROPERTY()
    UCubismModelComponent* ModelComponent;

    /** DTE-specific SDK integration wrapper */
    UPROPERTY()
    UCubismSDKIntegration* SDKIntegration;

    /** Accumulated time for animations */
    float AccumulatedTime = 0.0f;
};

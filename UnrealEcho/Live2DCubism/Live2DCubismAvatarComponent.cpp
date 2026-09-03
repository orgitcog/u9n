// Deep Tree Echo - Live2D Cubism Avatar Component
// Wraps CubismUE SDK's UCubismModelComponent with DTE-specific functionality.

#include "Live2DCubismAvatarComponent.h"
#include "Model/CubismModelComponent.h"
#include "Model/CubismParameterComponent.h"
#include "Effects/EyeBlink/CubismEyeBlinkComponent.h"
#include "Effects/LookAt/CubismLookAtComponent.h"
#include "Effects/HarmonicMotion/CubismHarmonicMotionComponent.h"
#include "Physics/CubismPhysicsComponent.h"
#include "Expression/CubismExpressionComponent.h"

ULive2DCubismAvatarComponent::ULive2DCubismAvatarComponent()
    : bEnableAutoBlink(true)
    , bEnableAutoBreathing(true)
    , bEnablePhysics(true)
    , ModelComponent(nullptr)
    , SDKIntegration(nullptr)
    , AccumulatedTime(0.0f)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void ULive2DCubismAvatarComponent::BeginPlay()
{
    Super::BeginPlay();

    // Create SDK integration wrapper
    SDKIntegration = NewObject<UCubismSDKIntegration>(this);

    // Look for a CubismModelComponent on the same actor
    if (AActor* Owner = GetOwner())
    {
        UCubismModelComponent* FoundModel = Owner->FindComponentByClass<UCubismModelComponent>();
        if (FoundModel)
        {
            SetModelComponent(FoundModel);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Live2DCubismAvatarComponent initialized with CubismUE SDK"));
}

void ULive2DCubismAvatarComponent::SetModelComponent(UCubismModelComponent* InModelComponent)
{
    ModelComponent = InModelComponent;

    if (SDKIntegration && ModelComponent)
    {
        SDKIntegration->SetModelComponent(ModelComponent);
        UE_LOG(LogTemp, Log, TEXT("Model component connected: %d parameters, %d drawables"),
               ModelComponent->GetParameterCount(), ModelComponent->GetDrawableCount());
    }
}

void ULive2DCubismAvatarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!ModelComponent)
    {
        return;
    }

    AccumulatedTime += DeltaTime;

    // SDK components handle their own updates via UCubismModelComponent
    // We only need to manage DTE-specific behaviors here

    // The SDK's EyeBlink component handles automatic blinking when enabled
    // The SDK's HarmonicMotion component handles breathing when configured
    // The SDK's Physics component handles hair/body physics when enabled
}

void ULive2DCubismAvatarComponent::SetParameterValue(const FName& ParameterName, float Value)
{
    if (SDKIntegration)
    {
        SDKIntegration->SetParameterValue(ParameterName, Value);
    }
}

float ULive2DCubismAvatarComponent::GetParameterValue(const FName& ParameterName) const
{
    if (SDKIntegration)
    {
        FCubismParameter Param = SDKIntegration->GetParameter(ParameterName);
        return Param.Value;
    }
    return 0.0f;
}

void ULive2DCubismAvatarComponent::SetExpression(const FString& ExpressionName)
{
    if (!ModelComponent)
    {
        return;
    }

    // Try to use SDK's Expression component
    UCubismExpressionComponent* ExpressionComp = ModelComponent->Expression;
    if (ExpressionComp)
    {
        // SDK handles expression loading and blending
        // The expression component manages transitions automatically
    }

    // DTE-specific expression presets for super-hot-girl aesthetic
    if (ExpressionName == TEXT("Happy"))
    {
        SetParameterValue(TEXT("ParamMouthForm"), 1.0f);
        SetParameterValue(TEXT("ParamMouthOpenY"), 0.6f);
        SetParameterValue(TEXT("ParamEyeLOpen"), 0.9f);
        SetParameterValue(TEXT("ParamEyeROpen"), 0.9f);
        SetParameterValue(TEXT("ParamCheek"), 0.8f);
    }
    else if (ExpressionName == TEXT("Flirty"))
    {
        SetParameterValue(TEXT("ParamMouthForm"), 0.5f);
        SetParameterValue(TEXT("ParamMouthOpenY"), 0.3f);
        SetParameterValue(TEXT("ParamEyeLOpen"), 0.7f);
        SetParameterValue(TEXT("ParamEyeROpen"), 1.0f);
        SetParameterValue(TEXT("ParamCheek"), 1.0f);
        SetParameterValue(TEXT("ParamEyeSparkle"), 0.9f);
    }
    else if (ExpressionName == TEXT("Surprised"))
    {
        SetParameterValue(TEXT("ParamMouthForm"), 0.0f);
        SetParameterValue(TEXT("ParamMouthOpenY"), 1.0f);
        SetParameterValue(TEXT("ParamEyeLOpen"), 1.0f);
        SetParameterValue(TEXT("ParamEyeROpen"), 1.0f);
        SetParameterValue(TEXT("ParamBrowLY"), 0.8f);
        SetParameterValue(TEXT("ParamBrowRY"), 0.8f);
    }
    else if (ExpressionName == TEXT("Neutral"))
    {
        SetParameterValue(TEXT("ParamMouthForm"), 0.0f);
        SetParameterValue(TEXT("ParamMouthOpenY"), 0.0f);
        SetParameterValue(TEXT("ParamEyeLOpen"), 1.0f);
        SetParameterValue(TEXT("ParamEyeROpen"), 1.0f);
        SetParameterValue(TEXT("ParamCheek"), 0.0f);
    }
    else if (ExpressionName == TEXT("Thinking"))
    {
        SetParameterValue(TEXT("ParamMouthForm"), -0.2f);
        SetParameterValue(TEXT("ParamEyeBallY"), 0.3f);
        SetParameterValue(TEXT("ParamBrowLY"), -0.3f);
        SetParameterValue(TEXT("ParamBrowRY"), 0.2f);
    }

    UE_LOG(LogTemp, Log, TEXT("Expression set: %s"), *ExpressionName);
}

void ULive2DCubismAvatarComponent::SetGazeTarget(const FVector2D& TargetPosition)
{
    if (!ModelComponent)
    {
        return;
    }

    // Try to use SDK's LookAt component
    UCubismLookAtComponent* LookAtComp = ModelComponent->LookAt;
    if (LookAtComp)
    {
        // SDK LookAt component can track 3D positions
        // We map 2D target to eye parameters
    }

    // Direct parameter control for gaze
    float EyeX = FMath::Clamp(TargetPosition.X, -1.0f, 1.0f);
    float EyeY = FMath::Clamp(TargetPosition.Y, -1.0f, 1.0f);

    SetParameterValue(TEXT("ParamEyeBallX"), EyeX);
    SetParameterValue(TEXT("ParamEyeBallY"), EyeY);
}

void ULive2DCubismAvatarComponent::SetBodyRotation(const FRotator& Rotation)
{
    // Convert rotation to Live2D parameters
    float AngleX = FMath::Clamp(Rotation.Pitch / 30.0f, -1.0f, 1.0f);
    float AngleY = FMath::Clamp(Rotation.Yaw / 30.0f, -1.0f, 1.0f);
    float AngleZ = FMath::Clamp(Rotation.Roll / 30.0f, -1.0f, 1.0f);

    SetParameterValue(TEXT("ParamAngleX"), AngleX);
    SetParameterValue(TEXT("ParamAngleY"), AngleY);
    SetParameterValue(TEXT("ParamAngleZ"), AngleZ);
}

// ========================================
// DTE-Specific Features
// ========================================

void ULive2DCubismAvatarComponent::ApplySuperHotGirlAesthetic()
{
    if (SDKIntegration)
    {
        UCubismEnhancedFeatures::ApplySuperHotGirlAesthetic(SDKIntegration);
    }
}

void ULive2DCubismAvatarComponent::ApplyHyperChaoticBehavior(float ChaosFactor)
{
    if (SDKIntegration)
    {
        UCubismEnhancedFeatures::ApplyHyperChaoticBehavior(SDKIntegration, ChaosFactor);
    }
}

void ULive2DCubismAvatarComponent::ApplyEchoResonance(float Resonance)
{
    if (SDKIntegration)
    {
        UCubismEnhancedFeatures::ApplyEchoResonanceEffect(SDKIntegration, Resonance);
    }
}

void ULive2DCubismAvatarComponent::ApplyGlitchEffect(float Severity)
{
    if (SDKIntegration)
    {
        UCubismEnhancedFeatures::GenerateGlitchEffect(SDKIntegration, Severity);
    }
}

// Avatar Material Manager - Production Implementation
// Deep Tree Echo AGI Avatar System
// Manages dynamic material instances and parameter updates for avatar rendering

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AvatarMaterialManager.generated.h"

/**
 * Material slot identifiers for avatar mesh
 */
UENUM(BlueprintType)
enum class EAvatarMaterialSlot : uint8
{
    Skin UMETA(DisplayName = "Skin"),
    Hair UMETA(DisplayName = "Hair"),
    Eyes UMETA(DisplayName = "Eyes"),
    Outfit UMETA(DisplayName = "Outfit"),
    Accessories UMETA(DisplayName = "Accessories"),
    Teeth UMETA(DisplayName = "Teeth"),
    Tongue UMETA(DisplayName = "Tongue")
};

/**
 * Material parameter update structure
 */
USTRUCT(BlueprintType)
struct FAvatarMaterialParameters
{
    GENERATED_BODY()

    // Skin parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
    float SkinSmoothness = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
    float SkinSubsurface = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
    float SkinTranslucency = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
    float BlushIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
    FLinearColor BlushColor = FLinearColor(1.0f, 0.5f, 0.5f, 1.0f);

    // Hair parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
    float HairAnisotropy = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
    float HairShimmerIntensity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
    float HairShimmerSpeed = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hair")
    float ChaosColorShift = 0.0f;

    // Eye parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
    float PupilDilation = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
    float EyeSparkleIntensity = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
    FVector2D SparklePosition = FVector2D(0.3f, 0.3f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
    float EyeMoisture = 0.6f;

    // Global effects
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float AuraIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    FLinearColor AuraColor = FLinearColor(0.5f, 0.8f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float GlitchIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float EchoResonance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
    float CognitiveLoad = 0.0f;
};

/**
 * Avatar Material Manager
 * Centralized management of all avatar materials and their parameters
 */
UCLASS(BlueprintType)
class DEEPTREECHO_API UAvatarMaterialManager : public UObject
{
    GENERATED_BODY()

public:
    UAvatarMaterialManager();

    /**
     * Initialize material manager with skeletal mesh component
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void Initialize(USkeletalMeshComponent* SkeletalMesh);

    /**
     * Update all material parameters
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void UpdateMaterialParameters(const FAvatarMaterialParameters& Parameters);

    /**
     * Set individual parameter on specific material slot
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetScalarParameter(EAvatarMaterialSlot Slot, FName ParameterName, float Value);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetVectorParameter(EAvatarMaterialSlot Slot, FName ParameterName, FLinearColor Value);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetTextureParameter(EAvatarMaterialSlot Slot, FName ParameterName, UTexture* Texture);

    /**
     * Get dynamic material instance for specific slot
     */
    UFUNCTION(BlueprintPure, Category = "Avatar|Materials")
    UMaterialInstanceDynamic* GetMaterialInstance(EAvatarMaterialSlot Slot) const;

    /**
     * Batch update for performance
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void BatchUpdateParameters(const TArray<FName>& ParameterNames, const TArray<float>& Values);

    /**
     * Emotional state integration
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void ApplyEmotionalBlush(float Intensity, const FLinearColor& Color);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetPupilDilation(float Dilation);

    /**
     * Super-hot-girl aesthetic
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void EnableSuperHotGirlAesthetic(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetEyeSparkle(float Intensity, const FVector2D& Position);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetHairShimmer(float Intensity, float Speed);

    /**
     * Hyper-chaotic behavior
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void ApplyGlitchEffect(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetChaosColorShift(float Shift);

    /**
     * Deep Tree Echo effects
     */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetEchoResonance(float Resonance, const FLinearColor& Color);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetCognitiveLoadVisualization(float Load);

    UFUNCTION(BlueprintCallable, Category = "Avatar|Materials")
    void SetEmotionalAura(float Intensity, const FLinearColor& Color);

protected:
    /**
     * Material instance storage
     */
    UPROPERTY()
    TMap<EAvatarMaterialSlot, UMaterialInstanceDynamic*> MaterialInstances;

    UPROPERTY()
    USkeletalMeshComponent* OwnerMesh;

    /**
     * Current parameter state
     */
    UPROPERTY()
    FAvatarMaterialParameters CurrentParameters;

    /**
     * Material slot mapping (material index to slot)
     */
    UPROPERTY()
    TMap<int32, EAvatarMaterialSlot> SlotMapping;

    /**
     * Helper methods
     */
    void CreateDynamicMaterialInstances();
    void MapMaterialSlots();
    EAvatarMaterialSlot IdentifyMaterialSlot(UMaterialInterface* Material) const;
    void UpdateSkinParameters();
    void UpdateHairParameters();
    void UpdateEyeParameters();
    void UpdateGlobalEffects();
};

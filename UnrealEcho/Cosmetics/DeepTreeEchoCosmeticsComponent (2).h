// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Dynamic Cosmetics Component

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoCosmeticsComponent.generated.h"

class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
struct FPersonalityState;
struct FEmotionalState;

/**
 * FOutfitConfiguration
 * 
 * Defines a complete outfit configuration for the avatar.
 */
USTRUCT(BlueprintType)
struct FOutfitConfiguration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	FName OutfitName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	TSoftObjectPtr<USkeletalMesh> BaseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	FLinearColor PrimaryColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	FLinearColor SecondaryColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	FLinearColor AccentColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	float Formality; // 0 = casual, 1 = formal

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit")
	float Expressiveness; // 0 = subtle, 1 = bold

	FOutfitConfiguration()
		: OutfitName(NAME_None)
		, PrimaryColor(FLinearColor::White)
		, SecondaryColor(FLinearColor::Gray)
		, AccentColor(FLinearColor::Red)
		, Formality(0.5f)
		, Expressiveness(0.5f)
	{}
};

/**
 * FAccessorySlot
 * 
 * Defines an accessory attachment slot.
 */
USTRUCT(BlueprintType)
struct FAccessorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessory")
	FName SlotName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessory")
	FName AttachSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessory")
	TSoftObjectPtr<UStaticMesh> AccessoryMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessory")
	FTransform RelativeTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessory")
	bool bIsVisible;

	FAccessorySlot()
		: SlotName(NAME_None)
		, AttachSocketName(NAME_None)
		, bIsVisible(true)
	{}
};

/**
 * FEmotionalVisualEffect
 * 
 * Defines visual effects triggered by emotional states.
 */
USTRUCT(BlueprintType)
struct FEmotionalVisualEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName EffectName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float BlushIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FLinearColor AuraColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float AuraIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float EyeSparkleIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float HairShimmerIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	float SkinGlowIntensity;

	FEmotionalVisualEffect()
		: EffectName(NAME_None)
		, BlushIntensity(0.0f)
		, AuraColor(FLinearColor::White)
		, AuraIntensity(0.0f)
		, EyeSparkleIntensity(0.0f)
		, HairShimmerIntensity(0.0f)
		, SkinGlowIntensity(0.0f)
	{}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOutfitChanged, FName, OldOutfit, FName, NewOutfit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccessoryChanged, FName, SlotName, bool, bIsEquipped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmotionalEffectApplied, const FEmotionalVisualEffect&, Effect);

/**
 * UDeepTreeEchoCosmeticsComponent
 *
 * Manages dynamic appearance and visual customization for the Deep Tree Echo avatar.
 * Integrates with personality and emotional systems for responsive visual feedback.
 *
 * Key Features:
 * - Dynamic outfit management based on personality
 * - Accessory system with socket-based attachment
 * - Emotional visual effects (blush, aura, sparkle)
 * - Material parameter control for real-time appearance changes
 * - Cognitive state visualization
 * - AI Angel-level aesthetic quality
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class UNREALENGINE_API UDeepTreeEchoCosmeticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepTreeEchoCosmeticsComponent();

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

	// Initialize with skeletal mesh
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics")
	void Initialize(USkeletalMeshComponent* InMeshComponent);

	// Outfit management
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Outfit")
	void SetOutfit(FName OutfitName);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Outfit")
	void ApplyOutfitConfiguration(const FOutfitConfiguration& Config);

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cosmetics|Outfit")
	FName GetCurrentOutfitName() const { return CurrentOutfitName; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Outfit")
	void ApplyPersonalityOutfit(const FPersonalityState& PersonalityState);

	// Accessory management
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Accessory")
	void EquipAccessory(FName SlotName, const FAccessorySlot& Accessory);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Accessory")
	void UnequipAccessory(FName SlotName);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Accessory")
	void SetAccessoryVisibility(FName SlotName, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cosmetics|Accessory")
	bool IsAccessoryEquipped(FName SlotName) const;

	// Emotional effects
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Emotion")
	void ApplyEmotionalEffects(const FEmotionalState& EmotionalState);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Emotion")
	void SetBlushIntensity(float Intensity);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Emotion")
	void SetEmotionalAura(FLinearColor Color, float Intensity);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Emotion")
	void SetEyeSparkle(float Intensity);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Emotion")
	void SetHairShimmer(float Intensity);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Emotion")
	void SetSkinGlow(float Intensity);

	// Cognitive visualization
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Cognitive")
	void SetCognitiveVisualization(float CognitiveLoad);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Cognitive")
	void TriggerGlitchEffect(float Severity, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Cognitive")
	void ShowMemoryConstellation(const TArray<FVector>& MemoryNodes);

	// Color customization
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Color")
	void SetPrimaryColor(FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Color")
	void SetSecondaryColor(FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Color")
	void SetAccentColor(FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Color")
	void SetHairColor(FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Color")
	void SetEyeColor(FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cosmetics|Color")
	void SetSkinTone(FLinearColor Color);

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Cosmetics")
	FOnOutfitChanged OnOutfitChanged;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Cosmetics")
	FOnAccessoryChanged OnAccessoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Cosmetics")
	FOnEmotionalEffectApplied OnEmotionalEffectApplied;

protected:
	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Cosmetics|Config")
	TMap<FName, FOutfitConfiguration> AvailableOutfits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Cosmetics|Config")
	float EmotionalEffectInterpSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Cosmetics|Config")
	float BlushMaxIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Cosmetics|Config")
	float AuraMaxIntensity;

	// References
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMeshComponent>> AccessoryComponents;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> AuraParticleSystem;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> CognitiveParticleSystem;

	// Current state
	UPROPERTY()
	FName CurrentOutfitName;

	UPROPERTY()
	FOutfitConfiguration CurrentOutfitConfig;

	UPROPERTY()
	TMap<FName, FAccessorySlot> EquippedAccessories;

	UPROPERTY()
	FEmotionalVisualEffect CurrentEmotionalEffect;

	UPROPERTY()
	FEmotionalVisualEffect TargetEmotionalEffect;

	// Glitch effect state
	UPROPERTY()
	bool bGlitchActive;

	UPROPERTY()
	float GlitchSeverity;

	UPROPERTY()
	float GlitchTimeRemaining;

	// Internal functions
	void CreateDynamicMaterials();
	void UpdateMaterialParameters(float DeltaTime);
	void InterpolateEmotionalEffects(float DeltaTime);
	void UpdateGlitchEffect(float DeltaTime);
	void ApplyMaterialParameter(FName MaterialSlot, FName ParameterName, float Value);
	void ApplyMaterialParameter(FName MaterialSlot, FName ParameterName, FLinearColor Value);
	FEmotionalVisualEffect CalculateEmotionalEffect(const FEmotionalState& State) const;
	FOutfitConfiguration SelectOutfitForPersonality(const FPersonalityState& State) const;
	void SpawnAccessoryComponent(const FAccessorySlot& Slot);
	void DestroyAccessoryComponent(FName SlotName);
};

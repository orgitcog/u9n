// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Advanced Character Class

#pragma once

#include "CoreMinimal.h"
#include "ModularCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "DeepTreeEchoCharacter.generated.h"

class UPersonalityTraitSystem;
class UCognitiveStateComponent;
class UNeurochemicalSimulationComponent;
class UAvatar3DComponent;
class UDeepTreeEchoMovementComponent;
class UDeepTreeEchoInteractionComponent;
class UDeepTreeEchoCosmeticsComponent;
class UAudioManagerComponent;
class UAbilitySystemComponent;
class UInputComponent;

/**
 * FDeepTreeEchoReplicatedState
 * 
 * Compressed representation of the avatar's cognitive and emotional state for network replication.
 */
USTRUCT(BlueprintType)
struct FDeepTreeEchoReplicatedState
{
	GENERATED_BODY()

	// Emotional state (0-255 for each dimension)
	UPROPERTY()
	uint8 Valence = 128; // 0 = negative, 255 = positive

	UPROPERTY()
	uint8 Arousal = 128; // 0 = calm, 255 = excited

	UPROPERTY()
	uint8 Dominance = 128; // 0 = submissive, 255 = dominant

	// Cognitive load (0-255)
	UPROPERTY()
	uint8 CognitiveLoad = 0;

	// Personality traits (compressed to 5 bytes)
	UPROPERTY()
	uint8 Openness = 128;

	UPROPERTY()
	uint8 Conscientiousness = 128;

	UPROPERTY()
	uint8 Extraversion = 128;

	UPROPERTY()
	uint8 Agreeableness = 128;

	UPROPERTY()
	uint8 Neuroticism = 128;

	// Behavioral flags
	UPROPERTY()
	uint8 BehaviorFlags = 0; // Bit flags for various states

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FDeepTreeEchoReplicatedState> : public TStructOpsTypeTraitsBase2<FDeepTreeEchoReplicatedState>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/**
 * ADeepTreeEchoCharacter
 *
 * The main character class for the Deep Tree Echo AGI avatar.
 * Integrates personality, cognition, neurochemistry, and advanced visual systems.
 * Based on Lyra's modular character architecture for production-ready features.
 */
UCLASS(Blueprintable, BlueprintType, Config = Game, Meta = (ShortTooltip = "Deep Tree Echo AGI Avatar Character"))
class UNREALENGINE_API ADeepTreeEchoCharacter : public AModularCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADeepTreeEchoCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ACharacter interface
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	//~End of ACharacter interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	// Get the Deep Tree Echo components
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UPersonalityTraitSystem* GetPersonalitySystem() const { return PersonalitySystem; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UCognitiveStateComponent* GetCognitiveState() const { return CognitiveState; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UNeurochemicalSimulationComponent* GetNeurochemicalSystem() const { return NeurochemicalSystem; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UAvatar3DComponent* GetAvatarComponent() const { return AvatarComponent; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UAudioManagerComponent* GetAudioManager() const { return AudioManager; }

	// Emotional state accessors
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Emotion")
	float GetEmotionalValence() const;

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Emotion")
	float GetEmotionalArousal() const;

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Emotion")
	float GetEmotionalDominance() const;

	// Cognitive state accessors
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cognition")
	float GetCognitiveLoad() const;

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Cognition")
	float GetAttentionLevel() const;

	// Personality accessors
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Personality")
	float GetPersonalityTrait(FName TraitName) const;

	// Behavioral control
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Behavior")
	void TriggerEmergentBehavior(FName BehaviorName);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Behavior")
	void InterruptCurrentBehavior();

	// Environmental interaction
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void InteractWithObject(AActor* TargetObject);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	TArray<AActor*> GetNearbyInteractableObjects(float Radius = 500.0f) const;

	// Visual effects control
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Visual")
	void SetEmotionalAuraColor(FLinearColor Color, float Intensity);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Visual")
	void TriggerCognitiveVisualization(FName VisualizationType);

protected:
	// Core Deep Tree Echo components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPersonalityTraitSystem> PersonalitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCognitiveStateComponent> CognitiveState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNeurochemicalSimulationComponent> NeurochemicalSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAvatar3DComponent> AvatarComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDeepTreeEchoInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDeepTreeEchoCosmeticsComponent> CosmeticsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioManagerComponent> AudioManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// Replicated state
	UPROPERTY(Replicated)
	FDeepTreeEchoReplicatedState ReplicatedState;

	// Internal update functions
	void UpdateReplicatedState();
	void UpdateComponentSynchronization();
	void UpdateEmotionalEffects();
	void UpdateCognitiveVisualization();

	// Network replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Component initialization
	void InitializeComponents();
	void InitializeAbilitySystem();

	// Event handlers
	UFUNCTION()
	void OnPersonalityShift(const FPersonalityState& OldState, const FPersonalityState& NewState);

	UFUNCTION()
	void OnEmotionalStateChanged(const FEmotionalState& NewState);

	UFUNCTION()
	void OnCognitiveLoadChanged(float NewLoad);

private:
	// Cached values for performance
	float CachedCognitiveLoad;
	FVector CachedEmotionalState;
	
	// Update timers
	float ReplicationUpdateTimer;
	float VisualizationUpdateTimer;
};

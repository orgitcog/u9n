// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Advanced Character Class

#pragma once

#include "CoreMinimal.h"
#include "ModularCharacter.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "DeepTreeEchoCharacter.generated.h"

class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;
class UAvatar3DComponent;
class UDeepTreeEchoMovementComponent;
class UDeepTreeEchoInteractionComponent;
class UDeepTreeEchoCosmeticsComponent;
class UAudioManagerComponent;
class UDeepTreeEchoExpressionSystem;
class UDTELanguageModelBridge;
class UAbilitySystemComponent;
class UInputComponent;
struct FEmotionalChemistry;
enum class EGestureType : uint8;
enum class EEmotionState : uint8;
enum class EAuraEmotionType : uint8;

/**
 * FDeepTreeEchoAffect
 *
 * The character's affect as valence / arousal / dominance in 0..1 (0.5 neutral),
 * derived deterministically from the neurochemical simulation's
 * FEmotionalChemistry. This is the only affect the character reports or shows;
 * it is measured state, never a scripted mood.
 */
USTRUCT(BlueprintType)
struct FDeepTreeEchoAffect
{
	GENERATED_BODY()

	/** 0 = negative, 1 = positive. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep Tree Echo|Emotion")
	float Valence = 0.5f;

	/** 0 = calm, 1 = excited. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep Tree Echo|Emotion")
	float Arousal = 0.5f;

	/** 0 = submissive, 1 = dominant. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep Tree Echo|Emotion")
	float Dominance = 0.5f;

	/** Distance from neutral (0..1); below ~0.15 the affect reads as neutral. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep Tree Echo|Emotion")
	float Intensity = 0.0f;

	/** Deterministic mapping from the six chemistry readings to the three dimensions. */
	static FDeepTreeEchoAffect FromChemistry(const FEmotionalChemistry& Chemistry);

	/** One line of "valence 0.62, arousal 0.48, dominance 0.55" for model context and logs. */
	FString Describe() const;
};

/**
 * FDeepTreeEchoReplicatedState
 *
 * Compressed representation of the avatar's affect, cognitive load and
 * personality for network replication.
 */
USTRUCT(BlueprintType)
struct FDeepTreeEchoReplicatedState
{
	GENERATED_BODY()

	// Affect (0-255 for each dimension)
	UPROPERTY()
	uint8 Valence = 128; // 0 = negative, 255 = positive

	UPROPERTY()
	uint8 Arousal = 128; // 0 = calm, 255 = excited

	UPROPERTY()
	uint8 Dominance = 128; // 0 = submissive, 255 = dominant

	// Cognitive load (0-255)
	UPROPERTY()
	uint8 CognitiveLoad = 0;

	/**
	 * Personality trait intensities (0-255), indexed by EPersonalityTraitType.
	 * The array is sized for the ten traits that enum declares.
	 */
	UPROPERTY()
	uint8 TraitIntensities[10];

	// Behavioral flags (see EDeepTreeEchoBehaviorFlags)
	UPROPERTY()
	uint8 BehaviorFlags = 0;

	FDeepTreeEchoReplicatedState()
	{
		FMemory::Memset(TraitIntensities, 128, sizeof(TraitIntensities));
	}

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

/** Bits of FDeepTreeEchoReplicatedState::BehaviorFlags. */
namespace EDeepTreeEchoBehaviorFlags
{
	enum Type : uint8
	{
		Speaking = 1 << 0,          // speech audio is playing
		GesturePlaying = 1 << 1,    // a gesture chosen for a behaviour is playing
		BehaviorPending = 1 << 2,   // the model is choosing a gesture for a behaviour
		UtterancePending = 1 << 3   // the model is composing an utterance
	};
}

/** A gesture was chosen (by the model) for a named behaviour and started. GestureName is None when the model chose no gesture. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDeepTreeEchoBehaviorChosen, FName, BehaviorName, FName, GestureName, float, Intensity);

/** Model text was handed to the audio manager for synthesis. RequestId is the Speak() request. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeepTreeEchoUtteranceRequested, const FString&, Text, const FString&, RequestId);

/**
 * ADeepTreeEchoCharacter
 *
 * The main character class for the Deep Tree Echo AGI avatar.
 * Integrates personality, neurochemistry and the avatar's visual and audio systems.
 * Based on Lyra's modular character architecture for production-ready features.
 *
 * Nothing this character says or does as a reaction is scripted here:
 *   - affect is read from UNeurochemicalSimulationComponent and mapped deterministically
 *     to the face, aura, cosmetics and soundtrack;
 *   - every utterance is requested from the language model (UDTELanguageModelBridge) and
 *     spoken only through UAudioManagerComponent::Speak, i.e. only as synthesized audio;
 *   - a behaviour request asks the model to choose the gesture from the rig's gesture menu.
 * When no model answers, the character does nothing for that occasion and a warning is logged.
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
	UNeurochemicalSimulationComponent* GetNeurochemicalSystem() const { return NeurochemicalSystem; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UAvatar3DComponent* GetAvatarComponent() const { return AvatarComponent; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UAudioManagerComponent* GetAudioManager() const { return AudioManager; }

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UDeepTreeEchoInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** The language model bridge every utterance and behaviour choice goes through. */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo")
	UDTELanguageModelBridge* GetModelBridge() const { return ModelBridge; }

	// Affect accessors (derived from the neurochemical simulation)
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Emotion")
	FDeepTreeEchoAffect GetAffect() const;

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Emotion")
	float GetEmotionalValence() const;

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Emotion")
	float GetEmotionalArousal() const;

	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Emotion")
	float GetEmotionalDominance() const;

	// Cognitive state accessors
	/** Cognitive load (0-1): the inverse of the neurochemical system's cognitive clarity. */
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cognition")
	float GetCognitiveLoad() const;

	/** Attention (0-1): the expression system's measured attention when present, else the neurochemical focus reading. */
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Cognition")
	float GetAttentionLevel() const;

	// Personality accessors
	/** Intensity of the trait whose EPersonalityTraitType name is TraitName (e.g. "Confident"); 0 for an unknown name. */
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Personality")
	float GetPersonalityTrait(FName TraitName) const;

	// Behavioral control

	/**
	 * Lets the character act on a named behaviour (e.g. "greeting", "unpredictable_gesture").
	 * Which gesture, if any, is chosen by the language model from the rig's gesture menu with
	 * the behaviour name and the live state as context; the choice arrives asynchronously
	 * through OnBehaviorChosen. No gesture is played when the model does not answer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Behavior")
	void TriggerEmergentBehavior(FName BehaviorName);

	/** True while the model is choosing a gesture for a behaviour. */
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Behavior")
	bool IsBehaviorRequestPending() const { return bBehaviorRequestPending; }

	/** Stops the current gesture montage and any speech that is playing. */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Behavior")
	void InterruptCurrentBehavior();

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Behavior")
	FOnDeepTreeEchoBehaviorChosen OnBehaviorChosen;

	// Speech

	/**
	 * Asks the model for an utterance (Instruction says what kind: greet the visitor, answer
	 * the question in Context, ...) and hands the reply to the audio manager for synthesis.
	 * The character's live state is appended to Context. Returns false when no request was
	 * sent (no model bridge, or no speech synthesizer bound so nothing could be heard).
	 * Nothing is spoken when the model does not answer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Speech", meta = (AutoCreateRefTerm = "Context"))
	bool RequestUtterance(const FString& Instruction, const FString& Context);

	/** Number of utterance requests still waiting for the model. */
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Speech")
	int32 GetPendingUtteranceCount() const { return PendingUtteranceRequests; }

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Speech")
	FOnDeepTreeEchoUtteranceRequested OnUtteranceRequested;

	// Environmental interaction
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void InteractWithObject(AActor* TargetObject);

	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	TArray<AActor*> GetNearbyInteractableObjects(float Radius = 500.0f) const;

	// Visual effects control
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Visual")
	void SetEmotionalAuraColor(FLinearColor Color, float Intensity);

	/** Shows the named ECognitiveActivityType (e.g. "Thinking") in the cognitive visualization. */
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Visual")
	void TriggerCognitiveVisualization(FName VisualizationType);

protected:
	// Core Deep Tree Echo components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deep Tree Echo|Components", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPersonalityTraitSystem> PersonalitySystem;

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

	/** Resolved in BeginPlay via UDTELanguageModelBridge::FindOrAdd. */
	UPROPERTY(Transient)
	TObjectPtr<UDTELanguageModelBridge> ModelBridge;

	/** Optional expression system on this actor (added in Blueprint); found in BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<UDeepTreeEchoExpressionSystem> ExpressionSystem;

	// Replicated state
	UPROPERTY(Replicated)
	FDeepTreeEchoReplicatedState ReplicatedState;

	// Internal update functions
	void UpdateReplicatedState();
	void UpdateComponentSynchronization();
	void UpdateEmotionalEffects();
	void UpdateCognitiveVisualization();

	/** Runs the gesture the model chose for BehaviorName and broadcasts OnBehaviorChosen. */
	void ApplyBehavior(FName BehaviorName, EGestureType Gesture, float Intensity);

	/** Affect, chemistry, cognitive load, attention, dominant traits and speech state as model context. */
	FString DescribeStateForModel() const;

	// Network replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Component initialization
	void InitializeComponents();
	void InitializeAbilitySystem();

	// Event handlers

	/** The personality system chose a chaotic event; gesture events are routed through TriggerEmergentBehavior. */
	UFUNCTION()
	void HandleChaoticEventChosen(FName EventName, float Intensity);

private:
	// Deterministic affect -> discrete emotion mappings for the sub-systems that take one
	static EEmotionState MapAffectToFacialEmotion(const FDeepTreeEchoAffect& Affect);
	static EAuraEmotionType MapAffectToAuraEmotion(const FDeepTreeEchoAffect& Affect);

	// Last discrete emotions handed to the facial and aura systems (so transitions are not restarted every tick)
	bool bHasAppliedFacialEmotion;
	EEmotionState LastFacialEmotion;
	bool bHasAppliedAuraEmotion;
	EAuraEmotionType LastAuraEmotion;

	// Model request state
	bool bBehaviorRequestPending;
	int32 PendingUtteranceRequests;

	// Update timers
	float ReplicationUpdateTimer;
	float VisualizationUpdateTimer;
};

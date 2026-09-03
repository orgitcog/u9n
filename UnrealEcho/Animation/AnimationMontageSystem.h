// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Animation Montage Integration System

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AnimationMontageSystem.generated.h"

class UAnimMontage;
class UAnimInstance;
struct FEmotionalState;
struct FPersonalityState;

/**
 * EMontageCategory
 * 
 * Categories of animation montages.
 */
UENUM(BlueprintType)
enum class EMontageCategory : uint8
{
	Idle,           // Idle variations and fidgets
	Gesture,        // Hand gestures and body language
	Emote,          // Emotional expressions
	Interaction,    // Object interactions
	Social,         // Social interactions
	Locomotion,     // Movement variations
	Combat,         // Combat actions
	Special         // Special/unique animations
};

/**
 * FMontageVariant
 * 
 * A variant of a montage with personality/emotion weighting.
 */
USTRUCT(BlueprintType)
struct FMontageVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	TSoftObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	float PlayRate;

	// Personality weights (0-1, how much this variant matches each trait)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Personality")
	float ExtraversionWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Personality")
	float OpennessWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Personality")
	float ConscientiousnessWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Personality")
	float AgreeablenessWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Personality")
	float NeuroticismWeight;

	// Emotion weights
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Emotion")
	float ValenceWeight; // -1 = negative, 1 = positive

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Emotion")
	float ArousalWeight; // 0 = calm, 1 = excited

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage|Emotion")
	float DominanceWeight; // 0 = submissive, 1 = dominant

	FMontageVariant()
		: PlayRate(1.0f)
		, ExtraversionWeight(0.5f)
		, OpennessWeight(0.5f)
		, ConscientiousnessWeight(0.5f)
		, AgreeablenessWeight(0.5f)
		, NeuroticismWeight(0.5f)
		, ValenceWeight(0.0f)
		, ArousalWeight(0.5f)
		, DominanceWeight(0.5f)
	{}
};

/**
 * FMontageEntry
 * 
 * A montage entry with multiple variants.
 */
USTRUCT(BlueprintType)
struct FMontageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	FName MontageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	EMontageCategory Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	TArray<FMontageVariant> Variants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	FGameplayTagContainer MontageTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	float Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	bool bCanInterrupt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	int32 Priority; // Higher priority can interrupt lower

	FMontageEntry()
		: Category(EMontageCategory::Gesture)
		, Cooldown(0.0f)
		, bCanInterrupt(true)
		, Priority(0)
	{}
};

/**
 * UMontageLibraryDataAsset
 * 
 * A library of animation montages.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UMontageLibraryDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Library")
	FName LibraryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Library")
	FText LibraryName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Library")
	TArray<FMontageEntry> Montages;

	// Get montage by ID
	UFUNCTION(BlueprintPure, Category = "Library")
	FMontageEntry GetMontageById(FName MontageId) const;

	// Get montages by category
	UFUNCTION(BlueprintPure, Category = "Library")
	TArray<FMontageEntry> GetMontagesByCategory(EMontageCategory Category) const;

	// Get montages by tag
	UFUNCTION(BlueprintPure, Category = "Library")
	TArray<FMontageEntry> GetMontagesByTag(FGameplayTag Tag) const;
};

/**
 * FMontagePlayRequest
 * 
 * A request to play a montage.
 */
USTRUCT(BlueprintType)
struct FMontagePlayRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Request")
	FName MontageId;

	UPROPERTY(BlueprintReadWrite, Category = "Request")
	float PlayRate;

	UPROPERTY(BlueprintReadWrite, Category = "Request")
	float StartPosition;

	UPROPERTY(BlueprintReadWrite, Category = "Request")
	FName StartSection;

	UPROPERTY(BlueprintReadWrite, Category = "Request")
	bool bForcePlay;

	FMontagePlayRequest()
		: PlayRate(1.0f)
		, StartPosition(0.0f)
		, StartSection(NAME_None)
		, bForcePlay(false)
	{}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageStarted, FName, MontageId, UAnimMontage*, Montage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMontageEnded, FName, MontageId, UAnimMontage*, Montage, bool, bInterrupted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageNotify, FName, MontageId, FName, NotifyName);

/**
 * UAnimationMontageSystem
 *
 * Manages animation montage playback with personality and emotion-aware variant selection.
 * Provides a high-level interface for playing context-appropriate animations.
 *
 * Key Features:
 * - Personality-based variant selection
 * - Emotion-based variant selection
 * - Cooldown management
 * - Priority-based interruption
 * - Montage library support
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class UNREALENGINE_API UAnimationMontageSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UAnimationMontageSystem();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Initialize with anim instance
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Initialize(UAnimInstance* InAnimInstance);

	// Set montage library
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void SetMontageLibrary(UMontageLibraryDataAsset* Library);

	// Play montage by ID (selects best variant based on personality/emotion)
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	bool PlayMontage(FName MontageId, const FMontagePlayRequest& Request = FMontagePlayRequest());

	// Play specific montage directly
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	bool PlayMontageDirectly(UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSection = NAME_None);

	// Play random montage from category
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	bool PlayRandomFromCategory(EMontageCategory Category);

	// Stop current montage
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void StopCurrentMontage(float BlendOutTime = 0.25f);

	// Check if montage is playing
	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	bool IsMontagePlayingById(FName MontageId) const;

	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	bool IsAnyMontagePlayingFromCategory(EMontageCategory Category) const;

	// Set personality state for variant selection
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void SetPersonalityState(const FPersonalityState& State);

	// Set emotional state for variant selection
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void SetEmotionalState(const FEmotionalState& State);

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Animation|Montage")
	FOnMontageStarted OnMontageStarted;

	UPROPERTY(BlueprintAssignable, Category = "Animation|Montage")
	FOnMontageEnded OnMontageEnded;

	UPROPERTY(BlueprintAssignable, Category = "Animation|Montage")
	FOnMontageNotify OnMontageNotify;

protected:
	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Montage|Config")
	TObjectPtr<UMontageLibraryDataAsset> DefaultMontageLibrary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Montage|Config")
	float DefaultBlendInTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Montage|Config")
	float DefaultBlendOutTime;

	// References
	UPROPERTY()
	TObjectPtr<UAnimInstance> AnimInstance;

	UPROPERTY()
	TObjectPtr<UMontageLibraryDataAsset> CurrentLibrary;

	// State
	UPROPERTY()
	FName CurrentMontageId;

	UPROPERTY()
	TObjectPtr<UAnimMontage> CurrentMontage;

	UPROPERTY()
	int32 CurrentMontagePriority;

	UPROPERTY()
	TMap<FName, float> MontageCooldowns;

	// Personality/Emotion state for variant selection
	UPROPERTY()
	float PersonalityExtraversion;

	UPROPERTY()
	float PersonalityOpenness;

	UPROPERTY()
	float PersonalityConscientiousness;

	UPROPERTY()
	float PersonalityAgreeableness;

	UPROPERTY()
	float PersonalityNeuroticism;

	UPROPERTY()
	float EmotionalValence;

	UPROPERTY()
	float EmotionalArousal;

	UPROPERTY()
	float EmotionalDominance;

	// Internal functions
	FMontageVariant SelectBestVariant(const FMontageEntry& Entry) const;
	float CalculateVariantScore(const FMontageVariant& Variant) const;
	bool CanPlayMontage(const FMontageEntry& Entry, bool bForce) const;
	void UpdateCooldowns(float DeltaTime);
	void OnMontageEndedInternal(UAnimMontage* Montage, bool bInterrupted);

	// Delegate handles
	FDelegateHandle MontageEndedHandle;
};

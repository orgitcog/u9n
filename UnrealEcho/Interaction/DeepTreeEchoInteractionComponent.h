// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Cognitive-Aware Interaction Component

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DeepTreeEchoInteractionComponent.generated.h"

class ADeepTreeEchoCharacter;
class UCognitiveStateComponent;
class UPersonalityTraitSystem;

/**
 * FInteractionMemory
 * 
 * Records a past interaction for cognitive processing and learning.
 */
USTRUCT(BlueprintType)
struct FInteractionMemory
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> InteractedActor;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FName InteractionType;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Timestamp;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float EmotionalValence; // How positive/negative the interaction was

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float CognitiveLoad; // How mentally demanding it was

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bWasSuccessful;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTagContainer InteractionTags;

	FInteractionMemory()
		: InteractionType(NAME_None)
		, Timestamp(0.0f)
		, EmotionalValence(0.5f)
		, CognitiveLoad(0.0f)
		, bWasSuccessful(true)
	{}
};

/**
 * FInteractableInfo
 * 
 * Information about a nearby interactable object.
 */
USTRUCT(BlueprintType)
struct FInteractableInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Distance;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Priority; // Calculated based on cognitive state and personality

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FName InteractionType;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTagContainer InteractionTags;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bRequiresLineOfSight;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bIsCurrentlyVisible;

	FInteractableInfo()
		: Distance(0.0f)
		, Priority(0.0f)
		, InteractionType(NAME_None)
		, bRequiresLineOfSight(true)
		, bIsCurrentlyVisible(false)
	{}
};

/**
 * FInteractionResult
 * 
 * Result of an interaction attempt.
 */
USTRUCT(BlueprintType)
struct FInteractionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bSuccess;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText ResultMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float EmotionalImpact; // -1 to 1, negative to positive

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float CognitiveImpact; // 0 to 1, how much cognitive load was added

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TArray<FName> UnlockedInteractions; // New interactions now available

	FInteractionResult()
		: bSuccess(false)
		, EmotionalImpact(0.0f)
		, CognitiveImpact(0.0f)
	{}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionStarted, AActor*, Target, FName, InteractionType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionCompleted, AActor*, Target, const FInteractionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableDiscovered, const FInteractableInfo&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableLost, AActor*, Actor);

/**
 * UDeepTreeEchoInteractionComponent
 *
 * Manages environmental awareness and object interactions for the Deep Tree Echo avatar.
 * Integrates with cognitive and personality systems to provide intelligent, context-aware interactions.
 *
 * Key Features:
 * - Environmental scanning for interactable objects
 * - Priority-based interaction selection based on cognitive state
 * - Interaction memory for learning and adaptation
 * - Personality-influenced interaction preferences
 * - Line-of-sight and attention-based filtering
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class UNREALENGINE_API UDeepTreeEchoInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepTreeEchoInteractionComponent();

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

	// Initialize with owning character
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void Initialize(ADeepTreeEchoCharacter* InOwner);

	// Find all interactable objects within radius
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	TArray<AActor*> FindInteractableObjects(float Radius = 500.0f) const;

	// Get detailed info about nearby interactables (sorted by priority)
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	TArray<FInteractableInfo> GetNearbyInteractables() const;

	// Get the highest priority interactable
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	FInteractableInfo GetBestInteractable() const;

	// Interact with a specific actor
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	FInteractionResult InteractWith(AActor* Target);

	// Interact with the best available target
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	FInteractionResult InteractWithBest();

	// Check if an actor is interactable
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	bool IsInteractable(AActor* Actor) const;

	// Check if currently interacting
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	bool IsInteracting() const { return bIsInteracting; }

	// Cancel current interaction
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void CancelInteraction();

	// Get interaction memory
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	TArray<FInteractionMemory> GetInteractionHistory() const { return InteractionHistory; }

	// Get interaction count with specific actor
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	int32 GetInteractionCount(AActor* Actor) const;

	// Get emotional association with actor (based on past interactions)
	UFUNCTION(BlueprintPure, Category = "Deep Tree Echo|Interaction")
	float GetEmotionalAssociation(AActor* Actor) const;

	// Set gaze target for attention-based interactions
	UFUNCTION(BlueprintCallable, Category = "Deep Tree Echo|Interaction")
	void SetGazeTarget(AActor* Target);

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractionCompleted OnInteractionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractableDiscovered OnInteractableDiscovered;

	UPROPERTY(BlueprintAssignable, Category = "Deep Tree Echo|Interaction")
	FOnInteractableLost OnInteractableLost;

protected:
	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	float ScanRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	float ScanInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	int32 MaxInteractionMemory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	TArray<TEnumAsByte<EObjectTypeQuery>> InteractableObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	FGameplayTagContainer RequiredInteractionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	bool bRequireLineOfSight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deep Tree Echo|Interaction|Config")
	float AttentionBonusMultiplier;

	// References
	UPROPERTY()
	TObjectPtr<ADeepTreeEchoCharacter> OwningCharacter;

	UPROPERTY()
	TObjectPtr<UCognitiveStateComponent> CognitiveState;

	UPROPERTY()
	TObjectPtr<UPersonalityTraitSystem> PersonalitySystem;

	// State
	UPROPERTY()
	TArray<FInteractableInfo> CachedInteractables;

	UPROPERTY()
	TArray<FInteractionMemory> InteractionHistory;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentInteractionTarget;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentGazeTarget;

	UPROPERTY()
	bool bIsInteracting;

	UPROPERTY()
	float TimeSinceLastScan;

	// Internal functions
	void PerformEnvironmentScan();
	void UpdateInteractablePriorities();
	float CalculateInteractionPriority(const FInteractableInfo& Info) const;
	bool CheckLineOfSight(AActor* Target) const;
	void RecordInteraction(AActor* Target, FName InteractionType, const FInteractionResult& Result);
	void CleanupOldMemories();
	FInteractionResult ExecuteInteraction(AActor* Target);
	void NotifyInteractableChanges(const TArray<FInteractableInfo>& OldInteractables);

	// Personality-based modifiers
	float GetPersonalityInteractionModifier(const FInteractableInfo& Info) const;
	float GetCognitiveInteractionModifier(const FInteractableInfo& Info) const;
	float GetMemoryInteractionModifier(AActor* Actor) const;
};

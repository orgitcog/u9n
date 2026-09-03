// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Interactable Object System

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "InteractableObject.generated.h"

class UDeepTreeEchoInteractionComponent;

/**
 * EInteractionType
 * 
 * Types of interactions available in the system.
 */
UENUM(BlueprintType)
enum class EInteractionType : uint8
{
	None,
	Examine,        // Look at and learn about
	PickUp,         // Take into inventory
	Use,            // Activate or operate
	Talk,           // Engage in conversation
	Push,           // Physical manipulation
	Open,           // Open container/door
	Sit,            // Sit on furniture
	Sleep,          // Rest on bed
	Eat,            // Consume food
	Drink,          // Consume beverage
	Read,           // Read text
	Play,           // Play with toy/game
	Pet,            // Pet an animal
	Hug,            // Embrace
	Dance,          // Dance with/on
	Custom          // Custom interaction
};

/**
 * FInteractionData
 * 
 * Data defining a single interaction type for an object.
 */
USTRUCT(BlueprintType)
struct FInteractionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	EInteractionType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float Duration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float CognitiveLoad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float EmotionalImpact; // -1 to 1

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTagContainer GrantedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TSoftObjectPtr<UAnimMontage> InteractionMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TSoftObjectPtr<USoundBase> InteractionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bRequiresLineOfSight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float MaxInteractionDistance;

	FInteractionData()
		: Type(EInteractionType::Examine)
		, Duration(1.0f)
		, CognitiveLoad(0.1f)
		, EmotionalImpact(0.0f)
		, bRequiresLineOfSight(true)
		, MaxInteractionDistance(200.0f)
	{}
};

/**
 * UInteractableDataAsset
 * 
 * Data asset defining properties of an interactable object.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UInteractableDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable")
	FName InteractableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable")
	TArray<FInteractionData> AvailableInteractions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable")
	FGameplayTagContainer ObjectTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Personality")
	float ExtraversionAffinity; // How much extraverts like this object

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Personality")
	float OpennessAffinity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Personality")
	float ConscientiousnessAffinity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Personality")
	float AgreeablenessAffinity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interactable|Personality")
	float NeuroticismAffinity;

	UInteractableDataAsset()
		: ExtraversionAffinity(0.5f)
		, OpennessAffinity(0.5f)
		, ConscientiousnessAffinity(0.5f)
		, AgreeablenessAffinity(0.5f)
		, NeuroticismAffinity(0.5f)
	{}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionBegin, AActor*, Interactor, EInteractionType, Type);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInteractionEnd, AActor*, Interactor, EInteractionType, Type, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionAvailable, AActor*, Interactor);

/**
 * AInteractableObject
 *
 * Base class for all interactable objects in the Deep Tree Echo system.
 * Provides a flexible interaction framework that integrates with the
 * cognitive and personality systems.
 */
UCLASS(Blueprintable)
class UNREALENGINE_API AInteractableObject : public AActor
{
	GENERATED_BODY()

public:
	AInteractableObject();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Check if interaction is available
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool IsInteractionAvailable(AActor* Interactor, EInteractionType Type) const;

	// Get available interactions for an actor
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	TArray<FInteractionData> GetAvailableInteractions(AActor* Interactor) const;

	// Begin interaction
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool BeginInteraction(AActor* Interactor, EInteractionType Type);

	// End interaction
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void EndInteraction(AActor* Interactor, bool bSuccess = true);

	// Cancel interaction
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CancelInteraction(AActor* Interactor);

	// Get interaction data
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FInteractionData GetInteractionData(EInteractionType Type) const;

	// Get personality affinity
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetPersonalityAffinity(const struct FPersonalityState& Personality) const;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionBegin OnInteractionBegin;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionEnd OnInteractionEnd;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionAvailable OnInteractionAvailable;

protected:
	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Config")
	TObjectPtr<UInteractableDataAsset> InteractableData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Config")
	bool bCanInteractWhileBusy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Config")
	int32 MaxSimultaneousInteractors;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction|Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// State
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> CurrentInteractors;

	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, EInteractionType> ActiveInteractions;

	UPROPERTY()
	float InteractionProgress;

	// Internal functions
	virtual void OnInteractionStarted(AActor* Interactor, EInteractionType Type);
	virtual void OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess);
	virtual void UpdateInteraction(float DeltaTime);
	bool CheckInteractionRequirements(AActor* Interactor, const FInteractionData& Data) const;
};

/**
 * AInteractableContainer
 * 
 * An interactable object that can contain other items.
 */
UCLASS(Blueprintable)
class UNREALENGINE_API AInteractableContainer : public AInteractableObject
{
	GENERATED_BODY()

public:
	AInteractableContainer();

	UFUNCTION(BlueprintCallable, Category = "Container")
	void Open(AActor* Opener);

	UFUNCTION(BlueprintCallable, Category = "Container")
	void Close(AActor* Closer);

	UFUNCTION(BlueprintPure, Category = "Container")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintCallable, Category = "Container")
	TArray<AActor*> GetContents() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
	bool bIsOpen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
	TArray<TSubclassOf<AActor>> ContainedItemClasses;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedContents;

	virtual void OnInteractionStarted(AActor* Interactor, EInteractionType Type) override;
};

/**
 * AInteractableFurniture
 * 
 * Furniture that the avatar can sit on, sleep on, etc.
 */
UCLASS(Blueprintable)
class UNREALENGINE_API AInteractableFurniture : public AInteractableObject
{
	GENERATED_BODY()

public:
	AInteractableFurniture();

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	bool Occupy(AActor* Occupant);

	UFUNCTION(BlueprintCallable, Category = "Furniture")
	void Vacate(AActor* Occupant);

	UFUNCTION(BlueprintPure, Category = "Furniture")
	bool IsOccupied() const { return CurrentOccupant != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Furniture")
	AActor* GetOccupant() const { return CurrentOccupant; }

	UFUNCTION(BlueprintPure, Category = "Furniture")
	FTransform GetSitTransform() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	FName SitSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	FTransform SitOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	TSoftObjectPtr<UAnimMontage> SitDownMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
	TSoftObjectPtr<UAnimMontage> StandUpMontage;

	UPROPERTY()
	TObjectPtr<AActor> CurrentOccupant;

	virtual void OnInteractionStarted(AActor* Interactor, EInteractionType Type) override;
	virtual void OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess) override;
};

/**
 * AInteractableNPC
 * 
 * An NPC that can be interacted with for conversation, etc.
 */
UCLASS(Blueprintable)
class UNREALENGINE_API AInteractableNPC : public AInteractableObject
{
	GENERATED_BODY()

public:
	AInteractableNPC();

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void StartConversation(AActor* Initiator);

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void EndConversation();

	UFUNCTION(BlueprintPure, Category = "NPC")
	bool IsInConversation() const { return bInConversation; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FText NPCName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	TArray<FText> GreetingLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	float RelationshipLevel;

	UPROPERTY()
	bool bInConversation;

	UPROPERTY()
	TWeakObjectPtr<AActor> ConversationPartner;

	virtual void OnInteractionStarted(AActor* Interactor, EInteractionType Type) override;
	virtual void OnInteractionCompleted(AActor* Interactor, EInteractionType Type, bool bSuccess) override;
};

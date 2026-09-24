// SocialCognitionSystem.h
// Social Cognition and Theory of Mind for Deep Tree Echo
// Implements agent modeling, relationship tracking, and social inference

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SocialCognitionSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UMemorySystems;
class UEmotionalSystem;

/**
 * Relationship type
 */
UENUM(BlueprintType)
enum class ERelationshipType : uint8
{
    Unknown     UMETA(DisplayName = "Unknown"),
    Stranger    UMETA(DisplayName = "Stranger"),
    Acquaintance UMETA(DisplayName = "Acquaintance"),
    Friend      UMETA(DisplayName = "Friend"),
    CloseFriend UMETA(DisplayName = "Close Friend"),
    Ally        UMETA(DisplayName = "Ally"),
    Rival       UMETA(DisplayName = "Rival"),
    Adversary   UMETA(DisplayName = "Adversary")
};

/**
 * Agent mental state
 */
USTRUCT(BlueprintType)
struct FAgentMentalState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Beliefs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Desires;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Intentions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EstimatedValence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EstimatedArousal = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.5f;
};

/**
 * Social agent model
 */
USTRUCT(BlueprintType)
struct FSocialAgent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAgentMentalState MentalState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> Traits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Trustworthiness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Competence = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Warmth = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 InteractionCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastInteractionTime = 0.0f;
};

/**
 * Social relationship
 */
USTRUCT(BlueprintType)
struct FSocialRelationship
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RelationshipID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ERelationshipType Type = ERelationshipType::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Affinity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Trust = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Familiarity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Reciprocity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SharedExperiences;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RelationshipStartTime = 0.0f;
};

/**
 * Social interaction
 */
USTRUCT(BlueprintType)
struct FSocialInteraction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InteractionID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InteractionType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Valence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Context;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAgentDiscovered, const FSocialAgent&, Agent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRelationshipChanged, const FString&, AgentID, ERelationshipType, NewType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSocialInteraction, const FSocialInteraction&, Interaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMentalStateInferred, const FString&, AgentID, const FAgentMentalState&, State);

/**
 * Social Cognition System Component
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USocialCognitionSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    USocialCognitionSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAgentDiscovered OnAgentDiscovered;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRelationshipChanged OnRelationshipChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSocialInteraction OnSocialInteraction;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMentalStateInferred OnMentalStateInferred;

    // ========================================
    // AGENT MODELING
    // ========================================

    /** Register a new social agent */
    UFUNCTION(BlueprintCallable, Category = "Agents")
    FSocialAgent RegisterAgent(const FString& Name);

    /** Get agent by ID */
    UFUNCTION(BlueprintPure, Category = "Agents")
    FSocialAgent GetAgent(const FString& AgentID) const;

    /** Get all known agents */
    UFUNCTION(BlueprintPure, Category = "Agents")
    TArray<FSocialAgent> GetAllAgents() const;

    /** Update agent trait */
    UFUNCTION(BlueprintCallable, Category = "Agents")
    void UpdateAgentTrait(const FString& AgentID, const FString& Trait, float Value);

    /** Infer agent mental state */
    UFUNCTION(BlueprintCallable, Category = "Agents")
    FAgentMentalState InferMentalState(const FString& AgentID, const TArray<FString>& ObservedBehaviors);

    // ========================================
    // RELATIONSHIPS
    // ========================================

    /** Get relationship with agent */
    UFUNCTION(BlueprintPure, Category = "Relationships")
    FSocialRelationship GetRelationship(const FString& AgentID) const;

    /** Update relationship affinity */
    UFUNCTION(BlueprintCallable, Category = "Relationships")
    void UpdateAffinity(const FString& AgentID, float Delta);

    /** Update trust */
    UFUNCTION(BlueprintCallable, Category = "Relationships")
    void UpdateTrust(const FString& AgentID, float Delta);

    /** Get agents by relationship type */
    UFUNCTION(BlueprintPure, Category = "Relationships")
    TArray<FSocialAgent> GetAgentsByRelationship(ERelationshipType Type) const;

    // ========================================
    // SOCIAL INTERACTIONS
    // ========================================

    /** Record a social interaction */
    UFUNCTION(BlueprintCallable, Category = "Interactions")
    FSocialInteraction RecordInteraction(const FString& AgentID, const FString& Type, 
                                          float Valence, const FString& Context);

    /** Get recent interactions */
    UFUNCTION(BlueprintPure, Category = "Interactions")
    TArray<FSocialInteraction> GetRecentInteractions(int32 Count) const;

    /** Get interactions with agent */
    UFUNCTION(BlueprintPure, Category = "Interactions")
    TArray<FSocialInteraction> GetInteractionsWithAgent(const FString& AgentID) const;

    // ========================================
    // THEORY OF MIND
    // ========================================

    /** Predict agent action */
    UFUNCTION(BlueprintPure, Category = "Theory of Mind")
    FString PredictAgentAction(const FString& AgentID, const FString& Situation) const;

    /** Estimate agent emotion */
    UFUNCTION(BlueprintPure, Category = "Theory of Mind")
    float EstimateAgentEmotion(const FString& AgentID, const FString& Event) const;

    /** Check if agent believes something */
    UFUNCTION(BlueprintPure, Category = "Theory of Mind")
    bool DoesAgentBelieve(const FString& AgentID, const FString& Belief) const;

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UMemorySystems* MemoryComponent;

    UPROPERTY()
    UEmotionalSystem* EmotionalComponent;

    // Internal state
    TArray<FSocialAgent> KnownAgents;
    TArray<FSocialRelationship> Relationships;
    TArray<FSocialInteraction> Interactions;

    int32 AgentIDCounter = 0;
    int32 RelationshipIDCounter = 0;
    int32 InteractionIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeSocialSystem();

    void UpdateRelationshipTypes();
    ERelationshipType ComputeRelationshipType(const FSocialRelationship& Rel) const;

    FString GenerateAgentID();
    FString GenerateRelationshipID();
    FString GenerateInteractionID();

    int32 FindAgentIndex(const FString& AgentID) const;
    int32 FindRelationshipIndex(const FString& AgentID) const;
};

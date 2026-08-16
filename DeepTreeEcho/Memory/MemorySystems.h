// MemorySystems.h
// Episodic and Semantic Memory Systems for Deep Tree Echo
// Implements multi-level memory architecture with consolidation and retrieval

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MemorySystems.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class URelevanceRealizationEnnead;

/**
 * Memory type enumeration
 */
UENUM(BlueprintType)
enum class EMemoryType : uint8
{
    Episodic    UMETA(DisplayName = "Episodic (Events)"),
    Semantic    UMETA(DisplayName = "Semantic (Facts)"),
    Procedural  UMETA(DisplayName = "Procedural (Skills)"),
    Working     UMETA(DisplayName = "Working (Active)")
};

/**
 * Memory encoding strength
 */
UENUM(BlueprintType)
enum class EEncodingStrength : uint8
{
    Weak        UMETA(DisplayName = "Weak"),
    Moderate    UMETA(DisplayName = "Moderate"),
    Strong      UMETA(DisplayName = "Strong"),
    Consolidated UMETA(DisplayName = "Consolidated")
};

/**
 * Retrieval cue type
 */
UENUM(BlueprintType)
enum class ERetrievalCueType : uint8
{
    Temporal    UMETA(DisplayName = "Temporal (When)"),
    Spatial     UMETA(DisplayName = "Spatial (Where)"),
    Semantic    UMETA(DisplayName = "Semantic (What)"),
    Emotional   UMETA(DisplayName = "Emotional (How Felt)"),
    Contextual  UMETA(DisplayName = "Contextual (With Whom)")
};

/**
 * Episodic memory trace
 */
USTRUCT(BlueprintType)
struct FEpisodicMemory
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MemoryID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector SpatialContext = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Participants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArousal = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SemanticTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EEncodingStrength Strength = EEncodingStrength::Moderate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RetrievalCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastRetrievalTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> AssociatedMemories;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Vividness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsConsolidated = false;
};

/**
 * Semantic concept node
 */
USTRUCT(BlueprintType)
struct FSemanticConcept
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConceptID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ConceptName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Definition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Categories;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> Properties;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RelatedConcepts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Relations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaselineActivation = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AccessCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastAccessTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> EpisodicLinks;
};

/**
 * Working memory item
 */
USTRUCT(BlueprintType)
struct FWorkingMemoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMemoryType SourceType = EMemoryType::Working;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SourceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationLevel = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EntryTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RefreshCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Priority = 0.5f;
};

/**
 * Retrieval result
 */
USTRUCT(BlueprintType)
struct FRetrievalResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MemoryID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMemoryType MemoryType = EMemoryType::Episodic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MatchScore = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> MatchedCues;
};

/**
 * Memory consolidation event
 */
USTRUCT(BlueprintType)
struct FConsolidationEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MemoryID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMemoryType MemoryType = EMemoryType::Episodic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ConsolidationTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StrengthIncrease = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> NewAssociations;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMemoryEncoded, const FEpisodicMemory&, Memory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConceptLearned, const FSemanticConcept&, Concept);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMemoryRetrieved, const FRetrievalResult&, Result, ERetrievalCueType, CueType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConsolidationComplete, const FConsolidationEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorkingMemoryFull, int32, ItemCount);

/**
 * Memory Systems Component
 * Implements episodic, semantic, procedural, and working memory
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UMemorySystems : public UActorComponent
{
    GENERATED_BODY()

public:
    UMemorySystems();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxEpisodicMemories = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxSemanticConcepts = 500;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 WorkingMemoryCapacity = 7;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MemoryDecayRate = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ConsolidationInterval = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ActivationSpreadRate = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float RetrievalThreshold = 0.3f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMemoryEncoded OnMemoryEncoded;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnConceptLearned OnConceptLearned;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMemoryRetrieved OnMemoryRetrieved;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnConsolidationComplete OnConsolidationComplete;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWorkingMemoryFull OnWorkingMemoryFull;

    // ========================================
    // EPISODIC MEMORY
    // ========================================

    /** Encode a new episodic memory */
    UFUNCTION(BlueprintCallable, Category = "Episodic Memory")
    FEpisodicMemory EncodeEpisode(const FString& Description, FVector Location, 
                                   float EmotionalValence, float EmotionalArousal,
                                   const TArray<FString>& Tags);

    /** Retrieve episodic memories by cue */
    UFUNCTION(BlueprintCallable, Category = "Episodic Memory")
    TArray<FRetrievalResult> RetrieveEpisodes(ERetrievalCueType CueType, 
                                               const FString& CueValue, int32 MaxResults);

    /** Get episodic memory by ID */
    UFUNCTION(BlueprintPure, Category = "Episodic Memory")
    FEpisodicMemory GetEpisode(const FString& MemoryID) const;

    /** Get recent episodes */
    UFUNCTION(BlueprintPure, Category = "Episodic Memory")
    TArray<FEpisodicMemory> GetRecentEpisodes(int32 Count) const;

    /** Get emotionally significant episodes */
    UFUNCTION(BlueprintPure, Category = "Episodic Memory")
    TArray<FEpisodicMemory> GetEmotionalEpisodes(float MinArousal) const;

    /** Associate two episodic memories */
    UFUNCTION(BlueprintCallable, Category = "Episodic Memory")
    void AssociateEpisodes(const FString& MemoryID1, const FString& MemoryID2);

    // ========================================
    // SEMANTIC MEMORY
    // ========================================

    /** Learn a new semantic concept */
    UFUNCTION(BlueprintCallable, Category = "Semantic Memory")
    FSemanticConcept LearnConcept(const FString& Name, const FString& Definition,
                                   const TArray<FString>& Categories);

    /** Add property to concept */
    UFUNCTION(BlueprintCallable, Category = "Semantic Memory")
    void AddConceptProperty(const FString& ConceptID, const FString& Property, float Value);

    /** Add relation between concepts */
    UFUNCTION(BlueprintCallable, Category = "Semantic Memory")
    void AddConceptRelation(const FString& ConceptID1, const FString& Relation, 
                            const FString& ConceptID2);

    /** Get concept by ID */
    UFUNCTION(BlueprintPure, Category = "Semantic Memory")
    FSemanticConcept GetConcept(const FString& ConceptID) const;

    /** Find concepts by category */
    UFUNCTION(BlueprintPure, Category = "Semantic Memory")
    TArray<FSemanticConcept> FindConceptsByCategory(const FString& Category) const;

    /** Spread activation from concept */
    UFUNCTION(BlueprintCallable, Category = "Semantic Memory")
    void SpreadActivation(const FString& ConceptID, float ActivationAmount);

    /** Get most activated concepts */
    UFUNCTION(BlueprintPure, Category = "Semantic Memory")
    TArray<FSemanticConcept> GetActivatedConcepts(int32 Count) const;

    // ========================================
    // WORKING MEMORY
    // ========================================

    /** Add item to working memory */
    UFUNCTION(BlueprintCallable, Category = "Working Memory")
    bool AddToWorkingMemory(const FString& Content, EMemoryType SourceType, 
                            const FString& SourceID, float Priority);

    /** Refresh working memory item */
    UFUNCTION(BlueprintCallable, Category = "Working Memory")
    void RefreshWorkingMemory(const FString& ItemID);

    /** Get working memory contents */
    UFUNCTION(BlueprintPure, Category = "Working Memory")
    TArray<FWorkingMemoryItem> GetWorkingMemory() const;

    /** Clear working memory */
    UFUNCTION(BlueprintCallable, Category = "Working Memory")
    void ClearWorkingMemory();

    /** Get working memory utilization */
    UFUNCTION(BlueprintPure, Category = "Working Memory")
    float GetWorkingMemoryUtilization() const;

    // ========================================
    // MEMORY CONSOLIDATION
    // ========================================

    /** Trigger memory consolidation */
    UFUNCTION(BlueprintCallable, Category = "Consolidation")
    void TriggerConsolidation();

    /** Consolidate specific memory */
    UFUNCTION(BlueprintCallable, Category = "Consolidation")
    void ConsolidateMemory(const FString& MemoryID, EMemoryType MemoryType);

    /** Get consolidation status */
    UFUNCTION(BlueprintPure, Category = "Consolidation")
    float GetConsolidationProgress() const;

    // ========================================
    // MEMORY STATISTICS
    // ========================================

    /** Get total episodic memory count */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    int32 GetEpisodicMemoryCount() const;

    /** Get total semantic concept count */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    int32 GetSemanticConceptCount() const;

    /** Get average memory strength */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    float GetAverageMemoryStrength() const;

    /** Get memory system health */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    float GetMemorySystemHealth() const;

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    URelevanceRealizationEnnead* EnneadComponent;

    // Memory stores
    TArray<FEpisodicMemory> EpisodicMemories;
    TMap<FString, FSemanticConcept> SemanticConcepts;
    TArray<FWorkingMemoryItem> WorkingMemoryItems;

    // Consolidation state
    float ConsolidationTimer = 0.0f;
    float ConsolidationProgress = 0.0f;
    bool bIsConsolidating = false;

    // ID counters
    int32 EpisodeIDCounter = 0;
    int32 ConceptIDCounter = 0;
    int32 WorkingItemIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeMemorySystems();

    void UpdateMemoryDecay(float DeltaTime);
    void UpdateWorkingMemory(float DeltaTime);
    void UpdateActivationLevels(float DeltaTime);
    void PerformConsolidation();

    float ComputeRetrievalStrength(const FEpisodicMemory& Memory) const;
    float ComputeMatchScore(const FEpisodicMemory& Memory, ERetrievalCueType CueType, 
                            const FString& CueValue) const;
    float ComputeConceptSimilarity(const FSemanticConcept& Concept1, 
                                    const FSemanticConcept& Concept2) const;

    void LinkEpisodeToSemantic(const FString& EpisodeID, const FString& ConceptID);
    void ExtractSemanticFromEpisode(const FEpisodicMemory& Episode);

    FString GenerateEpisodeID();
    FString GenerateConceptID();
    FString GenerateWorkingItemID();

    void PruneOldMemories();
    void EvictWorkingMemoryItem();
};

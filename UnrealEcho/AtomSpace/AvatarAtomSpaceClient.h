#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AvatarAtomSpaceClient.generated.h"

// Forward declarations
class UAvatar9PServer;
class UDiaryInsightBlogLoop;

/**
 * Atom Types following OpenCog AtomSpace conventions
 */
UENUM(BlueprintType)
enum class EAtomType : uint8
{
    // Node types
    ConceptNode UMETA(DisplayName = "ConceptNode"),
    PredicateNode UMETA(DisplayName = "PredicateNode"),
    NumberNode UMETA(DisplayName = "NumberNode"),
    TypeNode UMETA(DisplayName = "TypeNode"),
    VariableNode UMETA(DisplayName = "VariableNode"),
    AnchorNode UMETA(DisplayName = "AnchorNode"),

    // Link types
    InheritanceLink UMETA(DisplayName = "InheritanceLink"),
    EvaluationLink UMETA(DisplayName = "EvaluationLink"),
    ListLink UMETA(DisplayName = "ListLink"),
    MemberLink UMETA(DisplayName = "MemberLink"),
    ContextLink UMETA(DisplayName = "ContextLink"),
    ImplicationLink UMETA(DisplayName = "ImplicationLink"),
    EquivalenceLink UMETA(DisplayName = "EquivalenceLink"),
    ExecutionLink UMETA(DisplayName = "ExecutionLink"),
    StateLink UMETA(DisplayName = "StateLink"),
    AtTimeLink UMETA(DisplayName = "AtTimeLink"),

    // Custom avatar-specific types
    DiaryEntryNode UMETA(DisplayName = "DiaryEntryNode"),
    InsightNode UMETA(DisplayName = "InsightNode"),
    EmotionalStateNode UMETA(DisplayName = "EmotionalStateNode"),
    PersonalityTraitNode UMETA(DisplayName = "PersonalityTraitNode"),
    MemoryNode UMETA(DisplayName = "MemoryNode"),
    EchoResonanceLink UMETA(DisplayName = "EchoResonanceLink")
};

/**
 * TruthValue representing confidence/strength of knowledge
 */
USTRUCT(BlueprintType)
struct FTruthValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength; // Probability of truth (0-1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence; // Certainty of the strength (0-1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count; // Number of observations

    FTruthValue()
        : Strength(1.0f)
        , Confidence(0.9f)
        , Count(1)
    {}

    FTruthValue(float InStrength, float InConfidence, int32 InCount = 1)
        : Strength(InStrength)
        , Confidence(InConfidence)
        , Count(InCount)
    {}
};

/**
 * AttentionValue representing importance/salience
 */
USTRUCT(BlueprintType)
struct FAttentionValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int16 STI; // Short-Term Importance (-32768 to 32767)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int16 LTI; // Long-Term Importance

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int16 VLTI; // Very Long-Term Importance

    FAttentionValue()
        : STI(0)
        , LTI(0)
        , VLTI(0)
    {}
};

/**
 * Atom Handle - unique identifier for atoms in the space
 */
USTRUCT(BlueprintType)
struct FAtomHandle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint64 Handle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsValid;

    FAtomHandle()
        : Handle(0)
        , bIsValid(false)
    {}

    FAtomHandle(uint64 InHandle)
        : Handle(InHandle)
        , bIsValid(true)
    {}
};

/**
 * Atom structure representing nodes and links
 */
USTRUCT(BlueprintType)
struct FAtom
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAtomHandle Handle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAtomType Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTruthValue TruthValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAttentionValue AttentionValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FAtomHandle> Outgoing; // For links, the atoms they connect

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime CreatedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime ModifiedAt;

    FAtom()
        : Type(EAtomType::ConceptNode)
        , CreatedAt(FDateTime::Now())
        , ModifiedAt(FDateTime::Now())
    {}
};

/**
 * Diary Entry structure for narrative storage
 */
USTRUCT(BlueprintType)
struct FDiaryEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EntryId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EmotionalContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArousal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RelatedMemoryIds;

    FDiaryEntry()
        : Timestamp(FDateTime::Now())
        , EmotionalValence(0.0f)
        , EmotionalArousal(0.5f)
    {}
};

/**
 * Insight structure for knowledge representation
 */
USTRUCT(BlueprintType)
struct FInsight
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InsightId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime DiscoveredAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SourceDiaryEntryIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsValidated;

    FInsight()
        : Confidence(0.5f)
        , DiscoveredAt(FDateTime::Now())
        , bIsValidated(false)
    {}
};

/**
 * Memory query parameters
 */
USTRUCT(BlueprintType)
struct FMemoryQuery
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EAtomType> AtomTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NamePattern;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinStrength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinConfidence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinSTI;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime FromTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime ToTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxResults;

    FMemoryQuery()
        : MinStrength(0.0f)
        , MinConfidence(0.0f)
        , MinSTI(INT32_MIN)
        , FromTime(FDateTime::MinValue())
        , ToTime(FDateTime::MaxValue())
        , MaxResults(100)
    {}
};

/**
 * Memory structure for retrieved memories
 */
USTRUCT(BlueprintType)
struct FMemory
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MemoryId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MemoryType; // episodic, semantic, procedural, emotional

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Relevance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Context;

    FMemory()
        : Timestamp(FDateTime::Now())
        , Relevance(1.0f)
        , Strength(1.0f)
    {}
};

/**
 * Delegates for AtomSpace events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtomCreated, const FAtomHandle&, Handle, const FAtom&, Atom);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtomModified, const FAtomHandle&, Handle, const FAtom&, Atom);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAtomDeleted, const FAtomHandle&, Handle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionStateChanged, bool, bConnected);

/**
 * Avatar AtomSpace Client Component
 * Provides interface to OpenCog AtomSpace for knowledge storage and retrieval
 *
 * AtomSpace Representation:
 *
 * ; Avatar identity
 * (ConceptNode "deep_tree_echo")
 *
 * ; Diary entry
 * (EvaluationLink
 *     (PredicateNode "diary_entry")
 *     (ListLink
 *         (ConceptNode "deep_tree_echo")
 *         (ConceptNode "2025-12-13_entry_001")
 *         (ConceptNode "emotional_state_happy")))
 *
 * ; Insight
 * (InheritanceLink
 *     (ConceptNode "pattern_playfulness_increases_with_dopamine")
 *     (ConceptNode "behavioral_insight"))
 *
 * ; Personality trait
 * (EvaluationLink
 *     (PredicateNode "has_trait")
 *     (ListLink
 *         (ConceptNode "deep_tree_echo")
 *         (ConceptNode "confidence")
 *         (NumberNode 0.85)))
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAvatarAtomSpaceClient : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarAtomSpaceClient();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Connection Management =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Connection")
    void Connect(const FString& Host = TEXT("localhost"), int32 Port = 17001);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Connection")
    void Disconnect();

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Connection")
    bool IsConnected() const { return bConnected; }

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Connection")
    void SetConnectionVia9P(bool bUse9P);

    // ===== Atom Operations =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Atoms")
    FAtomHandle CreateNode(EAtomType Type, const FString& Name, const FTruthValue& TV);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Atoms")
    FAtomHandle CreateLink(EAtomType Type, const TArray<FAtomHandle>& Outgoing, const FTruthValue& TV);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Atoms")
    FAtom GetAtom(const FAtomHandle& Handle);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Atoms")
    bool DeleteAtom(const FAtomHandle& Handle);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Atoms")
    void SetTruthValue(const FAtomHandle& Handle, const FTruthValue& TV);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Atoms")
    void SetAttentionValue(const FAtomHandle& Handle, const FAttentionValue& AV);

    // ===== Diary Storage =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Diary")
    void StoreDiaryEntry(const FDiaryEntry& Entry);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Diary")
    FDiaryEntry GetDiaryEntry(const FString& EntryId);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Diary")
    TArray<FDiaryEntry> GetRecentDiaryEntries(int32 Count = 10);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Diary")
    TArray<FDiaryEntry> SearchDiaryEntries(const FString& Query, int32 MaxResults = 20);

    // ===== Insight Storage =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Insights")
    void StoreInsight(const FInsight& Insight);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Insights")
    FInsight GetInsight(const FString& InsightId);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Insights")
    TArray<FInsight> GetRelatedInsights(const FString& Context, int32 MaxResults = 10);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Insights")
    void ValidateInsight(const FString& InsightId, bool bValid);

    // ===== Personality Trait Storage =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Personality")
    void StorePersonalityTrait(const FString& TraitName, float Value, float Confidence = 0.9f);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Personality")
    float GetPersonalityTrait(const FString& TraitName);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Personality")
    TMap<FString, float> GetAllPersonalityTraits();

    // ===== Memory Queries =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Memory")
    TArray<FMemory> QueryRelatedMemories(const FString& Context, int32 MaxResults = 10);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Memory")
    TArray<FAtom> QueryAtoms(const FMemoryQuery& Query);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Memory")
    TArray<FAtomHandle> FindByName(const FString& NamePattern);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Memory")
    TArray<FAtomHandle> FindByType(EAtomType Type);

    // ===== Pattern Mining =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Patterns")
    TArray<FInsight> MinePatterns(int32 MinSupport = 2);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Patterns")
    void DiscoverCorrelations(const FString& ConceptA, const FString& ConceptB);

    // ===== Echo Resonance Integration =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Echo")
    void StoreEchoResonance(const FString& SourceId, const FString& TargetId, float Resonance);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Echo")
    float GetEchoResonance(const FString& SourceId, const FString& TargetId);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Echo")
    TArray<FString> GetResonantMemories(const FString& CurrentContext, float MinResonance = 0.5f);

    // ===== Synchronization =====

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Sync")
    void SyncWithServer();

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Sync")
    void EnableAutoSync(bool bEnable, float IntervalSeconds = 5.0f);

    UFUNCTION(BlueprintCallable, Category = "AtomSpace|Sync")
    int32 GetPendingChangeCount() const { return PendingChanges.Num(); }

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "AtomSpace|Events")
    FOnAtomCreated OnAtomCreated;

    UPROPERTY(BlueprintAssignable, Category = "AtomSpace|Events")
    FOnAtomModified OnAtomModified;

    UPROPERTY(BlueprintAssignable, Category = "AtomSpace|Events")
    FOnAtomDeleted OnAtomDeleted;

    UPROPERTY(BlueprintAssignable, Category = "AtomSpace|Events")
    FOnConnectionStateChanged OnConnectionStateChanged;

protected:
    // ===== Internal Methods =====

    void InitializeAvatarConcept();
    FString AtomToScheme(const FAtom& Atom);
    FAtom SchemeToAtom(const FString& Scheme);
    void ProcessPendingChanges();
    FString GenerateUniqueId();

    // 9P-based communication
    FString Read9PPath(const FString& Path);
    bool Write9PPath(const FString& Path, const FString& Data);

private:
    // ===== Connection State =====

    UPROPERTY()
    bool bConnected;

    UPROPERTY()
    FString ServerHost;

    UPROPERTY()
    int32 ServerPort;

    UPROPERTY()
    bool bUse9PConnection;

    UPROPERTY()
    UAvatar9PServer* P9Server;

    // ===== Local AtomSpace Cache =====

    UPROPERTY()
    TMap<uint64, FAtom> AtomCache;

    UPROPERTY()
    uint64 NextHandle;

    UPROPERTY()
    FAtomHandle AvatarConceptHandle;

    // ===== Synchronization =====

    UPROPERTY()
    bool bAutoSyncEnabled;

    UPROPERTY()
    float SyncInterval;

    UPROPERTY()
    float SyncTimer;

    UPROPERTY()
    TArray<FAtom> PendingChanges;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "AtomSpace|Configuration")
    FString AvatarName;

    UPROPERTY(EditAnywhere, Category = "AtomSpace|Configuration")
    int32 MaxCacheSize;

    UPROPERTY(EditAnywhere, Category = "AtomSpace|Configuration")
    bool bVerboseLogging;
};

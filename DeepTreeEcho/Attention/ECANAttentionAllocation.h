// ECANAttentionAllocation.h
// Economic Attention Networks (ECAN) for importance-based resource allocation
// Implements OpenCog-style attention value dynamics with STI/LTI/VLTI
// Feature F1.4.3 - Hypergraph Memory System / Attention Allocation

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ECANAttentionAllocation.generated.h"

// Forward declarations
class UAttentionSystem;
class UAvatarAtomSpaceClient;
class UCognitiveCycleManager;

/**
 * Attention Focus Level - determines atom placement in focus hierarchy
 */
UENUM(BlueprintType)
enum class EAttentionFocusLevel : uint8
{
    AttentionalFocus    UMETA(DisplayName = "Attentional Focus (Top STI)"),
    ActiveSet           UMETA(DisplayName = "Active Set (High STI)"),
    PassiveSet          UMETA(DisplayName = "Passive Set (Moderate STI)"),
    InactiveSet         UMETA(DisplayName = "Inactive Set (Low STI)"),
    ForgetSet           UMETA(DisplayName = "Forget Set (Below Threshold)")
};

/**
 * ECAN Spread Type - how attention spreads through the hypergraph
 */
UENUM(BlueprintType)
enum class EECANSpreadType : uint8
{
    HebbianLink         UMETA(DisplayName = "Hebbian (Co-activation)"),
    ImportanceLink      UMETA(DisplayName = "Importance (Direct)"),
    ContextualLink      UMETA(DisplayName = "Contextual (Associative)"),
    TemporalLink        UMETA(DisplayName = "Temporal (Sequential)")
};

/**
 * Attention Value - OpenCog-compatible importance metrics
 */
USTRUCT(BlueprintType)
struct FECANAttentionValue
{
    GENERATED_BODY()

    /** Short-Term Importance: immediate relevance, high volatility */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float STI = 0.0f;

    /** Long-Term Importance: persistent relevance, slow decay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LTI = 0.0f;

    /** Very Long-Term Importance: permanent relevance, protected from forgetting */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VLTI = 0.0f;

    /** Last update timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastUpdateTime = 0.0f;

    /** Current focus level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAttentionFocusLevel FocusLevel = EAttentionFocusLevel::InactiveSet;

    /** Constructor */
    FECANAttentionValue()
        : STI(0.0f), LTI(0.0f), VLTI(0.0f), LastUpdateTime(0.0f),
          FocusLevel(EAttentionFocusLevel::InactiveSet)
    {}

    FECANAttentionValue(float InSTI, float InLTI, float InVLTI = 0.0f)
        : STI(InSTI), LTI(InLTI), VLTI(InVLTI), LastUpdateTime(0.0f),
          FocusLevel(EAttentionFocusLevel::InactiveSet)
    {}

    /** Compute total importance (weighted sum) */
    float GetTotalImportance(float STIWeight = 0.6f, float LTIWeight = 0.3f, float VLTIWeight = 0.1f) const
    {
        return STI * STIWeight + LTI * LTIWeight + VLTI * VLTIWeight;
    }
};

/**
 * ECAN Atom - represents an attention-managed entity
 */
USTRUCT(BlueprintType)
struct FECANAtom
{
    GENERATED_BODY()

    /** Unique identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AtomID;

    /** Human-readable name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    /** Atom type (ConceptNode, PredicateNode, Link, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AtomType = TEXT("ConceptNode");

    /** Attention value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FECANAttentionValue AttentionValue;

    /** Outgoing atoms (for links) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> OutgoingAtomIDs;

    /** Incoming atoms (backlinks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> IncomingAtomIDs;

    /** Associated semantic tags */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;

    /** Creation timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CreatedAt = 0.0f;

    /** Last access timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastAccessedAt = 0.0f;

    /** Access count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AccessCount = 0;
};

/**
 * Hebbian Link - represents co-activation strength between atoms
 */
USTRUCT(BlueprintType)
struct FHebbianLink
{
    GENERATED_BODY()

    /** Source atom ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SourceAtomID;

    /** Target atom ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetAtomID;

    /** Link strength (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength = 0.0f;

    /** Co-activation count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CoActivationCount = 0;

    /** Last update time */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastUpdateTime = 0.0f;
};

/**
 * ECAN Configuration - tunable parameters for attention dynamics
 */
USTRUCT(BlueprintType)
struct FECANConfig
{
    GENERATED_BODY()

    // === Focus Thresholds ===

    /** STI threshold for Attentional Focus */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionalFocusThreshold = 80.0f;

    /** STI threshold for Active Set */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActiveSetThreshold = 50.0f;

    /** STI threshold for Passive Set */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PassiveSetThreshold = 20.0f;

    /** STI below which atoms enter Forget Set */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ForgetThreshold = -50.0f;

    // === Economic Parameters ===

    /** Maximum STI value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSTI = 100.0f;

    /** Minimum STI value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinSTI = -100.0f;

    /** Total STI funds in the system (economic constraint) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TotalSTIFunds = 10000.0f;

    /** STI rent rate - atoms pay rent proportional to STI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float STIRentRate = 0.01f;

    /** LTI wage rate - atoms earn LTI proportional to usage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LTIWageRate = 0.1f;

    // === Decay Parameters ===

    /** STI decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float STIDecayRate = 0.1f;

    /** LTI decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LTIDecayRate = 0.01f;

    /** VLTI decay rate per second (very slow) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VLTIDecayRate = 0.001f;

    // === Spreading Parameters ===

    /** Maximum atoms to spread attention to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxSpreadTargets = 10;

    /** Spreading activation fraction (0.0 to 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpreadingFraction = 0.3f;

    /** Minimum link strength for spreading */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinSpreadLinkStrength = 0.1f;

    // === Hebbian Learning ===

    /** Hebbian link learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HebbianLearningRate = 0.1f;

    /** Hebbian link decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HebbianDecayRate = 0.01f;

    /** Co-activation time window (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CoActivationWindow = 1.0f;

    // === Focus Set Limits ===

    /** Maximum atoms in Attentional Focus */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxAttentionalFocusSize = 10;

    /** Maximum atoms in Active Set */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxActiveSetSize = 100;

    /** Maximum total atoms managed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxTotalAtoms = 10000;
};

/**
 * ECAN Statistics - runtime metrics
 */
USTRUCT(BlueprintType)
struct FECANStatistics
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TotalAtomCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AttentionalFocusCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ActiveSetCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PassiveSetCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 InactiveSetCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ForgetSetCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TotalSTI = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TotalLTI = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AverageSTI = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AverageLTI = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HebbianLinkCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SpreadingEventsThisCycle = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ForgettingEventsThisCycle = 0;
};

/**
 * Attention Spreading Event
 */
USTRUCT(BlueprintType)
struct FAttentionSpreadEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SourceAtomID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetAtomID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpreadAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EECANSpreadType SpreadType = EECANSpreadType::ImportanceLink;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtomFocusChanged, const FString&, AtomID, EAttentionFocusLevel, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAtomForgotten, const FString&, AtomID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSTIStimulus, const FString&, AtomID, float, STIChange);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttentionalFocusChanged, const TArray<FString>&, FocusAtomIDs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHebbianLinkCreated, const FHebbianLink&, Link);

/**
 * ECAN Attention Allocation Component
 * Implements Economic Attention Networks for importance-based resource allocation
 * 
 * Based on OpenCog ECAN principles:
 * - STI (Short-Term Importance): immediate relevance, rapid change
 * - LTI (Long-Term Importance): persistent value, slow accumulation
 * - VLTI (Very Long-Term Importance): permanent importance, protected
 * 
 * Economic metaphor:
 * - STI represents "attention currency" - finite resource
 * - Atoms pay "rent" based on STI holdings
 * - Atoms earn "wages" (LTI) based on usefulness
 * - Spreading activation transfers STI through links
 * - Hebbian learning strengthens co-activation pathways
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UECANAttentionAllocation : public UActorComponent
{
    GENERATED_BODY()

public:
    UECANAttentionAllocation();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ECAN|Configuration")
    FECANConfig Config;

    /** Enable automatic attention dynamics */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ECAN|Configuration")
    bool bEnableAutoDynamics = true;

    /** Enable Hebbian learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ECAN|Configuration")
    bool bEnableHebbianLearning = true;

    /** Enable spreading activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ECAN|Configuration")
    bool bEnableSpreadingActivation = true;

    /** Enable economic rent/wage dynamics */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ECAN|Configuration")
    bool bEnableEconomicDynamics = true;

    /** Enable forgetting (atoms below threshold removed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ECAN|Configuration")
    bool bEnableForgetting = true;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "ECAN|Events")
    FOnAtomFocusChanged OnAtomFocusChanged;

    UPROPERTY(BlueprintAssignable, Category = "ECAN|Events")
    FOnAtomForgotten OnAtomForgotten;

    UPROPERTY(BlueprintAssignable, Category = "ECAN|Events")
    FOnSTIStimulus OnSTIStimulus;

    UPROPERTY(BlueprintAssignable, Category = "ECAN|Events")
    FOnAttentionalFocusChanged OnAttentionalFocusChanged;

    UPROPERTY(BlueprintAssignable, Category = "ECAN|Events")
    FOnHebbianLinkCreated OnHebbianLinkCreated;

    // ========================================
    // ATOM MANAGEMENT
    // ========================================

    /** Create a new ECAN atom */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Atoms")
    FString CreateAtom(const FString& Name, const FString& AtomType,
                       float InitialSTI = 0.0f, float InitialLTI = 0.0f,
                       const TArray<FString>& Tags = TArray<FString>());

    /** Get atom by ID */
    UFUNCTION(BlueprintPure, Category = "ECAN|Atoms")
    FECANAtom GetAtom(const FString& AtomID) const;

    /** Check if atom exists */
    UFUNCTION(BlueprintPure, Category = "ECAN|Atoms")
    bool AtomExists(const FString& AtomID) const;

    /** Remove atom from ECAN */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Atoms")
    bool RemoveAtom(const FString& AtomID);

    /** Get all atoms */
    UFUNCTION(BlueprintPure, Category = "ECAN|Atoms")
    TArray<FECANAtom> GetAllAtoms() const;

    /** Get atoms by focus level */
    UFUNCTION(BlueprintPure, Category = "ECAN|Atoms")
    TArray<FECANAtom> GetAtomsByFocusLevel(EAttentionFocusLevel Level) const;

    /** Get atoms in Attentional Focus */
    UFUNCTION(BlueprintPure, Category = "ECAN|Atoms")
    TArray<FECANAtom> GetAttentionalFocus() const;

    /** Get atoms in Active Set */
    UFUNCTION(BlueprintPure, Category = "ECAN|Atoms")
    TArray<FECANAtom> GetActiveSet() const;

    /** Add link between atoms */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Atoms")
    void AddAtomLink(const FString& SourceID, const FString& TargetID);

    // ========================================
    // ATTENTION VALUE MANAGEMENT
    // ========================================

    /** Get attention value for atom */
    UFUNCTION(BlueprintPure, Category = "ECAN|AttentionValue")
    FECANAttentionValue GetAttentionValue(const FString& AtomID) const;

    /** Set attention value for atom */
    UFUNCTION(BlueprintCallable, Category = "ECAN|AttentionValue")
    void SetAttentionValue(const FString& AtomID, const FECANAttentionValue& Value);

    /** Stimulate STI for atom (add attention) */
    UFUNCTION(BlueprintCallable, Category = "ECAN|AttentionValue")
    void StimulateSTI(const FString& AtomID, float Amount);

    /** Stimulate LTI for atom */
    UFUNCTION(BlueprintCallable, Category = "ECAN|AttentionValue")
    void StimulateLTI(const FString& AtomID, float Amount);

    /** Set VLTI for atom (protected importance) */
    UFUNCTION(BlueprintCallable, Category = "ECAN|AttentionValue")
    void SetVLTI(const FString& AtomID, float Value);

    /** Transfer STI from one atom to another */
    UFUNCTION(BlueprintCallable, Category = "ECAN|AttentionValue")
    void TransferSTI(const FString& SourceID, const FString& TargetID, float Amount);

    /** Get current focus level for atom */
    UFUNCTION(BlueprintPure, Category = "ECAN|AttentionValue")
    EAttentionFocusLevel GetFocusLevel(const FString& AtomID) const;

    // ========================================
    // SPREADING ACTIVATION
    // ========================================

    /** Spread attention from an atom to its linked neighbors */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Spreading")
    void SpreadAttention(const FString& SourceAtomID);

    /** Spread attention from all atoms in Attentional Focus */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Spreading")
    void SpreadFromFocus();

    /** Get recent spreading events */
    UFUNCTION(BlueprintPure, Category = "ECAN|Spreading")
    TArray<FAttentionSpreadEvent> GetRecentSpreadEvents(int32 Count = 10) const;

    // ========================================
    // HEBBIAN LEARNING
    // ========================================

    /** Record co-activation of atoms (strengthens Hebbian links) */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Hebbian")
    void RecordCoActivation(const FString& AtomID1, const FString& AtomID2);

    /** Get Hebbian link strength between atoms */
    UFUNCTION(BlueprintPure, Category = "ECAN|Hebbian")
    float GetHebbianLinkStrength(const FString& AtomID1, const FString& AtomID2) const;

    /** Get all Hebbian links for an atom */
    UFUNCTION(BlueprintPure, Category = "ECAN|Hebbian")
    TArray<FHebbianLink> GetHebbianLinks(const FString& AtomID) const;

    /** Get all Hebbian links in the system */
    UFUNCTION(BlueprintPure, Category = "ECAN|Hebbian")
    TArray<FHebbianLink> GetAllHebbianLinks() const;

    // ========================================
    // ECONOMIC DYNAMICS
    // ========================================

    /** Apply rent collection (STI decay based on holdings) */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Economic")
    void CollectRent();

    /** Award wages (LTI increase based on usage) */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Economic")
    void AwardWages(const FString& AtomID, float UsageScore);

    /** Get total STI in the system */
    UFUNCTION(BlueprintPure, Category = "ECAN|Economic")
    float GetTotalSystemSTI() const;

    /** Normalize STI to maintain economic balance */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Economic")
    void NormalizeSTI();

    // ========================================
    // FORGETTING
    // ========================================

    /** Process forgetting (remove atoms below threshold) */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Forgetting")
    TArray<FString> ProcessForgetting();

    /** Check if atom is a forgetting candidate */
    UFUNCTION(BlueprintPure, Category = "ECAN|Forgetting")
    bool IsForgettingCandidate(const FString& AtomID) const;

    /** Protect atom from forgetting (set VLTI) */
    UFUNCTION(BlueprintCallable, Category = "ECAN|Forgetting")
    void ProtectFromForgetting(const FString& AtomID);

    // ========================================
    // STATISTICS & QUERIES
    // ========================================

    /** Get ECAN statistics */
    UFUNCTION(BlueprintPure, Category = "ECAN|Statistics")
    FECANStatistics GetStatistics() const;

    /** Get atoms sorted by STI (descending) */
    UFUNCTION(BlueprintPure, Category = "ECAN|Queries")
    TArray<FECANAtom> GetTopAtomsBySTI(int32 Count = 10) const;

    /** Get atoms sorted by LTI (descending) */
    UFUNCTION(BlueprintPure, Category = "ECAN|Queries")
    TArray<FECANAtom> GetTopAtomsByLTI(int32 Count = 10) const;

    /** Find atoms by tag */
    UFUNCTION(BlueprintPure, Category = "ECAN|Queries")
    TArray<FECANAtom> FindAtomsByTag(const FString& Tag) const;

    /** Get atoms co-active with given atom (Hebbian neighbors) */
    UFUNCTION(BlueprintPure, Category = "ECAN|Queries")
    TArray<FECANAtom> GetCoActiveAtoms(const FString& AtomID, float MinLinkStrength = 0.1f) const;

protected:
    // Component references
    UPROPERTY()
    UAttentionSystem* AttentionSystem;

    UPROPERTY()
    UAvatarAtomSpaceClient* AtomSpaceClient;

    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    // Internal state
    TMap<FString, FECANAtom> Atoms;
    TMap<FString, FHebbianLink> HebbianLinks;
    TArray<FAttentionSpreadEvent> SpreadHistory;
    TArray<FString> RecentActivations;

    int32 AtomIDCounter = 0;
    float LastDecayTime = 0.0f;
    float LastRentTime = 0.0f;
    float LastForgetTime = 0.0f;
    int32 SpreadingEventsThisCycle = 0;
    int32 ForgettingEventsThisCycle = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeECAN();

    void UpdateAttentionDynamics(float DeltaTime);
    void UpdateSTIDecay(float DeltaTime);
    void UpdateLTIDecay(float DeltaTime);
    void UpdateHebbianLinks(float DeltaTime);
    void UpdateFocusLevels();

    EAttentionFocusLevel ComputeFocusLevel(float STI) const;
    FString GenerateAtomID();
    FString GenerateHebbianLinkKey(const FString& AtomID1, const FString& AtomID2) const;

    void RecordSpreadEvent(const FString& SourceID, const FString& TargetID, 
                           float Amount, EECANSpreadType Type);

    void BroadcastFocusChange(const FString& AtomID, EAttentionFocusLevel OldLevel, 
                              EAttentionFocusLevel NewLevel);
};

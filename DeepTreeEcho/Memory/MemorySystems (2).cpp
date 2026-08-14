// MemorySystems.cpp
// Implementation of Episodic and Semantic Memory Systems

#include "MemorySystems.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Wisdom/RelevanceRealizationEnnead.h"
#include "Math/UnrealMathUtility.h"

UMemorySystems::UMemorySystems()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UMemorySystems::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeMemorySystems();
}

void UMemorySystems::TickComponent(float DeltaTime, ELevelTick TickType, 
                                    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateMemoryDecay(DeltaTime);
    UpdateWorkingMemory(DeltaTime);
    UpdateActivationLevels(DeltaTime);

    // Consolidation timer
    ConsolidationTimer += DeltaTime;
    if (ConsolidationTimer >= ConsolidationInterval)
    {
        ConsolidationTimer = 0.0f;
        TriggerConsolidation();
    }
}

void UMemorySystems::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        EnneadComponent = Owner->FindComponentByClass<URelevanceRealizationEnnead>();
    }
}

void UMemorySystems::InitializeMemorySystems()
{
    EpisodicMemories.Empty();
    SemanticConcepts.Empty();
    WorkingMemoryItems.Empty();

    // Initialize some base semantic concepts
    LearnConcept(TEXT("Self"), TEXT("The cognitive agent itself"), {TEXT("Agent"), TEXT("Identity")});
    LearnConcept(TEXT("World"), TEXT("The external environment"), {TEXT("Environment"), TEXT("Context")});
    LearnConcept(TEXT("Time"), TEXT("Temporal dimension"), {TEXT("Dimension"), TEXT("Abstract")});
    LearnConcept(TEXT("Space"), TEXT("Spatial dimension"), {TEXT("Dimension"), TEXT("Abstract")});
}

// ========================================
// EPISODIC MEMORY
// ========================================

FEpisodicMemory UMemorySystems::EncodeEpisode(const FString& Description, FVector Location, 
                                               float EmotionalValence, float EmotionalArousal,
                                               const TArray<FString>& Tags)
{
    FEpisodicMemory Memory;
    Memory.MemoryID = GenerateEpisodeID();
    Memory.EventDescription = Description;
    Memory.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Memory.SpatialContext = Location;
    Memory.EmotionalValence = FMath::Clamp(EmotionalValence, -1.0f, 1.0f);
    Memory.EmotionalArousal = FMath::Clamp(EmotionalArousal, 0.0f, 1.0f);
    Memory.SemanticTags = Tags;
    Memory.Vividness = 1.0f;

    // Determine encoding strength based on emotional arousal
    if (EmotionalArousal > 0.8f)
    {
        Memory.Strength = EEncodingStrength::Strong;
    }
    else if (EmotionalArousal > 0.5f)
    {
        Memory.Strength = EEncodingStrength::Moderate;
    }
    else
    {
        Memory.Strength = EEncodingStrength::Weak;
    }

    EpisodicMemories.Add(Memory);

    // Extract semantic concepts from episode
    ExtractSemanticFromEpisode(Memory);

    // Prune if over capacity
    PruneOldMemories();

    OnMemoryEncoded.Broadcast(Memory);

    return Memory;
}

TArray<FRetrievalResult> UMemorySystems::RetrieveEpisodes(ERetrievalCueType CueType, 
                                                           const FString& CueValue, int32 MaxResults)
{
    TArray<FRetrievalResult> Results;

    for (FEpisodicMemory& Memory : EpisodicMemories)
    {
        float MatchScore = ComputeMatchScore(Memory, CueType, CueValue);

        if (MatchScore >= RetrievalThreshold)
        {
            FRetrievalResult Result;
            Result.MemoryID = Memory.MemoryID;
            Result.MemoryType = EMemoryType::Episodic;
            Result.Content = Memory.EventDescription;
            Result.MatchScore = MatchScore;
            Result.Confidence = ComputeRetrievalStrength(Memory);
            Result.MatchedCues.Add(CueValue);

            Results.Add(Result);

            // Update retrieval statistics
            Memory.RetrievalCount++;
            Memory.LastRetrievalTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        }
    }

    // Sort by match score
    Results.Sort([](const FRetrievalResult& A, const FRetrievalResult& B) {
        return A.MatchScore > B.MatchScore;
    });

    // Limit results
    if (Results.Num() > MaxResults)
    {
        Results.SetNum(MaxResults);
    }

    // Broadcast retrieval events
    for (const FRetrievalResult& Result : Results)
    {
        OnMemoryRetrieved.Broadcast(Result, CueType);
    }

    return Results;
}

FEpisodicMemory UMemorySystems::GetEpisode(const FString& MemoryID) const
{
    for (const FEpisodicMemory& Memory : EpisodicMemories)
    {
        if (Memory.MemoryID == MemoryID)
        {
            return Memory;
        }
    }
    return FEpisodicMemory();
}

TArray<FEpisodicMemory> UMemorySystems::GetRecentEpisodes(int32 Count) const
{
    TArray<FEpisodicMemory> Recent;
    int32 StartIndex = FMath::Max(0, EpisodicMemories.Num() - Count);
    for (int32 i = StartIndex; i < EpisodicMemories.Num(); ++i)
    {
        Recent.Add(EpisodicMemories[i]);
    }
    return Recent;
}

TArray<FEpisodicMemory> UMemorySystems::GetEmotionalEpisodes(float MinArousal) const
{
    TArray<FEpisodicMemory> Emotional;
    for (const FEpisodicMemory& Memory : EpisodicMemories)
    {
        if (Memory.EmotionalArousal >= MinArousal)
        {
            Emotional.Add(Memory);
        }
    }
    return Emotional;
}

void UMemorySystems::AssociateEpisodes(const FString& MemoryID1, const FString& MemoryID2)
{
    for (FEpisodicMemory& Memory : EpisodicMemories)
    {
        if (Memory.MemoryID == MemoryID1)
        {
            if (!Memory.AssociatedMemories.Contains(MemoryID2))
            {
                Memory.AssociatedMemories.Add(MemoryID2);
            }
        }
        else if (Memory.MemoryID == MemoryID2)
        {
            if (!Memory.AssociatedMemories.Contains(MemoryID1))
            {
                Memory.AssociatedMemories.Add(MemoryID1);
            }
        }
    }
}

// ========================================
// SEMANTIC MEMORY
// ========================================

FSemanticConcept UMemorySystems::LearnConcept(const FString& Name, const FString& Definition,
                                               const TArray<FString>& Categories)
{
    // Check if concept already exists
    for (auto& Pair : SemanticConcepts)
    {
        if (Pair.Value.ConceptName == Name)
        {
            // Update existing concept
            Pair.Value.AccessCount++;
            return Pair.Value;
        }
    }

    FSemanticConcept Concept;
    Concept.ConceptID = GenerateConceptID();
    Concept.ConceptName = Name;
    Concept.Definition = Definition;
    Concept.Categories = Categories;
    Concept.ActivationLevel = 0.5f;
    Concept.BaselineActivation = 0.1f;
    Concept.AccessCount = 1;
    Concept.LastAccessTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    SemanticConcepts.Add(Concept.ConceptID, Concept);

    OnConceptLearned.Broadcast(Concept);

    return Concept;
}

void UMemorySystems::AddConceptProperty(const FString& ConceptID, const FString& Property, float Value)
{
    if (SemanticConcepts.Contains(ConceptID))
    {
        SemanticConcepts[ConceptID].Properties.Add(Property, Value);
    }
}

void UMemorySystems::AddConceptRelation(const FString& ConceptID1, const FString& Relation, 
                                         const FString& ConceptID2)
{
    if (SemanticConcepts.Contains(ConceptID1) && SemanticConcepts.Contains(ConceptID2))
    {
        SemanticConcepts[ConceptID1].Relations.Add(Relation, ConceptID2);
        SemanticConcepts[ConceptID1].RelatedConcepts.AddUnique(ConceptID2);
        SemanticConcepts[ConceptID2].RelatedConcepts.AddUnique(ConceptID1);
    }
}

FSemanticConcept UMemorySystems::GetConcept(const FString& ConceptID) const
{
    if (SemanticConcepts.Contains(ConceptID))
    {
        return SemanticConcepts[ConceptID];
    }
    return FSemanticConcept();
}

TArray<FSemanticConcept> UMemorySystems::FindConceptsByCategory(const FString& Category) const
{
    TArray<FSemanticConcept> Found;
    for (const auto& Pair : SemanticConcepts)
    {
        if (Pair.Value.Categories.Contains(Category))
        {
            Found.Add(Pair.Value);
        }
    }
    return Found;
}

void UMemorySystems::SpreadActivation(const FString& ConceptID, float ActivationAmount)
{
    if (!SemanticConcepts.Contains(ConceptID))
    {
        return;
    }

    FSemanticConcept& SourceConcept = SemanticConcepts[ConceptID];
    SourceConcept.ActivationLevel = FMath::Min(1.0f, SourceConcept.ActivationLevel + ActivationAmount);
    SourceConcept.AccessCount++;
    SourceConcept.LastAccessTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Spread to related concepts
    float SpreadAmount = ActivationAmount * ActivationSpreadRate;
    for (const FString& RelatedID : SourceConcept.RelatedConcepts)
    {
        if (SemanticConcepts.Contains(RelatedID))
        {
            SemanticConcepts[RelatedID].ActivationLevel = 
                FMath::Min(1.0f, SemanticConcepts[RelatedID].ActivationLevel + SpreadAmount);
        }
    }
}

TArray<FSemanticConcept> UMemorySystems::GetActivatedConcepts(int32 Count) const
{
    TArray<FSemanticConcept> Concepts;
    for (const auto& Pair : SemanticConcepts)
    {
        Concepts.Add(Pair.Value);
    }

    // Sort by activation level
    Concepts.Sort([](const FSemanticConcept& A, const FSemanticConcept& B) {
        return A.ActivationLevel > B.ActivationLevel;
    });

    if (Concepts.Num() > Count)
    {
        Concepts.SetNum(Count);
    }

    return Concepts;
}

// ========================================
// WORKING MEMORY
// ========================================

bool UMemorySystems::AddToWorkingMemory(const FString& Content, EMemoryType SourceType, 
                                         const FString& SourceID, float Priority)
{
    // Check capacity
    if (WorkingMemoryItems.Num() >= WorkingMemoryCapacity)
    {
        OnWorkingMemoryFull.Broadcast(WorkingMemoryItems.Num());
        EvictWorkingMemoryItem();
    }

    FWorkingMemoryItem Item;
    Item.ItemID = GenerateWorkingItemID();
    Item.Content = Content;
    Item.SourceType = SourceType;
    Item.SourceID = SourceID;
    Item.ActivationLevel = 1.0f;
    Item.EntryTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Item.Priority = Priority;
    Item.RefreshCount = 0;

    WorkingMemoryItems.Add(Item);

    return true;
}

void UMemorySystems::RefreshWorkingMemory(const FString& ItemID)
{
    for (FWorkingMemoryItem& Item : WorkingMemoryItems)
    {
        if (Item.ItemID == ItemID)
        {
            Item.ActivationLevel = 1.0f;
            Item.RefreshCount++;
            break;
        }
    }
}

TArray<FWorkingMemoryItem> UMemorySystems::GetWorkingMemory() const
{
    return WorkingMemoryItems;
}

void UMemorySystems::ClearWorkingMemory()
{
    WorkingMemoryItems.Empty();
}

float UMemorySystems::GetWorkingMemoryUtilization() const
{
    return static_cast<float>(WorkingMemoryItems.Num()) / WorkingMemoryCapacity;
}

// ========================================
// MEMORY CONSOLIDATION
// ========================================

void UMemorySystems::TriggerConsolidation()
{
    if (bIsConsolidating)
    {
        return;
    }

    bIsConsolidating = true;
    ConsolidationProgress = 0.0f;

    PerformConsolidation();

    bIsConsolidating = false;
    ConsolidationProgress = 1.0f;
}

void UMemorySystems::ConsolidateMemory(const FString& MemoryID, EMemoryType MemoryType)
{
    if (MemoryType == EMemoryType::Episodic)
    {
        for (FEpisodicMemory& Memory : EpisodicMemories)
        {
            if (Memory.MemoryID == MemoryID && !Memory.bIsConsolidated)
            {
                Memory.bIsConsolidated = true;
                Memory.Strength = EEncodingStrength::Consolidated;

                FConsolidationEvent Event;
                Event.MemoryID = MemoryID;
                Event.MemoryType = MemoryType;
                Event.ConsolidationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
                Event.StrengthIncrease = 0.3f;

                OnConsolidationComplete.Broadcast(Event);
                break;
            }
        }
    }
}

float UMemorySystems::GetConsolidationProgress() const
{
    return ConsolidationProgress;
}

// ========================================
// MEMORY STATISTICS
// ========================================

int32 UMemorySystems::GetEpisodicMemoryCount() const
{
    return EpisodicMemories.Num();
}

int32 UMemorySystems::GetSemanticConceptCount() const
{
    return SemanticConcepts.Num();
}

float UMemorySystems::GetAverageMemoryStrength() const
{
    if (EpisodicMemories.Num() == 0)
    {
        return 0.0f;
    }

    float TotalStrength = 0.0f;
    for (const FEpisodicMemory& Memory : EpisodicMemories)
    {
        switch (Memory.Strength)
        {
            case EEncodingStrength::Weak: TotalStrength += 0.25f; break;
            case EEncodingStrength::Moderate: TotalStrength += 0.5f; break;
            case EEncodingStrength::Strong: TotalStrength += 0.75f; break;
            case EEncodingStrength::Consolidated: TotalStrength += 1.0f; break;
        }
    }

    return TotalStrength / EpisodicMemories.Num();
}

float UMemorySystems::GetMemorySystemHealth() const
{
    float EpisodicHealth = 1.0f - (static_cast<float>(EpisodicMemories.Num()) / MaxEpisodicMemories);
    float SemanticHealth = 1.0f - (static_cast<float>(SemanticConcepts.Num()) / MaxSemanticConcepts);
    float WorkingHealth = 1.0f - GetWorkingMemoryUtilization();
    float StrengthHealth = GetAverageMemoryStrength();

    return (EpisodicHealth + SemanticHealth + WorkingHealth + StrengthHealth) / 4.0f;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UMemorySystems::UpdateMemoryDecay(float DeltaTime)
{
    for (FEpisodicMemory& Memory : EpisodicMemories)
    {
        // Decay vividness over time
        float DecayAmount = MemoryDecayRate * DeltaTime;

        // Consolidated memories decay slower
        if (Memory.bIsConsolidated)
        {
            DecayAmount *= 0.1f;
        }

        // Strong emotional memories decay slower
        DecayAmount *= (1.0f - Memory.EmotionalArousal * 0.5f);

        Memory.Vividness = FMath::Max(0.1f, Memory.Vividness - DecayAmount);
    }
}

void UMemorySystems::UpdateWorkingMemory(float DeltaTime)
{
    TArray<int32> ToRemove;

    for (int32 i = 0; i < WorkingMemoryItems.Num(); ++i)
    {
        FWorkingMemoryItem& Item = WorkingMemoryItems[i];

        // Decay activation
        Item.ActivationLevel -= DeltaTime * 0.1f;

        if (Item.ActivationLevel <= 0.0f)
        {
            ToRemove.Add(i);
        }
    }

    // Remove decayed items (in reverse order)
    for (int32 i = ToRemove.Num() - 1; i >= 0; --i)
    {
        WorkingMemoryItems.RemoveAt(ToRemove[i]);
    }
}

void UMemorySystems::UpdateActivationLevels(float DeltaTime)
{
    for (auto& Pair : SemanticConcepts)
    {
        // Decay toward baseline
        float Diff = Pair.Value.ActivationLevel - Pair.Value.BaselineActivation;
        Pair.Value.ActivationLevel -= Diff * DeltaTime * 0.1f;
    }
}

void UMemorySystems::PerformConsolidation()
{
    // Consolidate frequently retrieved memories
    for (FEpisodicMemory& Memory : EpisodicMemories)
    {
        if (!Memory.bIsConsolidated && Memory.RetrievalCount >= 3)
        {
            Memory.bIsConsolidated = true;
            Memory.Strength = EEncodingStrength::Consolidated;

            FConsolidationEvent Event;
            Event.MemoryID = Memory.MemoryID;
            Event.MemoryType = EMemoryType::Episodic;
            Event.ConsolidationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            Event.StrengthIncrease = 0.3f;

            OnConsolidationComplete.Broadcast(Event);
        }
    }

    // Create associations between temporally close memories
    for (int32 i = 0; i < EpisodicMemories.Num() - 1; ++i)
    {
        for (int32 j = i + 1; j < EpisodicMemories.Num(); ++j)
        {
            float TimeDiff = FMath::Abs(EpisodicMemories[i].Timestamp - EpisodicMemories[j].Timestamp);
            if (TimeDiff < 60.0f) // Within 1 minute
            {
                AssociateEpisodes(EpisodicMemories[i].MemoryID, EpisodicMemories[j].MemoryID);
            }
        }
    }
}

float UMemorySystems::ComputeRetrievalStrength(const FEpisodicMemory& Memory) const
{
    float Strength = Memory.Vividness;

    // Boost for encoding strength
    switch (Memory.Strength)
    {
        case EEncodingStrength::Weak: Strength *= 0.5f; break;
        case EEncodingStrength::Moderate: Strength *= 0.75f; break;
        case EEncodingStrength::Strong: Strength *= 1.0f; break;
        case EEncodingStrength::Consolidated: Strength *= 1.25f; break;
    }

    // Boost for retrieval practice
    Strength += Memory.RetrievalCount * 0.05f;

    return FMath::Clamp(Strength, 0.0f, 1.0f);
}

float UMemorySystems::ComputeMatchScore(const FEpisodicMemory& Memory, ERetrievalCueType CueType, 
                                         const FString& CueValue) const
{
    float Score = 0.0f;

    switch (CueType)
    {
        case ERetrievalCueType::Semantic:
        {
            // Check if cue matches description or tags
            if (Memory.EventDescription.Contains(CueValue))
            {
                Score = 0.8f;
            }
            for (const FString& Tag : Memory.SemanticTags)
            {
                if (Tag.Contains(CueValue) || CueValue.Contains(Tag))
                {
                    Score = FMath::Max(Score, 0.7f);
                }
            }
            break;
        }

        case ERetrievalCueType::Temporal:
        {
            // Parse time cue and compare
            float CueTime = FCString::Atof(*CueValue);
            float TimeDiff = FMath::Abs(Memory.Timestamp - CueTime);
            Score = FMath::Max(0.0f, 1.0f - (TimeDiff / 3600.0f)); // Decay over 1 hour
            break;
        }

        case ERetrievalCueType::Emotional:
        {
            // Match emotional valence
            float CueValence = FCString::Atof(*CueValue);
            float ValenceDiff = FMath::Abs(Memory.EmotionalValence - CueValence);
            Score = 1.0f - ValenceDiff;
            break;
        }

        case ERetrievalCueType::Spatial:
        {
            // Would need to parse location from cue
            Score = 0.5f; // Default moderate match
            break;
        }

        case ERetrievalCueType::Contextual:
        {
            // Check participants
            for (const FString& Participant : Memory.Participants)
            {
                if (Participant.Contains(CueValue) || CueValue.Contains(Participant))
                {
                    Score = 0.8f;
                    break;
                }
            }
            break;
        }
    }

    // Modulate by retrieval strength
    Score *= ComputeRetrievalStrength(Memory);

    return Score;
}

float UMemorySystems::ComputeConceptSimilarity(const FSemanticConcept& Concept1, 
                                                const FSemanticConcept& Concept2) const
{
    float Similarity = 0.0f;

    // Category overlap
    int32 SharedCategories = 0;
    for (const FString& Cat : Concept1.Categories)
    {
        if (Concept2.Categories.Contains(Cat))
        {
            SharedCategories++;
        }
    }
    if (Concept1.Categories.Num() > 0 || Concept2.Categories.Num() > 0)
    {
        Similarity += static_cast<float>(SharedCategories) / 
                      FMath::Max(Concept1.Categories.Num(), Concept2.Categories.Num());
    }

    // Direct relation
    if (Concept1.RelatedConcepts.Contains(Concept2.ConceptID))
    {
        Similarity += 0.3f;
    }

    return FMath::Clamp(Similarity, 0.0f, 1.0f);
}

void UMemorySystems::LinkEpisodeToSemantic(const FString& EpisodeID, const FString& ConceptID)
{
    if (SemanticConcepts.Contains(ConceptID))
    {
        SemanticConcepts[ConceptID].EpisodicLinks.AddUnique(EpisodeID);
    }
}

void UMemorySystems::ExtractSemanticFromEpisode(const FEpisodicMemory& Episode)
{
    // Create or update concepts from semantic tags
    for (const FString& Tag : Episode.SemanticTags)
    {
        FSemanticConcept Concept = LearnConcept(Tag, FString::Printf(TEXT("Concept from episode: %s"), *Tag), 
                                                 {TEXT("Extracted")});
        LinkEpisodeToSemantic(Episode.MemoryID, Concept.ConceptID);
    }
}

FString UMemorySystems::GenerateEpisodeID()
{
    return FString::Printf(TEXT("EP_%d_%d"), ++EpisodeIDCounter, FMath::RandRange(1000, 9999));
}

FString UMemorySystems::GenerateConceptID()
{
    return FString::Printf(TEXT("CON_%d_%d"), ++ConceptIDCounter, FMath::RandRange(1000, 9999));
}

FString UMemorySystems::GenerateWorkingItemID()
{
    return FString::Printf(TEXT("WM_%d_%d"), ++WorkingItemIDCounter, FMath::RandRange(1000, 9999));
}

void UMemorySystems::PruneOldMemories()
{
    if (EpisodicMemories.Num() <= MaxEpisodicMemories)
    {
        return;
    }

    // Sort by retrieval strength (weakest first)
    EpisodicMemories.Sort([this](const FEpisodicMemory& A, const FEpisodicMemory& B) {
        return ComputeRetrievalStrength(A) < ComputeRetrievalStrength(B);
    });

    // Remove weakest memories
    while (EpisodicMemories.Num() > MaxEpisodicMemories)
    {
        // Don't remove consolidated memories
        if (!EpisodicMemories[0].bIsConsolidated)
        {
            EpisodicMemories.RemoveAt(0);
        }
        else
        {
            break;
        }
    }
}

void UMemorySystems::EvictWorkingMemoryItem()
{
    if (WorkingMemoryItems.Num() == 0)
    {
        return;
    }

    // Find lowest priority/activation item
    int32 LowestIndex = 0;
    float LowestScore = WorkingMemoryItems[0].Priority * WorkingMemoryItems[0].ActivationLevel;

    for (int32 i = 1; i < WorkingMemoryItems.Num(); ++i)
    {
        float Score = WorkingMemoryItems[i].Priority * WorkingMemoryItems[i].ActivationLevel;
        if (Score < LowestScore)
        {
            LowestScore = Score;
            LowestIndex = i;
        }
    }

    WorkingMemoryItems.RemoveAt(LowestIndex);
}

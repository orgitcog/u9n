// CausalChainTracking.cpp
// Causal relationship tracking between temporal events for reasoning about consequences
// Feature F1.5.2 - Temporal Reasoning System
// Deep Tree Echo Cognitive Architecture

#include "CausalChainTracking.h"
#include "EpisodicMemorySystem.h"
#include "HypergraphMemorySystem.h"

UCausalChainTracking::UCausalChainTracking()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz update rate
}

void UCausalChainTracking::BeginPlay()
{
    Super::BeginPlay();

    // Auto-discover memory systems on owner
    if (!EpisodicMemoryRef)
    {
        EpisodicMemoryRef = GetOwner()->FindComponentByClass<UEpisodicMemorySystem>();
    }

    if (!HypergraphMemoryRef)
    {
        HypergraphMemoryRef = GetOwner()->FindComponentByClass<UHypergraphMemorySystem>();
    }
}

void UCausalChainTracking::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AccumulatedTime += DeltaTime;

    // Decay unused links
    if (Config.LinkDecayRate > 0.0f)
    {
        DecayLinks(DeltaTime);
    }

    // Process automatic inference
    if (Config.bEnableAutoInference)
    {
        if (AccumulatedTime - LastInferenceTime >= Config.AutoInferenceInterval)
        {
            ProcessAutoInference();
            LastInferenceTime = AccumulatedTime;
        }
    }
}

// ========================================
// EVENT MANAGEMENT
// ========================================

int64 UCausalChainTracking::RecordEvent(const FString& Description, ECausalEventType EventType,
                                         FVector Location, float Salience)
{
    FCausalEvent Event;
    Event.EventID = GenerateEventID();
    Event.Description = Description;
    Event.EventType = EventType;
    Event.Location = Location;
    Event.Salience = FMath::Clamp(Salience, 0.0f, 1.0f);
    Event.Timestamp = GetCurrentTime();
    Event.OccurrenceConfidence = 1.0f;

    return RecordEventFull(Event);
}

int64 UCausalChainTracking::RecordEventFull(const FCausalEvent& Event)
{
    FCausalEvent NewEvent = Event;
    
    // Ensure valid ID
    if (NewEvent.EventID == 0)
    {
        NewEvent.EventID = GenerateEventID();
    }

    // Set timestamp if not provided
    if (NewEvent.Timestamp == 0.0f)
    {
        NewEvent.Timestamp = GetCurrentTime();
    }

    // Enforce capacity limits
    if (Events.Num() >= Config.MaxEvents)
    {
        // Remove oldest event
        int64 OldestID = 0;
        float OldestTime = FLT_MAX;
        for (const auto& Pair : Events)
        {
            if (Pair.Value.Timestamp < OldestTime)
            {
                OldestTime = Pair.Value.Timestamp;
                OldestID = Pair.Key;
            }
        }
        if (OldestID != 0)
        {
            Events.Remove(OldestID);
            CauseToLinks.Remove(OldestID);
            EffectToLinks.Remove(OldestID);
            EventToChains.Remove(OldestID);
        }
    }

    Events.Add(NewEvent.EventID, NewEvent);

    // Broadcast event
    OnCausalEventRecorded.Broadcast(NewEvent.EventID);

    return NewEvent.EventID;
}

FCausalEvent UCausalChainTracking::GetEvent(int64 EventID) const
{
    const FCausalEvent* Found = Events.Find(EventID);
    return Found ? *Found : FCausalEvent();
}

bool UCausalChainTracking::EventExists(int64 EventID) const
{
    return Events.Contains(EventID);
}

TArray<FCausalEvent> UCausalChainTracking::GetEventsInTimeRange(float StartTime, float EndTime) const
{
    TArray<FCausalEvent> Result;
    for (const auto& Pair : Events)
    {
        if (Pair.Value.Timestamp >= StartTime && Pair.Value.Timestamp <= EndTime)
        {
            Result.Add(Pair.Value);
        }
    }

    // Sort by timestamp
    Result.Sort([](const FCausalEvent& A, const FCausalEvent& B)
    {
        return A.Timestamp < B.Timestamp;
    });

    return Result;
}

TArray<FCausalEvent> UCausalChainTracking::GetEventsByType(ECausalEventType EventType) const
{
    TArray<FCausalEvent> Result;
    for (const auto& Pair : Events)
    {
        if (Pair.Value.EventType == EventType)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FCausalEvent> UCausalChainTracking::GetRecentEvents(int32 Count) const
{
    TArray<FCausalEvent> AllEvents;
    for (const auto& Pair : Events)
    {
        AllEvents.Add(Pair.Value);
    }

    // Sort by timestamp descending
    AllEvents.Sort([](const FCausalEvent& A, const FCausalEvent& B)
    {
        return A.Timestamp > B.Timestamp;
    });

    // Take top N
    if (AllEvents.Num() > Count)
    {
        AllEvents.SetNum(Count);
    }

    return AllEvents;
}

void UCausalChainTracking::UpdateEventSalience(int64 EventID, float NewSalience)
{
    FCausalEvent* Event = Events.Find(EventID);
    if (Event)
    {
        Event->Salience = FMath::Clamp(NewSalience, 0.0f, 1.0f);
    }
}

// ========================================
// CAUSAL LINK MANAGEMENT
// ========================================

int64 UCausalChainTracking::CreateCausalLink(int64 CauseEventID, int64 EffectEventID,
                                              ECausalRelationType RelationType,
                                              float Strength, const FString& Explanation)
{
    // Validate events exist
    if (!EventExists(CauseEventID) || !EventExists(EffectEventID))
    {
        return 0;
    }

    // Check if link already exists
    FCausalLink Existing = GetLinkBetween(CauseEventID, EffectEventID);
    if (Existing.LinkID != 0)
    {
        // Strengthen existing link
        StrengthenLink(Existing.LinkID, Config.ConfirmationFactor);
        return Existing.LinkID;
    }

    // Enforce capacity limits
    if (Links.Num() >= Config.MaxLinks)
    {
        PruneWeakLinks(Config.MinLinkStrength);
    }

    FCausalLink Link;
    Link.LinkID = GenerateLinkID();
    Link.CauseEventID = CauseEventID;
    Link.EffectEventID = EffectEventID;
    Link.RelationType = RelationType;
    Link.Strength = FMath::Clamp(Strength, 0.0f, 1.0f);
    Link.Explanation = Explanation;
    Link.CreationTime = GetCurrentTime();
    Link.LastUpdateTime = Link.CreationTime;
    Link.ObservationCount = 1;
    Link.bIsActive = true;

    // Compute temporal delay
    const FCausalEvent* Cause = Events.Find(CauseEventID);
    const FCausalEvent* Effect = Events.Find(EffectEventID);
    if (Cause && Effect)
    {
        Link.TemporalDelay = Effect->Timestamp - Cause->Timestamp;
    }

    // Compute confidence based on temporal plausibility and strength
    float TemporalPlausibility = 1.0f;
    if (Cause && Effect)
    {
        TemporalPlausibility = ComputeTemporalPlausibility(*Cause, *Effect);
    }
    Link.ConfidenceValue = FMath::Clamp(Strength * TemporalPlausibility, 0.0f, 1.0f);

    // Set confidence level
    if (Link.ConfidenceValue >= 0.9f)
    {
        Link.Confidence = ECausalConfidence::Certain;
    }
    else if (Link.ConfidenceValue >= 0.7f)
    {
        Link.Confidence = ECausalConfidence::Probable;
    }
    else if (Link.ConfidenceValue >= 0.5f)
    {
        Link.Confidence = ECausalConfidence::Possible;
    }
    else
    {
        Link.Confidence = ECausalConfidence::Speculative;
    }

    Links.Add(Link.LinkID, Link);

    // Update indexes
    CauseToLinks.FindOrAdd(CauseEventID).Add(Link.LinkID);
    EffectToLinks.FindOrAdd(EffectEventID).Add(Link.LinkID);

    // Broadcast
    OnCausalLinkCreated.Broadcast(Link.LinkID, RelationType);

    return Link.LinkID;
}

FCausalLink UCausalChainTracking::GetCausalLink(int64 LinkID) const
{
    const FCausalLink* Found = Links.Find(LinkID);
    return Found ? *Found : FCausalLink();
}

bool UCausalChainTracking::LinkExists(int64 LinkID) const
{
    return Links.Contains(LinkID);
}

TArray<FCausalLink> UCausalChainTracking::GetEffectsOf(int64 CauseEventID) const
{
    TArray<FCausalLink> Result;
    const TArray<int64>* LinkIDs = CauseToLinks.Find(CauseEventID);
    if (LinkIDs)
    {
        for (int64 LinkID : *LinkIDs)
        {
            const FCausalLink* Link = Links.Find(LinkID);
            if (Link && Link->bIsActive)
            {
                Result.Add(*Link);
            }
        }
    }
    return Result;
}

TArray<FCausalLink> UCausalChainTracking::GetCausesOf(int64 EffectEventID) const
{
    TArray<FCausalLink> Result;
    const TArray<int64>* LinkIDs = EffectToLinks.Find(EffectEventID);
    if (LinkIDs)
    {
        for (int64 LinkID : *LinkIDs)
        {
            const FCausalLink* Link = Links.Find(LinkID);
            if (Link && Link->bIsActive)
            {
                Result.Add(*Link);
            }
        }
    }
    return Result;
}

FCausalLink UCausalChainTracking::GetLinkBetween(int64 CauseEventID, int64 EffectEventID) const
{
    const TArray<int64>* LinkIDs = CauseToLinks.Find(CauseEventID);
    if (LinkIDs)
    {
        for (int64 LinkID : *LinkIDs)
        {
            const FCausalLink* Link = Links.Find(LinkID);
            if (Link && Link->EffectEventID == EffectEventID)
            {
                return *Link;
            }
        }
    }
    return FCausalLink();
}

void UCausalChainTracking::StrengthenLink(int64 LinkID, float Factor)
{
    FCausalLink* Link = Links.Find(LinkID);
    if (Link)
    {
        Link->Strength = FMath::Clamp(Link->Strength * Factor, 0.0f, 1.0f);
        Link->ConfidenceValue = FMath::Clamp(Link->ConfidenceValue * Factor, 0.0f, 1.0f);
        Link->ObservationCount++;
        Link->LastUpdateTime = GetCurrentTime();

        // Update confidence level
        if (Link->ConfidenceValue >= 0.9f)
        {
            Link->Confidence = ECausalConfidence::Certain;
        }
        else if (Link->ConfidenceValue >= 0.7f)
        {
            Link->Confidence = ECausalConfidence::Probable;
        }
    }
}

void UCausalChainTracking::WeakenLink(int64 LinkID, float Factor)
{
    FCausalLink* Link = Links.Find(LinkID);
    if (Link)
    {
        Link->Strength = FMath::Clamp(Link->Strength * Factor, 0.0f, 1.0f);
        Link->ConfidenceValue = FMath::Clamp(Link->ConfidenceValue * Factor, 0.0f, 1.0f);
        Link->LastUpdateTime = GetCurrentTime();

        // Update confidence level
        if (Link->ConfidenceValue < 0.3f)
        {
            Link->Confidence = ECausalConfidence::Speculative;
        }
        else if (Link->ConfidenceValue < 0.5f)
        {
            Link->Confidence = ECausalConfidence::Possible;
        }
    }
}

bool UCausalChainTracking::RemoveLink(int64 LinkID)
{
    const FCausalLink* Link = Links.Find(LinkID);
    if (!Link)
    {
        return false;
    }

    // Remove from indexes
    TArray<int64>* CauseLinks = CauseToLinks.Find(Link->CauseEventID);
    if (CauseLinks)
    {
        CauseLinks->Remove(LinkID);
    }

    TArray<int64>* EffectLinks = EffectToLinks.Find(Link->EffectEventID);
    if (EffectLinks)
    {
        EffectLinks->Remove(LinkID);
    }

    Links.Remove(LinkID);
    return true;
}

// ========================================
// CAUSAL CHAIN MANAGEMENT
// ========================================

int64 UCausalChainTracking::BuildChain(int64 RootCauseID, int64 FinalEffectID, const FString& Label)
{
    // Find path from root cause to final effect
    TArray<int64> Path = FindCausalPath(RootCauseID, FinalEffectID, Config.MaxChainDepth);
    if (Path.Num() < 2)
    {
        return 0; // No valid path found
    }

    // Enforce capacity limits
    if (Chains.Num() >= Config.MaxChains)
    {
        // Remove oldest chain
        int64 OldestID = 0;
        float OldestTime = FLT_MAX;
        for (const auto& Pair : Chains)
        {
            if (Pair.Value.CreationTime < OldestTime)
            {
                OldestTime = Pair.Value.CreationTime;
                OldestID = Pair.Key;
            }
        }
        if (OldestID != 0)
        {
            Chains.Remove(OldestID);
        }
    }

    FCausalChain Chain;
    Chain.ChainID = GenerateChainID();
    Chain.Label = Label.IsEmpty() ? FString::Printf(TEXT("Chain_%lld"), Chain.ChainID) : Label;
    Chain.EventIDs = Path;
    Chain.RootCauseID = RootCauseID;
    Chain.FinalEffectID = FinalEffectID;
    Chain.CreationTime = GetCurrentTime();
    Chain.Status = ECausalChainStatus::Complete;
    Chain.Depth = Path.Num() - 1;

    // Collect link IDs and compute chain strength
    float MinStrength = 1.0f;
    float ProductStrength = 1.0f;
    float TotalConfidence = 0.0f;
    int32 LinkCount = 0;

    for (int32 i = 0; i < Path.Num() - 1; ++i)
    {
        FCausalLink Link = GetLinkBetween(Path[i], Path[i + 1]);
        if (Link.LinkID != 0)
        {
            Chain.LinkIDs.Add(Link.LinkID);
            MinStrength = FMath::Min(MinStrength, Link.Strength);
            ProductStrength *= Link.Strength;
            TotalConfidence += Link.ConfidenceValue;
            LinkCount++;
        }
    }

    Chain.OverallStrength = MinStrength; // Conservative estimate
    Chain.OverallConfidence = LinkCount > 0 ? TotalConfidence / LinkCount : 0.0f;

    // Compute temporal span
    const FCausalEvent* Root = Events.Find(RootCauseID);
    const FCausalEvent* Final = Events.Find(FinalEffectID);
    if (Root && Final)
    {
        Chain.TemporalSpan = Final->Timestamp - Root->Timestamp;
    }

    Chains.Add(Chain.ChainID, Chain);

    // Update event-to-chain index
    for (int64 EventID : Path)
    {
        EventToChains.FindOrAdd(EventID).Add(Chain.ChainID);
    }

    // Broadcast
    OnCausalChainDiscovered.Broadcast(Chain.ChainID);

    return Chain.ChainID;
}

FCausalChain UCausalChainTracking::GetChain(int64 ChainID) const
{
    const FCausalChain* Found = Chains.Find(ChainID);
    return Found ? *Found : FCausalChain();
}

TArray<FCausalChain> UCausalChainTracking::GetChainsInvolvingEvent(int64 EventID) const
{
    TArray<FCausalChain> Result;
    const TArray<int64>* ChainIDs = EventToChains.Find(EventID);
    if (ChainIDs)
    {
        for (int64 ChainID : *ChainIDs)
        {
            const FCausalChain* Chain = Chains.Find(ChainID);
            if (Chain)
            {
                Result.Add(*Chain);
            }
        }
    }
    return Result;
}

TArray<FCausalChain> UCausalChainTracking::GetChainsByStatus(ECausalChainStatus Status) const
{
    TArray<FCausalChain> Result;
    for (const auto& Pair : Chains)
    {
        if (Pair.Value.Status == Status)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

bool UCausalChainTracking::ValidateChain(int64 ChainID)
{
    FCausalChain* Chain = Chains.Find(ChainID);
    if (!Chain)
    {
        return false;
    }

    // Check all events still exist
    for (int64 EventID : Chain->EventIDs)
    {
        if (!EventExists(EventID))
        {
            Chain->Status = ECausalChainStatus::Invalidated;
            return false;
        }
    }

    // Check all links still exist and are active
    for (int64 LinkID : Chain->LinkIDs)
    {
        const FCausalLink* Link = Links.Find(LinkID);
        if (!Link || !Link->bIsActive)
        {
            Chain->Status = ECausalChainStatus::Invalidated;
            return false;
        }
    }

    Chain->Status = ECausalChainStatus::Validated;
    return true;
}

void UCausalChainTracking::InvalidateChain(int64 ChainID)
{
    FCausalChain* Chain = Chains.Find(ChainID);
    if (Chain)
    {
        Chain->Status = ECausalChainStatus::Invalidated;
    }
}

FCausalEvent UCausalChainTracking::GetRootCause(int64 EffectEventID, int32 MaxDepth) const
{
    if (MaxDepth <= 0 || !EventExists(EffectEventID))
    {
        return GetEvent(EffectEventID);
    }

    TArray<FCausalLink> Causes = GetCausesOf(EffectEventID);
    if (Causes.Num() == 0)
    {
        return GetEvent(EffectEventID);
    }

    // Find strongest cause
    FCausalLink StrongestCause = Causes[0];
    for (const FCausalLink& Link : Causes)
    {
        if (Link.Strength > StrongestCause.Strength)
        {
            StrongestCause = Link;
        }
    }

    // Recursively trace back
    return GetRootCause(StrongestCause.CauseEventID, MaxDepth - 1);
}

TArray<FCausalEvent> UCausalChainTracking::GetAllConsequences(int64 CauseEventID, int32 MaxDepth) const
{
    TArray<FCausalEvent> Result;
    TSet<int64> Visited;

    TArray<int64> ToExplore;
    ToExplore.Add(CauseEventID);
    Visited.Add(CauseEventID);

    int32 CurrentDepth = 0;
    while (ToExplore.Num() > 0 && CurrentDepth < MaxDepth)
    {
        TArray<int64> NextLevel;

        for (int64 EventID : ToExplore)
        {
            TArray<FCausalLink> Effects = GetEffectsOf(EventID);
            for (const FCausalLink& Link : Effects)
            {
                if (!Visited.Contains(Link.EffectEventID))
                {
                    Visited.Add(Link.EffectEventID);
                    Result.Add(GetEvent(Link.EffectEventID));
                    NextLevel.Add(Link.EffectEventID);
                }
            }
        }

        ToExplore = NextLevel;
        CurrentDepth++;
    }

    return Result;
}

// ========================================
// CAUSAL INFERENCE
// ========================================

FCausalInferenceResult UCausalChainTracking::InferCausality(float TimeWindow)
{
    double StartTime = FPlatformTime::Seconds();

    FCausalInferenceResult Result;
    Result.bSuccess = true;

    float CurrentTime = GetCurrentTime();
    TArray<FCausalEvent> RecentEvents = GetEventsInTimeRange(CurrentTime - TimeWindow, CurrentTime);

    if (RecentEvents.Num() < 2)
    {
        Result.bSuccess = false;
        Result.Explanation = TEXT("Not enough events for inference");
        return Result;
    }

    Result.EventsExamined = RecentEvents.Num();

    // Look for temporal sequences that might be causal
    for (int32 i = 0; i < RecentEvents.Num() - 1; ++i)
    {
        for (int32 j = i + 1; j < RecentEvents.Num(); ++j)
        {
            const FCausalEvent& Earlier = RecentEvents[i];
            const FCausalEvent& Later = RecentEvents[j];

            // Check temporal plausibility
            float TemporalPlausibility = ComputeTemporalPlausibility(Earlier, Later);
            if (TemporalPlausibility < Config.MinConfidence)
            {
                continue;
            }

            // Check content similarity if enabled
            float ContentSim = 0.5f;
            if (Config.bUseContentSimilarity && Earlier.ContentVector.Num() > 0 && Later.ContentVector.Num() > 0)
            {
                ContentSim = ComputeEventSimilarity(Earlier, Later);
                if (ContentSim < Config.ContentSimilarityThreshold)
                {
                    continue;
                }
            }

            // Compute overall strength
            float Strength = TemporalPlausibility * (0.5f + 0.5f * ContentSim);
            if (Strength >= Config.MinLinkStrength)
            {
                // Create or strengthen link
                int64 LinkID = CreateCausalLink(Earlier.EventID, Later.EventID,
                    ECausalRelationType::ProbabilisticCause, Strength,
                    TEXT("Inferred from temporal sequence"));

                if (LinkID != 0)
                {
                    Result.DiscoveredLinks.Add(GetCausalLink(LinkID));
                }
            }
        }
    }

    // Try to build chains from discovered links
    for (const FCausalLink& Link : Result.DiscoveredLinks)
    {
        // Check if this link can extend an existing chain
        for (const auto& Pair : Chains)
        {
            if (Pair.Value.FinalEffectID == Link.CauseEventID)
            {
                // Extend chain
                int64 NewChainID = BuildChain(Pair.Value.RootCauseID, Link.EffectEventID);
                if (NewChainID != 0)
                {
                    Result.InferredChains.Add(GetChain(NewChainID));
                }
            }
        }
    }

    Result.InferenceConfidence = Result.DiscoveredLinks.Num() > 0 ?
        1.0f / (1.0f + FMath::Exp(-0.1f * Result.DiscoveredLinks.Num())) : 0.0f;

    Result.Explanation = FString::Printf(TEXT("Examined %d events, found %d links, %d chains"),
        Result.EventsExamined, Result.DiscoveredLinks.Num(), Result.InferredChains.Num());

    double EndTime = FPlatformTime::Seconds();
    Result.ExecutionTimeMs = (EndTime - StartTime) * 1000.0;

    TotalInferenceOperations++;
    TotalInferenceTimeMs += Result.ExecutionTimeMs;

    return Result;
}

FCausalInferenceResult UCausalChainTracking::InferRelationship(int64 EventA, int64 EventB)
{
    FCausalInferenceResult Result;

    const FCausalEvent* A = Events.Find(EventA);
    const FCausalEvent* B = Events.Find(EventB);

    if (!A || !B)
    {
        Result.bSuccess = false;
        Result.Explanation = TEXT("One or both events not found");
        return Result;
    }

    Result.bSuccess = true;
    Result.EventsExamined = 2;

    // Determine temporal order
    const FCausalEvent* Earlier = A->Timestamp <= B->Timestamp ? A : B;
    const FCausalEvent* Later = A->Timestamp <= B->Timestamp ? B : A;

    // Check temporal plausibility
    float TemporalPlausibility = ComputeTemporalPlausibility(*Earlier, *Later);

    // Check content similarity
    float ContentSim = 0.5f;
    if (Config.bUseContentSimilarity && Earlier->ContentVector.Num() > 0 && Later->ContentVector.Num() > 0)
    {
        ContentSim = ComputeEventSimilarity(*Earlier, *Later);
    }

    float Strength = TemporalPlausibility * (0.5f + 0.5f * ContentSim);

    if (Strength >= Config.MinLinkStrength)
    {
        ECausalRelationType RelationType = ECausalRelationType::ProbabilisticCause;
        if (TemporalPlausibility >= 0.9f)
        {
            RelationType = ECausalRelationType::DirectCause;
        }
        else if (TemporalPlausibility >= 0.5f)
        {
            RelationType = ECausalRelationType::IndirectCause;
        }

        int64 LinkID = CreateCausalLink(Earlier->EventID, Later->EventID, RelationType, Strength,
            TEXT("Inferred from pairwise analysis"));

        if (LinkID != 0)
        {
            Result.DiscoveredLinks.Add(GetCausalLink(LinkID));
        }
    }
    else
    {
        Result.AlternativeExplanations.Add(TEXT("Temporal precedence only (correlation)"));
    }

    Result.InferenceConfidence = Strength;
    Result.Explanation = FString::Printf(TEXT("Temporal plausibility: %.2f, Content similarity: %.2f"),
        TemporalPlausibility, ContentSim);

    return Result;
}

TArray<FCausalLink> UCausalChainTracking::FindPotentialCauses(int64 EffectEventID, float TimeWindow)
{
    TArray<FCausalLink> Result;

    const FCausalEvent* Effect = Events.Find(EffectEventID);
    if (!Effect)
    {
        return Result;
    }

    TArray<FCausalEvent> PotentialCauses = GetEventsInTimeRange(
        Effect->Timestamp - TimeWindow, Effect->Timestamp);

    for (const FCausalEvent& Cause : PotentialCauses)
    {
        if (Cause.EventID == EffectEventID)
        {
            continue;
        }

        float TemporalPlausibility = ComputeTemporalPlausibility(Cause, *Effect);
        if (TemporalPlausibility >= Config.MinConfidence)
        {
            FCausalLink Link;
            Link.CauseEventID = Cause.EventID;
            Link.EffectEventID = EffectEventID;
            Link.RelationType = ECausalRelationType::ProbabilisticCause;
            Link.Strength = TemporalPlausibility * Cause.Salience;
            Link.TemporalDelay = Effect->Timestamp - Cause.Timestamp;
            Result.Add(Link);
        }
    }

    // Sort by strength
    Result.Sort([](const FCausalLink& A, const FCausalLink& B)
    {
        return A.Strength > B.Strength;
    });

    return Result;
}

TArray<FCausalLink> UCausalChainTracking::FindPotentialEffects(int64 CauseEventID, float TimeWindow)
{
    TArray<FCausalLink> Result;

    const FCausalEvent* Cause = Events.Find(CauseEventID);
    if (!Cause)
    {
        return Result;
    }

    TArray<FCausalEvent> PotentialEffects = GetEventsInTimeRange(
        Cause->Timestamp, Cause->Timestamp + TimeWindow);

    for (const FCausalEvent& Effect : PotentialEffects)
    {
        if (Effect.EventID == CauseEventID)
        {
            continue;
        }

        float TemporalPlausibility = ComputeTemporalPlausibility(*Cause, Effect);
        if (TemporalPlausibility >= Config.MinConfidence)
        {
            FCausalLink Link;
            Link.CauseEventID = CauseEventID;
            Link.EffectEventID = Effect.EventID;
            Link.RelationType = ECausalRelationType::ProbabilisticCause;
            Link.Strength = TemporalPlausibility * Effect.Salience;
            Link.TemporalDelay = Effect.Timestamp - Cause->Timestamp;
            Result.Add(Link);
        }
    }

    // Sort by strength
    Result.Sort([](const FCausalLink& A, const FCausalLink& B)
    {
        return A.Strength > B.Strength;
    });

    return Result;
}

TArray<FCausalEvent> UCausalChainTracking::CounterfactualAnalysis(int64 EventID)
{
    TArray<FCausalEvent> AffectedEvents;

    if (!EventExists(EventID))
    {
        return AffectedEvents;
    }

    // Get all consequences of this event
    AffectedEvents = GetAllConsequences(EventID, Config.MaxChainDepth);

    // Mark affected events
    for (FCausalEvent& Event : AffectedEvents)
    {
        Event.EventType = ECausalEventType::Counterfactual;
        Event.Description = FString::Printf(TEXT("Would not occur without Event %lld: %s"),
            EventID, *Event.Description);
    }

    return AffectedEvents;
}

// ========================================
// PREDICTION
// ========================================

TArray<FCausalPrediction> UCausalChainTracking::PredictConsequences(int64 CauseEventID)
{
    TArray<FCausalPrediction> Result;

    if (!Config.bEnablePredictions || !EventExists(CauseEventID))
    {
        return Result;
    }

    const FCausalEvent* Cause = Events.Find(CauseEventID);
    if (!Cause)
    {
        return Result;
    }

    // Find similar past causes and their effects
    TArray<FCausalLink> Effects = GetEffectsOf(CauseEventID);
    for (const FCausalLink& Link : Effects)
    {
        FCausalPrediction Prediction;
        Prediction.PredictionID = GeneratePredictionID();
        Prediction.PredictedEvent = GetEvent(Link.EffectEventID);
        Prediction.PredictedEvent.EventType = ECausalEventType::Hypothetical;
        Prediction.PredictedEvent.Timestamp = Cause->Timestamp + Link.TemporalDelay;
        Prediction.SupportingChainID = 0; // Could look up chain
        Prediction.Confidence = Link.Strength * Link.ConfidenceValue;
        Prediction.PredictedTime = Cause->Timestamp + Link.TemporalDelay;
        Prediction.TimeWindow = FMath::Max(1.0f, Link.TemporalDelay * 0.2f);

        Predictions.Add(Prediction.PredictionID, Prediction);
        Result.Add(Prediction);

        OnPredictionMade.Broadcast(Prediction.PredictionID, Prediction.Confidence);
    }

    // Look for pattern matches in chains
    TArray<FCausalChain> InvolvingChains = GetChainsInvolvingEvent(CauseEventID);
    for (const FCausalChain& Chain : InvolvingChains)
    {
        // Find position in chain
        int32 Position = Chain.EventIDs.Find(CauseEventID);
        if (Position >= 0 && Position < Chain.EventIDs.Num() - 1)
        {
            // Predict next event in chain
            FCausalPrediction Prediction;
            Prediction.PredictionID = GeneratePredictionID();
            Prediction.PredictedEvent = GetEvent(Chain.EventIDs[Position + 1]);
            Prediction.PredictedEvent.EventType = ECausalEventType::Hypothetical;
            Prediction.SupportingChainID = Chain.ChainID;
            Prediction.Confidence = Chain.OverallConfidence * Chain.OverallStrength;

            // Estimate timing based on chain pattern
            if (Position > 0)
            {
                const FCausalEvent* Prev = Events.Find(Chain.EventIDs[Position - 1]);
                const FCausalEvent* Curr = Events.Find(CauseEventID);
                const FCausalEvent* Next = Events.Find(Chain.EventIDs[Position + 1]);
                if (Prev && Curr && Next)
                {
                    float ExpectedDelay = Next->Timestamp - Curr->Timestamp;
                    Prediction.PredictedTime = Cause->Timestamp + ExpectedDelay;
                    Prediction.TimeWindow = ExpectedDelay * 0.3f;
                }
            }

            Predictions.Add(Prediction.PredictionID, Prediction);
            Result.Add(Prediction);

            OnPredictionMade.Broadcast(Prediction.PredictionID, Prediction.Confidence);
        }
    }

    return Result;
}

TArray<FCausalPrediction> UCausalChainTracking::PredictCauses(int64 EffectEventID)
{
    TArray<FCausalPrediction> Result;

    if (!Config.bEnablePredictions || !EventExists(EffectEventID))
    {
        return Result;
    }

    TArray<FCausalLink> Causes = GetCausesOf(EffectEventID);
    for (const FCausalLink& Link : Causes)
    {
        FCausalPrediction Prediction;
        Prediction.PredictionID = GeneratePredictionID();
        Prediction.PredictedEvent = GetEvent(Link.CauseEventID);
        Prediction.Confidence = Link.Strength * Link.ConfidenceValue;

        Predictions.Add(Prediction.PredictionID, Prediction);
        Result.Add(Prediction);
    }

    return Result;
}

void UCausalChainTracking::VerifyPrediction(int64 PredictionID, int64 ActualOutcomeID)
{
    FCausalPrediction* Prediction = Predictions.Find(PredictionID);
    if (!Prediction)
    {
        return;
    }

    Prediction->bVerified = true;
    Prediction->ActualOutcomeID = ActualOutcomeID;

    // Check if prediction was correct (event occurred within time window)
    const FCausalEvent* Actual = Events.Find(ActualOutcomeID);
    if (Actual)
    {
        float TimeDiff = FMath::Abs(Actual->Timestamp - Prediction->PredictedTime);
        Prediction->bCorrect = TimeDiff <= Prediction->TimeWindow;

        // Update causal links based on prediction accuracy
        if (Prediction->bCorrect)
        {
            // Strengthen supporting links
            for (const auto& Pair : Links)
            {
                if (Pair.Value.EffectEventID == Prediction->PredictedEvent.EventID)
                {
                    StrengthenLink(Pair.Key, Config.ConfirmationFactor);
                }
            }
        }
        else
        {
            // Weaken supporting links
            for (const auto& Pair : Links)
            {
                if (Pair.Value.EffectEventID == Prediction->PredictedEvent.EventID)
                {
                    WeakenLink(Pair.Key, Config.DisconfirmationFactor);
                }
            }
        }
    }

    OnPredictionVerified.Broadcast(PredictionID, Prediction->bCorrect);
}

TArray<FCausalPrediction> UCausalChainTracking::GetActivePredictions() const
{
    TArray<FCausalPrediction> Result;
    for (const auto& Pair : Predictions)
    {
        if (!Pair.Value.bVerified)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

FCausalPrediction UCausalChainTracking::GetPrediction(int64 PredictionID) const
{
    const FCausalPrediction* Found = Predictions.Find(PredictionID);
    return Found ? *Found : FCausalPrediction();
}

// ========================================
// INTEGRATION
// ========================================

void UCausalChainTracking::SetEpisodicMemory(UEpisodicMemorySystem* EpisodicMemory)
{
    EpisodicMemoryRef = EpisodicMemory;
}

void UCausalChainTracking::SetHypergraphMemory(UHypergraphMemorySystem* HypergraphMemory)
{
    HypergraphMemoryRef = HypergraphMemory;
}

void UCausalChainTracking::SyncWithEpisodicMemory(int64 EventID, int64 MemoryTraceID)
{
    FCausalEvent* Event = Events.Find(EventID);
    if (Event)
    {
        Event->MemoryTraceID = MemoryTraceID;
    }
}

void UCausalChainTracking::SyncWithHypergraph(int64 EventID, int64 NodeID)
{
    FCausalEvent* Event = Events.Find(EventID);
    if (Event)
    {
        Event->HypergraphNodeID = NodeID;
    }
}

void UCausalChainTracking::DiscoverFromMemory()
{
    // This could be expanded to analyze episodic memory traces
    // and discover causal relationships from historical data
    if (Config.bEnableAutoInference)
    {
        InferCausality(Config.MaxCausalWindow);
    }
}

// ========================================
// STATISTICS AND DIAGNOSTICS
// ========================================

FCausalTrackingStats UCausalChainTracking::GetStatistics() const
{
    FCausalTrackingStats Stats;
    Stats.TotalEvents = Events.Num();
    Stats.TotalLinks = Links.Num();
    Stats.TotalChains = Chains.Num();

    // Count predictions
    for (const auto& Pair : Predictions)
    {
        if (!Pair.Value.bVerified)
        {
            Stats.ActivePredictions++;
        }
        else
        {
            Stats.VerifiedPredictions++;
            if (Pair.Value.bCorrect)
            {
                Stats.CorrectPredictions++;
            }
        }
    }

    Stats.PredictionAccuracy = Stats.VerifiedPredictions > 0 ?
        static_cast<float>(Stats.CorrectPredictions) / Stats.VerifiedPredictions : 0.0f;

    // Compute averages
    float TotalDepth = 0.0f;
    for (const auto& Pair : Chains)
    {
        TotalDepth += Pair.Value.Depth;
    }
    Stats.AverageChainDepth = Chains.Num() > 0 ? TotalDepth / Chains.Num() : 0.0f;

    float TotalStrength = 0.0f;
    for (const auto& Pair : Links)
    {
        TotalStrength += Pair.Value.Strength;
    }
    Stats.AverageLinkStrength = Links.Num() > 0 ? TotalStrength / Links.Num() : 0.0f;

    Stats.TotalInferences = TotalInferenceOperations;
    Stats.AverageInferenceTimeMs = TotalInferenceOperations > 0 ?
        TotalInferenceTimeMs / TotalInferenceOperations : 0.0f;

    return Stats;
}

FString UCausalChainTracking::GetSystemInfo() const
{
    FCausalTrackingStats Stats = GetStatistics();
    return FString::Printf(
        TEXT("CausalChainTracking: Events=%d, Links=%d, Chains=%d, Predictions=%d/%d (%.1f%% accurate)"),
        Stats.TotalEvents, Stats.TotalLinks, Stats.TotalChains,
        Stats.ActivePredictions, Stats.VerifiedPredictions,
        Stats.PredictionAccuracy * 100.0f);
}

void UCausalChainTracking::Reset()
{
    Events.Empty();
    Links.Empty();
    Chains.Empty();
    Predictions.Empty();
    CauseToLinks.Empty();
    EffectToLinks.Empty();
    EventToChains.Empty();
    NextEventID = 1;
    NextLinkID = 1;
    NextChainID = 1;
    NextPredictionID = 1;
    AccumulatedTime = 0.0f;
    LastInferenceTime = 0.0f;
    TotalInferenceOperations = 0;
    TotalInferenceTimeMs = 0.0;
}

void UCausalChainTracking::PruneWeakLinks(float MinStrength)
{
    TArray<int64> ToRemove;
    for (const auto& Pair : Links)
    {
        if (Pair.Value.Strength < MinStrength)
        {
            ToRemove.Add(Pair.Key);
        }
    }

    for (int64 LinkID : ToRemove)
    {
        RemoveLink(LinkID);
    }
}

// ========================================
// INTERNAL METHODS
// ========================================

void UCausalChainTracking::ProcessAutoInference()
{
    InferCausality(Config.AutoInferenceInterval * 2.0f);
}

void UCausalChainTracking::DecayLinks(float DeltaTime)
{
    float DecayAmount = Config.LinkDecayRate * DeltaTime;

    for (auto& Pair : Links)
    {
        Pair.Value.Strength -= DecayAmount;
        if (Pair.Value.Strength < 0.0f)
        {
            Pair.Value.Strength = 0.0f;
            Pair.Value.bIsActive = false;
        }
    }
}

float UCausalChainTracking::ComputeEventSimilarity(const FCausalEvent& A, const FCausalEvent& B) const
{
    if (A.ContentVector.Num() == 0 || B.ContentVector.Num() == 0)
    {
        return 0.5f; // Default moderate similarity
    }

    if (A.ContentVector.Num() != B.ContentVector.Num())
    {
        return 0.0f;
    }

    // Compute cosine similarity
    float DotProduct = 0.0f;
    float NormA = 0.0f;
    float NormB = 0.0f;

    for (int32 i = 0; i < A.ContentVector.Num(); ++i)
    {
        DotProduct += A.ContentVector[i] * B.ContentVector[i];
        NormA += A.ContentVector[i] * A.ContentVector[i];
        NormB += B.ContentVector[i] * B.ContentVector[i];
    }

    NormA = FMath::Sqrt(NormA);
    NormB = FMath::Sqrt(NormB);

    if (NormA < KINDA_SMALL_NUMBER || NormB < KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    return FMath::Clamp(DotProduct / (NormA * NormB), 0.0f, 1.0f);
}

float UCausalChainTracking::ComputeTemporalPlausibility(const FCausalEvent& Cause, const FCausalEvent& Effect) const
{
    float Delay = Effect.Timestamp - Cause.Timestamp;

    // Cause must precede effect
    if (Delay < 0.0f)
    {
        return 0.0f;
    }

    // Immediate effects are most plausible
    if (Delay < 0.1f)
    {
        return 1.0f;
    }

    // Exponential decay with configurable window
    float Decay = FMath::Exp(-Delay / Config.MaxCausalWindow);
    return FMath::Clamp(Decay, 0.0f, 1.0f);
}

TArray<int64> UCausalChainTracking::FindCausalPath(int64 StartEventID, int64 EndEventID, int32 MaxDepth) const
{
    TArray<int64> Path;

    if (!EventExists(StartEventID) || !EventExists(EndEventID))
    {
        return Path;
    }

    if (StartEventID == EndEventID)
    {
        Path.Add(StartEventID);
        return Path;
    }

    // BFS to find shortest path
    TMap<int64, int64> Predecessors;
    TArray<int64> Queue;
    TSet<int64> Visited;

    Queue.Add(StartEventID);
    Visited.Add(StartEventID);
    Predecessors.Add(StartEventID, -1);

    int32 Depth = 0;
    int32 LevelSize = 1;
    int32 NextLevelSize = 0;

    while (Queue.Num() > 0 && Depth < MaxDepth)
    {
        int64 Current = Queue[0];
        Queue.RemoveAt(0);
        LevelSize--;

        TArray<FCausalLink> Effects = GetEffectsOf(Current);
        for (const FCausalLink& Link : Effects)
        {
            if (!Visited.Contains(Link.EffectEventID))
            {
                Visited.Add(Link.EffectEventID);
                Predecessors.Add(Link.EffectEventID, Current);
                Queue.Add(Link.EffectEventID);
                NextLevelSize++;

                if (Link.EffectEventID == EndEventID)
                {
                    // Found! Reconstruct path
                    int64 Node = EndEventID;
                    while (Node != -1)
                    {
                        Path.Insert(Node, 0);
                        const int64* Pred = Predecessors.Find(Node);
                        Node = Pred ? *Pred : -1;
                    }
                    return Path;
                }
            }
        }

        if (LevelSize == 0)
        {
            LevelSize = NextLevelSize;
            NextLevelSize = 0;
            Depth++;
        }
    }

    return Path; // Empty if no path found
}

void UCausalChainTracking::UpdateChainStatistics(int64 ChainID)
{
    FCausalChain* Chain = Chains.Find(ChainID);
    if (!Chain)
    {
        return;
    }

    // Recompute strength and confidence
    float MinStrength = 1.0f;
    float TotalConfidence = 0.0f;
    int32 LinkCount = 0;

    for (int64 LinkID : Chain->LinkIDs)
    {
        const FCausalLink* Link = Links.Find(LinkID);
        if (Link)
        {
            MinStrength = FMath::Min(MinStrength, Link->Strength);
            TotalConfidence += Link->ConfidenceValue;
            LinkCount++;
        }
    }

    Chain->OverallStrength = MinStrength;
    Chain->OverallConfidence = LinkCount > 0 ? TotalConfidence / LinkCount : 0.0f;
}

float UCausalChainTracking::GetCurrentTime() const
{
    return AccumulatedTime;
}

int64 UCausalChainTracking::GenerateEventID()
{
    return NextEventID++;
}

int64 UCausalChainTracking::GenerateLinkID()
{
    return NextLinkID++;
}

int64 UCausalChainTracking::GenerateChainID()
{
    return NextChainID++;
}

int64 UCausalChainTracking::GeneratePredictionID()
{
    return NextPredictionID++;
}

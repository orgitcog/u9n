// TemporalAttentionMechanism.cpp
// Implementation of Temporal Attention Mechanism for focusing on relevant temporal windows and events
// Feature F1.5.4 - B-Series Temporal Integration / Temporal Attention Mechanism

#include "TemporalAttentionMechanism.h"
#include "AttentionSystem.h"
#include "ECANAttentionAllocation.h"
#include "../Memory/EpisodicMemorySystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "Math/UnrealMathUtility.h"

UTemporalAttentionMechanism::UTemporalAttentionMechanism()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UTemporalAttentionMechanism::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeTemporalAttention();
}

void UTemporalAttentionMechanism::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (Config.bEnableAutoUpdate)
    {
        UpdateAttentionWeights(DeltaTime);
        UpdateEventSalience(DeltaTime);
        ProcessEventExpiry();
        UpdateFocusedEvents();
    }
}

void UTemporalAttentionMechanism::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        AttentionSystem = Owner->FindComponentByClass<UAttentionSystem>();
        ECANSystem = Owner->FindComponentByClass<UECANAttentionAllocation>();
        MemorySystem = Owner->FindComponentByClass<UEpisodicMemorySystem>();
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
    }
}

void UTemporalAttentionMechanism::InitializeTemporalAttention()
{
    Events.Empty();
    Windows.Empty();
    FocusedEventIDs.Empty();
    TagBoosts.Empty();
    
    CurrentFocusWindowID = TEXT("");
    CurrentMode = ETemporalAttentionMode::Present;
    EventIDCounter = 0;
    WindowIDCounter = 0;
    
    CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

// ========================================
// EVENT MANAGEMENT
// ========================================

FTemporalEvent UTemporalAttentionMechanism::RegisterEvent(const FString& EventName,
                                                           ETemporalEventImportance Importance,
                                                           float Duration,
                                                           const TArray<FString>& Tags)
{
    // Check capacity
    if (Events.Num() >= Config.MaxEvents)
    {
        ProcessEventExpiry();
        
        // If still at capacity, remove lowest salience event
        if (Events.Num() >= Config.MaxEvents)
        {
            float LowestSalience = FLT_MAX;
            FString LowestID;
            for (const auto& Pair : Events)
            {
                if (Pair.Value.SalienceScore < LowestSalience)
                {
                    LowestSalience = Pair.Value.SalienceScore;
                    LowestID = Pair.Key;
                }
            }
            if (!LowestID.IsEmpty())
            {
                RemoveEvent(LowestID);
            }
        }
    }

    FTemporalEvent Event;
    Event.EventID = GenerateEventID();
    Event.EventName = EventName;
    Event.Timestamp = CurrentTime;
    Event.Duration = Duration;
    Event.Importance = Importance;
    Event.Tags = Tags;
    Event.LastAccessTime = CurrentTime;
    Event.AccessCount = 0;

    // Compute initial salience
    Event.SalienceScore = ComputeImportanceSalience(Event);
    Event.AttentionWeight = Event.SalienceScore;

    Events.Add(Event.EventID, Event);

    OnTemporalEventRegistered.Broadcast(Event);

    return Event;
}

FTemporalEvent UTemporalAttentionMechanism::RegisterEmotionalEvent(const FString& EventName,
                                                                    ETemporalEventImportance Importance,
                                                                    float EmotionalValence,
                                                                    float EmotionalArousal,
                                                                    const TArray<FString>& Tags)
{
    FTemporalEvent Event = RegisterEvent(EventName, Importance, 0.0f, Tags);
    
    // Update with emotional context
    if (Events.Contains(Event.EventID))
    {
        FTemporalEvent& StoredEvent = Events[Event.EventID];
        StoredEvent.EmotionalValence = FMath::Clamp(EmotionalValence, -1.0f, 1.0f);
        StoredEvent.EmotionalArousal = FMath::Clamp(EmotionalArousal, 0.0f, 1.0f);
        
        // Recompute salience with emotional component
        StoredEvent.SalienceScore = ComputeImportanceSalience(StoredEvent) + 
                                    ComputeEmotionalSalience(StoredEvent);
        StoredEvent.AttentionWeight = StoredEvent.SalienceScore;
        
        return StoredEvent;
    }
    
    return Event;
}

void UTemporalAttentionMechanism::UpdateEventImportance(const FString& EventID, 
                                                         ETemporalEventImportance NewImportance)
{
    if (Events.Contains(EventID))
    {
        Events[EventID].Importance = NewImportance;
        Events[EventID].SalienceScore = ComputeImportanceSalience(Events[EventID]) +
                                        ComputeRecencySalience(Events[EventID]) +
                                        ComputeEmotionalSalience(Events[EventID]);
    }
}

void UTemporalAttentionMechanism::SetEventFeatures(const FString& EventID, const TArray<float>& Features)
{
    if (Events.Contains(EventID))
    {
        Events[EventID].FeatureVector = Features;
    }
}

void UTemporalAttentionMechanism::LinkEvents(const FString& EventID1, const FString& EventID2)
{
    if (Events.Contains(EventID1) && Events.Contains(EventID2))
    {
        // Bidirectional linking
        if (!Events[EventID1].LinkedEventIDs.Contains(EventID2))
        {
            Events[EventID1].LinkedEventIDs.Add(EventID2);
        }
        if (!Events[EventID2].LinkedEventIDs.Contains(EventID1))
        {
            Events[EventID2].LinkedEventIDs.Add(EventID1);
        }
    }
}

FTemporalEvent UTemporalAttentionMechanism::GetEvent(const FString& EventID) const
{
    if (Events.Contains(EventID))
    {
        return Events[EventID];
    }
    return FTemporalEvent();
}

bool UTemporalAttentionMechanism::EventExists(const FString& EventID) const
{
    return Events.Contains(EventID);
}

void UTemporalAttentionMechanism::RemoveEvent(const FString& EventID)
{
    if (Events.Contains(EventID))
    {
        // Remove from focused events
        FocusedEventIDs.Remove(EventID);
        
        // Remove from linked events
        for (auto& Pair : Events)
        {
            Pair.Value.LinkedEventIDs.Remove(EventID);
        }
        
        // Remove from windows
        for (auto& WindowPair : Windows)
        {
            WindowPair.Value.EventIDs.Remove(EventID);
        }
        
        OnTemporalEventExpired.Broadcast(EventID);
        Events.Remove(EventID);
    }
}

// ========================================
// TEMPORAL WINDOW MANAGEMENT
// ========================================

FTemporalWindow UTemporalAttentionMechanism::CreateWindow(const FString& Label, 
                                                           float StartTime, 
                                                           float EndTime)
{
    // Check capacity
    if (Windows.Num() >= Config.MaxWindows)
    {
        // Remove oldest non-focus window
        float OldestTime = FLT_MAX;
        FString OldestID;
        for (const auto& Pair : Windows)
        {
            if (!Pair.Value.bIsFocusWindow && Pair.Value.StartTime < OldestTime)
            {
                OldestTime = Pair.Value.StartTime;
                OldestID = Pair.Key;
            }
        }
        if (!OldestID.IsEmpty())
        {
            Windows.Remove(OldestID);
        }
    }

    FTemporalWindow Window;
    Window.WindowID = GenerateWindowID();
    Window.Label = Label;
    Window.StartTime = StartTime;
    Window.EndTime = EndTime;
    
    // Determine scale
    float Duration = EndTime - StartTime;
    if (Duration <= Config.ImmediateBoundary)
    {
        Window.Scale = ETemporalScale::Immediate;
    }
    else if (Duration <= Config.ShortTermBoundary)
    {
        Window.Scale = ETemporalScale::ShortTerm;
    }
    else if (Duration <= Config.MediumTermBoundary)
    {
        Window.Scale = ETemporalScale::MediumTerm;
    }
    else if (Duration <= Config.LongTermBoundary)
    {
        Window.Scale = ETemporalScale::LongTerm;
    }
    else
    {
        Window.Scale = ETemporalScale::Extended;
    }

    Window.AttentionWeight = TemporalWeights.GetWeight(Window.Scale);

    Windows.Add(Window.WindowID, Window);
    
    // Populate events
    UpdateWindowEvents(Window.WindowID);

    OnTemporalWindowCreated.Broadcast(Window);

    return Window;
}

FTemporalWindow UTemporalAttentionMechanism::GetWindow(const FString& WindowID) const
{
    if (Windows.Contains(WindowID))
    {
        return Windows[WindowID];
    }
    return FTemporalWindow();
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetEventsInRange(float StartTime, float EndTime) const
{
    TArray<FTemporalEvent> Result;
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        float EventEnd = Event.Timestamp + Event.Duration;
        
        // Check if event overlaps with range
        if (Event.Timestamp <= EndTime && EventEnd >= StartTime)
        {
            Result.Add(Event);
        }
    }
    
    return Result;
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetEventsByScale(ETemporalScale Scale) const
{
    TArray<FTemporalEvent> Result;
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        float Age = CurrentTime - Event.Timestamp;
        ETemporalScale EventScale = GetScaleForTime(Age);
        
        if (EventScale == Scale)
        {
            Result.Add(Event);
        }
    }
    
    return Result;
}

FTemporalWindow UTemporalAttentionMechanism::GetCurrentFocusWindow() const
{
    if (!CurrentFocusWindowID.IsEmpty() && Windows.Contains(CurrentFocusWindowID))
    {
        return Windows[CurrentFocusWindowID];
    }
    return FTemporalWindow();
}

void UTemporalAttentionMechanism::SetFocusWindow(const FString& WindowID)
{
    if (Windows.Contains(WindowID))
    {
        // Clear previous focus
        if (!CurrentFocusWindowID.IsEmpty() && Windows.Contains(CurrentFocusWindowID))
        {
            Windows[CurrentFocusWindowID].bIsFocusWindow = false;
        }
        
        CurrentFocusWindowID = WindowID;
        Windows[WindowID].bIsFocusWindow = true;
        
        // Focus on events in window
        FocusedEventIDs.Empty();
        for (const FString& EventID : Windows[WindowID].EventIDs)
        {
            if (FocusedEventIDs.Num() < Config.MaxFocusEvents)
            {
                FocusedEventIDs.Add(EventID);
            }
        }
    }
}

// ========================================
// ATTENTION CONTROL
// ========================================

bool UTemporalAttentionMechanism::FocusOnEvent(const FString& EventID)
{
    if (!Events.Contains(EventID))
    {
        return false;
    }

    FString OldFocusID = FocusedEventIDs.Num() > 0 ? FocusedEventIDs[0] : TEXT("");

    // Clear current focus if at capacity
    if (FocusedEventIDs.Num() >= Config.MaxFocusEvents)
    {
        FocusedEventIDs.RemoveAt(0);
    }

    FocusedEventIDs.AddUnique(EventID);
    
    // Update access
    Events[EventID].LastAccessTime = CurrentTime;
    Events[EventID].AccessCount++;

    OnTemporalFocusChanged.Broadcast(OldFocusID, EventID);

    return true;
}

bool UTemporalAttentionMechanism::FocusOnTimeRange(float StartTime, float EndTime)
{
    // Create or reuse window for this range
    FTemporalWindow Window = CreateWindow(TEXT("FocusRange"), StartTime, EndTime);
    SetFocusWindow(Window.WindowID);
    
    return true;
}

void UTemporalAttentionMechanism::SetAttentionMode(ETemporalAttentionMode NewMode)
{
    if (CurrentMode != NewMode)
    {
        ETemporalAttentionMode OldMode = CurrentMode;
        CurrentMode = NewMode;
        
        // Adjust weights based on mode
        switch (NewMode)
        {
            case ETemporalAttentionMode::Present:
                TemporalWeights.ImmediateWeight = 0.5f;
                TemporalWeights.ShortTermWeight = 0.3f;
                TemporalWeights.MediumTermWeight = 0.1f;
                TemporalWeights.LongTermWeight = 0.05f;
                TemporalWeights.ExtendedWeight = 0.05f;
                break;
                
            case ETemporalAttentionMode::Retrospective:
                TemporalWeights.ImmediateWeight = 0.1f;
                TemporalWeights.ShortTermWeight = 0.2f;
                TemporalWeights.MediumTermWeight = 0.3f;
                TemporalWeights.LongTermWeight = 0.25f;
                TemporalWeights.ExtendedWeight = 0.15f;
                break;
                
            case ETemporalAttentionMode::Prospective:
                // Future focus - weight immediate for planning
                TemporalWeights.ImmediateWeight = 0.4f;
                TemporalWeights.ShortTermWeight = 0.35f;
                TemporalWeights.MediumTermWeight = 0.15f;
                TemporalWeights.LongTermWeight = 0.05f;
                TemporalWeights.ExtendedWeight = 0.05f;
                break;
                
            case ETemporalAttentionMode::Comparative:
                // Even distribution for comparison
                TemporalWeights.ImmediateWeight = 0.25f;
                TemporalWeights.ShortTermWeight = 0.25f;
                TemporalWeights.MediumTermWeight = 0.2f;
                TemporalWeights.LongTermWeight = 0.15f;
                TemporalWeights.ExtendedWeight = 0.15f;
                break;
                
            case ETemporalAttentionMode::Anticipatory:
                // Focus on patterns from past
                TemporalWeights.ImmediateWeight = 0.3f;
                TemporalWeights.ShortTermWeight = 0.3f;
                TemporalWeights.MediumTermWeight = 0.2f;
                TemporalWeights.LongTermWeight = 0.15f;
                TemporalWeights.ExtendedWeight = 0.05f;
                break;
        }
        
        OnTemporalModeChanged.Broadcast(OldMode, NewMode);
    }
}

ETemporalAttentionMode UTemporalAttentionMechanism::GetAttentionMode() const
{
    return CurrentMode;
}

void UTemporalAttentionMechanism::SetTemporalWeights(const FTemporalAttentionWeights& NewWeights)
{
    TemporalWeights = NewWeights;
}

void UTemporalAttentionMechanism::BoostTagAttention(const TArray<FString>& Tags, float BoostAmount)
{
    for (const FString& Tag : Tags)
    {
        if (TagBoosts.Contains(Tag))
        {
            TagBoosts[Tag] += BoostAmount;
        }
        else
        {
            TagBoosts.Add(Tag, BoostAmount);
        }
    }
}

void UTemporalAttentionMechanism::ReleaseFocus()
{
    FocusedEventIDs.Empty();
    
    if (!CurrentFocusWindowID.IsEmpty() && Windows.Contains(CurrentFocusWindowID))
    {
        Windows[CurrentFocusWindowID].bIsFocusWindow = false;
    }
    CurrentFocusWindowID = TEXT("");
}

// ========================================
// SALIENCE COMPUTATION
// ========================================

float UTemporalAttentionMechanism::ComputeEventSalience(const FString& EventID) const
{
    if (!Events.Contains(EventID))
    {
        return 0.0f;
    }

    const FTemporalEvent& Event = Events[EventID];
    
    float Recency = ComputeRecencySalience(Event) * Config.RecencyWeight;
    float Importance = ComputeImportanceSalience(Event) * Config.ImportanceWeight;
    float Emotional = ComputeEmotionalSalience(Event) * Config.EmotionalWeight;
    float TagBoost = ComputeTagBoost(Event);
    
    return FMath::Clamp(Recency + Importance + Emotional + TagBoost, 0.0f, 1.0f);
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetMostSalientEvents(int32 Count) const
{
    TArray<FTemporalEvent> AllEvents;
    for (const auto& Pair : Events)
    {
        AllEvents.Add(Pair.Value);
    }
    
    // Sort by salience
    AllEvents.Sort([](const FTemporalEvent& A, const FTemporalEvent& B)
    {
        return A.SalienceScore > B.SalienceScore;
    });
    
    // Return top N
    if (AllEvents.Num() > Count)
    {
        AllEvents.SetNum(Count);
    }
    
    return AllEvents;
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetMostSalientInRange(float StartTime, 
                                                                           float EndTime, 
                                                                           int32 Count) const
{
    TArray<FTemporalEvent> InRange = GetEventsInRange(StartTime, EndTime);
    
    // Sort by salience
    InRange.Sort([](const FTemporalEvent& A, const FTemporalEvent& B)
    {
        return A.SalienceScore > B.SalienceScore;
    });
    
    // Return top N
    if (InRange.Num() > Count)
    {
        InRange.SetNum(Count);
    }
    
    return InRange;
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetEventsByImportance(ETemporalEventImportance MinImportance) const
{
    TArray<FTemporalEvent> Result;
    
    for (const auto& Pair : Events)
    {
        if (static_cast<uint8>(Pair.Value.Importance) >= static_cast<uint8>(MinImportance))
        {
            Result.Add(Pair.Value);
        }
    }
    
    return Result;
}

// ========================================
// TEMPORAL QUERIES
// ========================================

FTemporalQueryResult UTemporalAttentionMechanism::QueryByTags(const TArray<FString>& Tags, 
                                                               int32 MaxResults) const
{
    FTemporalQueryResult Result;
    Result.QueryStartTime = 0.0f;
    Result.QueryEndTime = CurrentTime;
    
    TArray<FTemporalEvent> Matches;
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        
        // Check if any tag matches
        bool bHasMatch = false;
        for (const FString& Tag : Tags)
        {
            if (Event.Tags.Contains(Tag))
            {
                bHasMatch = true;
                break;
            }
        }
        
        if (bHasMatch)
        {
            Matches.Add(Event);
        }
    }
    
    // Sort by salience
    Matches.Sort([](const FTemporalEvent& A, const FTemporalEvent& B)
    {
        return A.SalienceScore > B.SalienceScore;
    });
    
    Result.TotalMatches = Matches.Num();
    
    if (Matches.Num() > MaxResults)
    {
        Matches.SetNum(MaxResults);
    }
    
    Result.Events = Matches;
    
    // Compute average attention weight
    float TotalWeight = 0.0f;
    for (const FTemporalEvent& Event : Result.Events)
    {
        TotalWeight += Event.AttentionWeight;
    }
    Result.AverageAttentionWeight = Result.Events.Num() > 0 ? TotalWeight / Result.Events.Num() : 0.0f;
    
    return Result;
}

FTemporalQueryResult UTemporalAttentionMechanism::QueryByEmotion(float TargetValence, 
                                                                  float ValenceTolerance, 
                                                                  int32 MaxResults) const
{
    FTemporalQueryResult Result;
    Result.QueryStartTime = 0.0f;
    Result.QueryEndTime = CurrentTime;
    
    TArray<FTemporalEvent> Matches;
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        
        if (FMath::Abs(Event.EmotionalValence - TargetValence) <= ValenceTolerance)
        {
            Matches.Add(Event);
        }
    }
    
    // Sort by emotional match closeness
    Matches.Sort([TargetValence](const FTemporalEvent& A, const FTemporalEvent& B)
    {
        float DiffA = FMath::Abs(A.EmotionalValence - TargetValence);
        float DiffB = FMath::Abs(B.EmotionalValence - TargetValence);
        return DiffA < DiffB;
    });
    
    Result.TotalMatches = Matches.Num();
    
    if (Matches.Num() > MaxResults)
    {
        Matches.SetNum(MaxResults);
    }
    
    Result.Events = Matches;
    
    return Result;
}

FTemporalQueryResult UTemporalAttentionMechanism::QueryRecent(float SecondsAgo, int32 MaxResults) const
{
    float StartTime = CurrentTime - SecondsAgo;
    return FTemporalQueryResult{GetMostSalientInRange(StartTime, CurrentTime, MaxResults),
                                StartTime, CurrentTime, 0, 0.0f};
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetLinkedEvents(const FString& EventID, 
                                                                     int32 MaxDepth) const
{
    TArray<FTemporalEvent> Result;
    TSet<FString> Visited;
    TArray<FString> ToProcess;
    
    ToProcess.Add(EventID);
    int32 CurrentDepth = 0;
    
    while (ToProcess.Num() > 0 && CurrentDepth < MaxDepth)
    {
        TArray<FString> NextLevel;
        
        for (const FString& ID : ToProcess)
        {
            if (Visited.Contains(ID))
            {
                continue;
            }
            
            Visited.Add(ID);
            
            if (Events.Contains(ID))
            {
                if (ID != EventID) // Don't include source event
                {
                    Result.Add(Events[ID]);
                }
                
                // Add linked events for next level
                for (const FString& LinkedID : Events[ID].LinkedEventIDs)
                {
                    if (!Visited.Contains(LinkedID))
                    {
                        NextLevel.Add(LinkedID);
                    }
                }
            }
        }
        
        ToProcess = NextLevel;
        CurrentDepth++;
    }
    
    return Result;
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::QuerySimilarEvents(const TArray<float>& Features, 
                                                                        float Threshold, 
                                                                        int32 MaxResults) const
{
    TArray<TPair<float, FTemporalEvent>> ScoredEvents;
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        
        if (Event.FeatureVector.Num() == 0 || Event.FeatureVector.Num() != Features.Num())
        {
            continue;
        }
        
        // Compute cosine similarity
        float DotProduct = 0.0f;
        float NormA = 0.0f;
        float NormB = 0.0f;
        
        for (int32 i = 0; i < Features.Num(); ++i)
        {
            DotProduct += Features[i] * Event.FeatureVector[i];
            NormA += Features[i] * Features[i];
            NormB += Event.FeatureVector[i] * Event.FeatureVector[i];
        }
        
        float Similarity = 0.0f;
        if (NormA > 0.0f && NormB > 0.0f)
        {
            Similarity = DotProduct / (FMath::Sqrt(NormA) * FMath::Sqrt(NormB));
        }
        
        if (Similarity >= Threshold)
        {
            ScoredEvents.Add(TPair<float, FTemporalEvent>(Similarity, Event));
        }
    }
    
    // Sort by similarity
    ScoredEvents.Sort([](const TPair<float, FTemporalEvent>& A, const TPair<float, FTemporalEvent>& B)
    {
        return A.Key > B.Key;
    });
    
    TArray<FTemporalEvent> Result;
    for (int32 i = 0; i < FMath::Min(ScoredEvents.Num(), MaxResults); ++i)
    {
        Result.Add(ScoredEvents[i].Value);
    }
    
    return Result;
}

// ========================================
// STATE QUERIES
// ========================================

FTemporalAttentionState UTemporalAttentionMechanism::GetAttentionState() const
{
    FTemporalAttentionState State;
    State.Mode = CurrentMode;
    State.CurrentWindowID = CurrentFocusWindowID;
    State.FocusedEventIDs = FocusedEventIDs;
    State.Weights = TemporalWeights;
    State.TotalEvents = Events.Num();
    State.LastUpdateTime = CurrentTime;
    
    // Compute capacity used
    float TotalAttention = 0.0f;
    for (const auto& Pair : Events)
    {
        TotalAttention += Pair.Value.AttentionWeight;
    }
    State.AttentionCapacityUsed = TotalAttention / FMath::Max(1, Events.Num());
    
    return State;
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetAllEvents() const
{
    TArray<FTemporalEvent> Result;
    for (const auto& Pair : Events)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FTemporalWindow> UTemporalAttentionMechanism::GetAllWindows() const
{
    TArray<FTemporalWindow> Result;
    for (const auto& Pair : Windows)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FTemporalEvent> UTemporalAttentionMechanism::GetFocusedEvents() const
{
    TArray<FTemporalEvent> Result;
    for (const FString& EventID : FocusedEventIDs)
    {
        if (Events.Contains(EventID))
        {
            Result.Add(Events[EventID]);
        }
    }
    return Result;
}

int32 UTemporalAttentionMechanism::GetEventCount() const
{
    return Events.Num();
}

ETemporalScale UTemporalAttentionMechanism::GetScaleForTime(float TimeOffset) const
{
    if (TimeOffset <= Config.ImmediateBoundary)
    {
        return ETemporalScale::Immediate;
    }
    else if (TimeOffset <= Config.ShortTermBoundary)
    {
        return ETemporalScale::ShortTerm;
    }
    else if (TimeOffset <= Config.MediumTermBoundary)
    {
        return ETemporalScale::MediumTerm;
    }
    else if (TimeOffset <= Config.LongTermBoundary)
    {
        return ETemporalScale::LongTerm;
    }
    return ETemporalScale::Extended;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UTemporalAttentionMechanism::UpdateAttentionWeights(float DeltaTime)
{
    for (auto& Pair : Events)
    {
        FTemporalEvent& Event = Pair.Value;
        
        // Apply temporal decay
        float Age = CurrentTime - Event.Timestamp;
        float DecayFactor = ComputeTemporalDecay(Age);
        
        // Get base weight from temporal scale
        ETemporalScale Scale = GetScaleForTime(Age);
        float ScaleWeight = TemporalWeights.GetWeight(Scale);
        
        // Update attention weight
        Event.AttentionWeight = FMath::Max(
            Config.AttentionThreshold,
            Event.SalienceScore * DecayFactor * ScaleWeight
        );
    }
}

void UTemporalAttentionMechanism::UpdateEventSalience(float DeltaTime)
{
    for (auto& Pair : Events)
    {
        FTemporalEvent& Event = Pair.Value;
        
        float Recency = ComputeRecencySalience(Event) * Config.RecencyWeight;
        float Importance = ComputeImportanceSalience(Event) * Config.ImportanceWeight;
        float Emotional = ComputeEmotionalSalience(Event) * Config.EmotionalWeight;
        float TagBoost = ComputeTagBoost(Event);
        
        Event.SalienceScore = FMath::Clamp(Recency + Importance + Emotional + TagBoost, 0.0f, 1.0f);
    }
    
    // Update window salience
    for (auto& Pair : Windows)
    {
        UpdateWindowEvents(Pair.Key);
        
        float TotalSalience = 0.0f;
        for (const FString& EventID : Pair.Value.EventIDs)
        {
            if (Events.Contains(EventID))
            {
                TotalSalience += Events[EventID].SalienceScore;
            }
        }
        Pair.Value.WindowSalience = Pair.Value.EventIDs.Num() > 0 ? 
            TotalSalience / Pair.Value.EventIDs.Num() : 0.0f;
    }
}

void UTemporalAttentionMechanism::ProcessEventExpiry()
{
    TArray<FString> ToRemove;
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        
        // Remove events below threshold with no focus
        if (Event.AttentionWeight < Config.AttentionThreshold && 
            !FocusedEventIDs.Contains(Event.EventID))
        {
            float Age = CurrentTime - Event.Timestamp;
            
            // Only expire if old enough (beyond medium-term)
            if (Age > Config.MediumTermBoundary)
            {
                ToRemove.Add(Event.EventID);
            }
        }
    }
    
    for (const FString& EventID : ToRemove)
    {
        RemoveEvent(EventID);
    }
}

void UTemporalAttentionMechanism::UpdateFocusedEvents()
{
    // Remove expired focused events
    for (int32 i = FocusedEventIDs.Num() - 1; i >= 0; --i)
    {
        if (!Events.Contains(FocusedEventIDs[i]))
        {
            FocusedEventIDs.RemoveAt(i);
        }
    }
}

float UTemporalAttentionMechanism::ComputeRecencySalience(const FTemporalEvent& Event) const
{
    float Age = CurrentTime - Event.Timestamp;
    return ComputeTemporalDecay(Age);
}

float UTemporalAttentionMechanism::ComputeImportanceSalience(const FTemporalEvent& Event) const
{
    switch (Event.Importance)
    {
        case ETemporalEventImportance::Background:
            return 0.1f;
        case ETemporalEventImportance::Routine:
            return 0.3f;
        case ETemporalEventImportance::Notable:
            return 0.5f;
        case ETemporalEventImportance::Critical:
            return 0.8f;
        case ETemporalEventImportance::Landmark:
            return 1.0f;
        default:
            return 0.3f;
    }
}

float UTemporalAttentionMechanism::ComputeEmotionalSalience(const FTemporalEvent& Event) const
{
    // Combine valence intensity with arousal
    float ValenceIntensity = FMath::Abs(Event.EmotionalValence);
    return (ValenceIntensity + Event.EmotionalArousal) * 0.5f;
}

float UTemporalAttentionMechanism::ComputeTagBoost(const FTemporalEvent& Event) const
{
    float TotalBoost = 0.0f;
    
    for (const FString& Tag : Event.Tags)
    {
        if (TagBoosts.Contains(Tag))
        {
            TotalBoost += TagBoosts[Tag];
        }
    }
    
    return FMath::Clamp(TotalBoost, 0.0f, 0.5f); // Cap tag boost
}

float UTemporalAttentionMechanism::ComputeTemporalDecay(float Age) const
{
    // Exponential decay with configured rate
    return FMath::Exp(-Config.TemporalDecayRate * Age);
}

FString UTemporalAttentionMechanism::GenerateEventID()
{
    return FString::Printf(TEXT("TEMP_EVT_%d_%d"), ++EventIDCounter, FMath::RandRange(1000, 9999));
}

FString UTemporalAttentionMechanism::GenerateWindowID()
{
    return FString::Printf(TEXT("TEMP_WIN_%d_%d"), ++WindowIDCounter, FMath::RandRange(1000, 9999));
}

void UTemporalAttentionMechanism::UpdateWindowEvents(const FString& WindowID)
{
    if (!Windows.Contains(WindowID))
    {
        return;
    }
    
    FTemporalWindow& Window = Windows[WindowID];
    Window.EventIDs.Empty();
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        float EventEnd = Event.Timestamp + Event.Duration;
        
        // Check if event overlaps with window
        if (Event.Timestamp <= Window.EndTime && EventEnd >= Window.StartTime)
        {
            Window.EventIDs.Add(Event.EventID);
        }
    }
}

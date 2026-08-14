// AttentionSystem.cpp
// Implementation of Selective Attention and Salience Computation

#include "AttentionSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Wisdom/RelevanceRealizationEnnead.h"
#include "../Memory/MemorySystems.h"
#include "Math/UnrealMathUtility.h"

UAttentionSystem::UAttentionSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UAttentionSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeAttention();
}

void UAttentionSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                      FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateSalienceScores(DeltaTime);
    UpdateSpotlight(DeltaTime);
    UpdateFatigue(DeltaTime);
    CheckForInvoluntaryShifts();
}

void UAttentionSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        EnneadComponent = Owner->FindComponentByClass<URelevanceRealizationEnnead>();
        MemoryComponent = Owner->FindComponentByClass<UMemorySystems>();
    }
}

void UAttentionSystem::InitializeAttention()
{
    Spotlight.Mode = EAttentionMode::Diffuse;
    Spotlight.Intensity = 1.0f;
    Spotlight.FocusRadius = 200.0f;
    Spotlight.FatigueLevel = 0.0f;

    Targets.Empty();
    ShiftHistory.Empty();
    GoalSalienceMap.Empty();

    Alertness = 1.0f;
    AttentionalCapacity = 1.0f;
}

// ========================================
// TARGET MANAGEMENT
// ========================================

FAttentionTarget UAttentionSystem::RegisterTarget(const FString& Name, FVector Location, 
                                                   float InitialSalience, const TArray<FString>& Tags)
{
    FAttentionTarget Target;
    Target.TargetID = GenerateTargetID();
    Target.TargetName = Name;
    Target.WorldLocation = Location;
    Target.BottomUpSalience = InitialSalience;
    Target.SemanticTags = Tags;
    Target.SalienceScore = ComputeCombinedSalience(Target);

    Targets.Add(Target);

    // Limit active targets
    while (Targets.Num() > MaxActiveTargets)
    {
        // Remove lowest salience target
        int32 LowestIndex = 0;
        float LowestSalience = Targets[0].SalienceScore;
        for (int32 i = 1; i < Targets.Num(); ++i)
        {
            if (Targets[i].SalienceScore < LowestSalience && !Targets[i].bIsCurrentFocus)
            {
                LowestSalience = Targets[i].SalienceScore;
                LowestIndex = i;
            }
        }
        OnTargetLost.Broadcast(Targets[LowestIndex].TargetID);
        Targets.RemoveAt(LowestIndex);
    }

    OnTargetAcquired.Broadcast(Target);

    return Target;
}

void UAttentionSystem::UpdateTargetLocation(const FString& TargetID, FVector NewLocation)
{
    int32 Index = FindTargetIndex(TargetID);
    if (Index >= 0)
    {
        Targets[Index].WorldLocation = NewLocation;

        // Update spotlight if this is current focus
        if (Targets[Index].bIsCurrentFocus)
        {
            Spotlight.FocusLocation = NewLocation;
        }
    }
}

void UAttentionSystem::UpdateTargetSalience(const FString& TargetID, ESalienceSource Source, float Salience)
{
    int32 Index = FindTargetIndex(TargetID);
    if (Index >= 0)
    {
        switch (Source)
        {
            case ESalienceSource::BottomUp:
                Targets[Index].BottomUpSalience = Salience;
                break;
            case ESalienceSource::TopDown:
                Targets[Index].TopDownSalience = Salience;
                break;
            case ESalienceSource::Emotional:
                Targets[Index].EmotionalSalience = Salience;
                break;
            case ESalienceSource::Novelty:
                Targets[Index].NoveltySalience = Salience;
                break;
            default:
                break;
        }

        Targets[Index].SalienceScore = ComputeCombinedSalience(Targets[Index]);
    }
}

void UAttentionSystem::RemoveTarget(const FString& TargetID)
{
    int32 Index = FindTargetIndex(TargetID);
    if (Index >= 0)
    {
        if (Targets[Index].bIsCurrentFocus)
        {
            ReleaseFocus();
        }
        OnTargetLost.Broadcast(TargetID);
        Targets.RemoveAt(Index);
    }
}

FAttentionTarget UAttentionSystem::GetTarget(const FString& TargetID) const
{
    int32 Index = FindTargetIndex(TargetID);
    if (Index >= 0)
    {
        return Targets[Index];
    }
    return FAttentionTarget();
}

TArray<FAttentionTarget> UAttentionSystem::GetActiveTargets() const
{
    return Targets;
}

FAttentionTarget UAttentionSystem::GetCurrentFocus() const
{
    for (const FAttentionTarget& Target : Targets)
    {
        if (Target.bIsCurrentFocus)
        {
            return Target;
        }
    }
    return FAttentionTarget();
}

// ========================================
// ATTENTION CONTROL
// ========================================

bool UAttentionSystem::ShiftAttentionTo(const FString& TargetID)
{
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Check minimum shift interval
    if (CurrentTime - LastShiftTime < MinShiftInterval)
    {
        return false;
    }

    int32 Index = FindTargetIndex(TargetID);
    if (Index < 0)
    {
        return false;
    }

    PerformAttentionShift(TargetID, ESalienceSource::TopDown, true);
    return true;
}

bool UAttentionSystem::ShiftAttentionToLocation(FVector Location)
{
    // Find nearest target to location
    float NearestDist = FLT_MAX;
    int32 NearestIndex = -1;

    for (int32 i = 0; i < Targets.Num(); ++i)
    {
        float Dist = FVector::Dist(Targets[i].WorldLocation, Location);
        if (Dist < NearestDist)
        {
            NearestDist = Dist;
            NearestIndex = i;
        }
    }

    if (NearestIndex >= 0 && NearestDist < Spotlight.FocusRadius)
    {
        return ShiftAttentionTo(Targets[NearestIndex].TargetID);
    }

    // No target at location, just move spotlight
    Spotlight.FocusLocation = Location;
    Spotlight.CurrentTargetID = TEXT("");

    return true;
}

void UAttentionSystem::SetAttentionMode(EAttentionMode NewMode)
{
    if (Spotlight.Mode != NewMode)
    {
        EAttentionMode OldMode = Spotlight.Mode;
        Spotlight.Mode = NewMode;

        // Adjust focus radius based on mode
        switch (NewMode)
        {
            case EAttentionMode::Diffuse:
                Spotlight.FocusRadius = 400.0f;
                Spotlight.Intensity = 0.5f;
                break;
            case EAttentionMode::Focused:
                Spotlight.FocusRadius = 50.0f;
                Spotlight.Intensity = 1.0f;
                break;
            case EAttentionMode::Divided:
                Spotlight.FocusRadius = 200.0f;
                Spotlight.Intensity = 0.7f;
                break;
            case EAttentionMode::Sustained:
                Spotlight.FocusRadius = 100.0f;
                Spotlight.Intensity = 0.9f;
                break;
            case EAttentionMode::Alternating:
                Spotlight.FocusRadius = 150.0f;
                Spotlight.Intensity = 0.8f;
                break;
        }

        OnAttentionModeChanged.Broadcast(OldMode, NewMode);
    }
}

EAttentionMode UAttentionSystem::GetAttentionMode() const
{
    return Spotlight.Mode;
}

void UAttentionSystem::SetFocusRadius(float Radius)
{
    Spotlight.FocusRadius = FMath::Max(10.0f, Radius);
}

void UAttentionSystem::ReleaseFocus()
{
    for (FAttentionTarget& Target : Targets)
    {
        Target.bIsCurrentFocus = false;
    }
    Spotlight.CurrentTargetID = TEXT("");
    SetAttentionMode(EAttentionMode::Diffuse);
}

// ========================================
// SALIENCE COMPUTATION
// ========================================

float UAttentionSystem::ComputeCombinedSalience(const FAttentionTarget& Target) const
{
    float BottomUp = ComputeBottomUpSalience(Target) * BottomUpWeight;
    float TopDown = ComputeTopDownSalience(Target) * TopDownWeight;
    float Emotional = ComputeEmotionalSalience(Target) * EmotionalWeight;
    float Novelty = ComputeNoveltySalience(Target) * NoveltyWeight;

    return FMath::Clamp(BottomUp + TopDown + Emotional + Novelty, 0.0f, 1.0f);
}

float UAttentionSystem::GetSalienceAtLocation(FVector Location) const
{
    float MaxSalience = 0.0f;

    // Check targets
    for (const FAttentionTarget& Target : Targets)
    {
        float Dist = FVector::Dist(Target.WorldLocation, Location);
        if (Dist < Spotlight.FocusRadius)
        {
            float DistFactor = 1.0f - (Dist / Spotlight.FocusRadius);
            MaxSalience = FMath::Max(MaxSalience, Target.SalienceScore * DistFactor);
        }
    }

    // Check salience map
    for (const FSalienceMapEntry& Entry : SalienceMap)
    {
        float Dist = FVector::Dist(Entry.Location, Location);
        if (Dist < 100.0f)
        {
            float DistFactor = 1.0f - (Dist / 100.0f);
            MaxSalience = FMath::Max(MaxSalience, Entry.Salience * DistFactor);
        }
    }

    return MaxSalience;
}

FAttentionTarget UAttentionSystem::GetMostSalientTarget() const
{
    FAttentionTarget MostSalient;
    float HighestSalience = -1.0f;

    for (const FAttentionTarget& Target : Targets)
    {
        if (Target.SalienceScore > HighestSalience)
        {
            HighestSalience = Target.SalienceScore;
            MostSalient = Target;
        }
    }

    return MostSalient;
}

void UAttentionSystem::AddSalienceBoost(FVector Location, float Boost, ESalienceSource Source)
{
    FSalienceMapEntry Entry;
    Entry.Location = Location;
    Entry.Salience = Boost;
    Entry.PrimarySource = Source;
    Entry.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    SalienceMap.Add(Entry);

    // Limit salience map size
    while (SalienceMap.Num() > 100)
    {
        SalienceMap.RemoveAt(0);
    }
}

void UAttentionSystem::SetGoalSalience(const TArray<FString>& Tags, float Salience)
{
    for (const FString& Tag : Tags)
    {
        GoalSalienceMap.Add(Tag, Salience);
    }

    // Update top-down salience for all targets
    for (FAttentionTarget& Target : Targets)
    {
        Target.TopDownSalience = ComputeTopDownSalience(Target);
        Target.SalienceScore = ComputeCombinedSalience(Target);
    }
}

// ========================================
// STATE QUERIES
// ========================================

FAttentionState UAttentionSystem::GetAttentionState() const
{
    FAttentionState State;
    State.Spotlight = Spotlight;
    State.ActiveTargets = Targets;
    State.CurrentMode = Spotlight.Mode;
    State.OverallAlertness = Alertness;
    State.AttentionalCapacity = AttentionalCapacity;
    State.ShiftCount = ShiftHistory.Num();

    // Compute average fixation duration
    float TotalDuration = 0.0f;
    int32 FocusedCount = 0;
    for (const FAttentionTarget& Target : Targets)
    {
        if (Target.AttentionDuration > 0.0f)
        {
            TotalDuration += Target.AttentionDuration;
            FocusedCount++;
        }
    }
    State.AverageFixationDuration = FocusedCount > 0 ? TotalDuration / FocusedCount : 0.0f;

    return State;
}

FAttentionSpotlight UAttentionSystem::GetSpotlight() const
{
    return Spotlight;
}

float UAttentionSystem::GetFatigueLevel() const
{
    return Spotlight.FatigueLevel;
}

float UAttentionSystem::GetAlertnessLevel() const
{
    return Alertness;
}

float UAttentionSystem::GetAttentionalCapacity() const
{
    return AttentionalCapacity;
}

bool UAttentionSystem::IsFocused() const
{
    return !Spotlight.CurrentTargetID.IsEmpty();
}

TArray<FAttentionShift> UAttentionSystem::GetRecentShifts(int32 Count) const
{
    TArray<FAttentionShift> Recent;
    int32 StartIndex = FMath::Max(0, ShiftHistory.Num() - Count);
    for (int32 i = StartIndex; i < ShiftHistory.Num(); ++i)
    {
        Recent.Add(ShiftHistory[i]);
    }
    return Recent;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UAttentionSystem::UpdateSalienceScores(float DeltaTime)
{
    for (FAttentionTarget& Target : Targets)
    {
        // Decay bottom-up salience
        Target.BottomUpSalience = FMath::Max(0.0f, Target.BottomUpSalience - AttentionDecayRate * DeltaTime);

        // Decay novelty salience
        Target.NoveltySalience = FMath::Max(0.0f, Target.NoveltySalience - AttentionDecayRate * 2.0f * DeltaTime);

        // Recompute combined salience
        Target.SalienceScore = ComputeCombinedSalience(Target);
    }

    // Decay salience map entries
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    for (int32 i = SalienceMap.Num() - 1; i >= 0; --i)
    {
        float Age = CurrentTime - SalienceMap[i].Timestamp;
        SalienceMap[i].Salience -= AttentionDecayRate * DeltaTime;

        if (SalienceMap[i].Salience <= 0.0f || Age > 60.0f)
        {
            SalienceMap.RemoveAt(i);
        }
    }
}

void UAttentionSystem::UpdateSpotlight(float DeltaTime)
{
    // Update focus duration
    if (!Spotlight.CurrentTargetID.IsEmpty())
    {
        Spotlight.FocusDuration += DeltaTime;

        int32 Index = FindTargetIndex(Spotlight.CurrentTargetID);
        if (Index >= 0)
        {
            Targets[Index].AttentionDuration += DeltaTime;
        }
    }
}

void UAttentionSystem::UpdateFatigue(float DeltaTime)
{
    // Increase fatigue when focused
    if (Spotlight.Mode == EAttentionMode::Focused || Spotlight.Mode == EAttentionMode::Sustained)
    {
        Spotlight.FatigueLevel = FMath::Min(1.0f, Spotlight.FatigueLevel + FatigueRate * DeltaTime);
    }
    else
    {
        // Recover when diffuse
        Spotlight.FatigueLevel = FMath::Max(0.0f, Spotlight.FatigueLevel - RecoveryRate * DeltaTime);
    }

    // Fatigue affects capacity
    AttentionalCapacity = 1.0f - (Spotlight.FatigueLevel * 0.5f);

    // Notify if highly fatigued
    if (Spotlight.FatigueLevel > 0.8f)
    {
        OnAttentionFatigued.Broadcast(Spotlight.FatigueLevel);
    }
}

void UAttentionSystem::CheckForInvoluntaryShifts()
{
    // Only check for involuntary shifts if not in sustained mode
    if (Spotlight.Mode == EAttentionMode::Sustained)
    {
        return;
    }

    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime - LastShiftTime < MinShiftInterval)
    {
        return;
    }

    // Find most salient target
    FAttentionTarget MostSalient = GetMostSalientTarget();

    // Check if it's significantly more salient than current focus
    if (!MostSalient.TargetID.IsEmpty() && MostSalient.TargetID != Spotlight.CurrentTargetID)
    {
        FAttentionTarget CurrentFocus = GetCurrentFocus();
        float SalienceDiff = MostSalient.SalienceScore - CurrentFocus.SalienceScore;

        // Threshold for involuntary capture depends on fatigue
        float CaptureThreshold = 0.3f * (1.0f - Spotlight.FatigueLevel * 0.5f);

        if (SalienceDiff > CaptureThreshold)
        {
            // Determine trigger source
            ESalienceSource TriggerSource = ESalienceSource::BottomUp;
            if (MostSalient.EmotionalSalience > MostSalient.BottomUpSalience)
            {
                TriggerSource = ESalienceSource::Emotional;
            }
            else if (MostSalient.NoveltySalience > MostSalient.BottomUpSalience)
            {
                TriggerSource = ESalienceSource::Novelty;
            }

            PerformAttentionShift(MostSalient.TargetID, TriggerSource, false);
        }
    }
}

float UAttentionSystem::ComputeBottomUpSalience(const FAttentionTarget& Target) const
{
    return Target.BottomUpSalience;
}

float UAttentionSystem::ComputeTopDownSalience(const FAttentionTarget& Target) const
{
    float TopDown = Target.TopDownSalience;

    // Add goal-driven salience from matching tags
    for (const FString& Tag : Target.SemanticTags)
    {
        if (GoalSalienceMap.Contains(Tag))
        {
            TopDown = FMath::Max(TopDown, GoalSalienceMap[Tag]);
        }
    }

    return TopDown;
}

float UAttentionSystem::ComputeEmotionalSalience(const FAttentionTarget& Target) const
{
    return Target.EmotionalSalience;
}

float UAttentionSystem::ComputeNoveltySalience(const FAttentionTarget& Target) const
{
    return Target.NoveltySalience;
}

void UAttentionSystem::PerformAttentionShift(const FString& ToTargetID, ESalienceSource TriggerSource, bool bVoluntary)
{
    FAttentionShift Shift;
    Shift.FromTargetID = Spotlight.CurrentTargetID;
    Shift.ToTargetID = ToTargetID;
    Shift.ShiftTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Shift.TriggerSource = TriggerSource;
    Shift.bWasVoluntary = bVoluntary;

    // Update focus flags
    for (FAttentionTarget& Target : Targets)
    {
        Target.bIsCurrentFocus = (Target.TargetID == ToTargetID);
        if (Target.bIsCurrentFocus)
        {
            Target.LastAttendedTime = Shift.ShiftTime;
            Spotlight.FocusLocation = Target.WorldLocation;
        }
    }

    Spotlight.CurrentTargetID = ToTargetID;
    Spotlight.FocusDuration = 0.0f;
    LastShiftTime = Shift.ShiftTime;

    // Set focused mode for voluntary shifts
    if (bVoluntary)
    {
        SetAttentionMode(EAttentionMode::Focused);
    }

    RecordShift(Shift);
    OnAttentionShifted.Broadcast(Shift);
}

void UAttentionSystem::RecordShift(const FAttentionShift& Shift)
{
    ShiftHistory.Add(Shift);

    // Limit history
    while (ShiftHistory.Num() > 100)
    {
        ShiftHistory.RemoveAt(0);
    }
}

FString UAttentionSystem::GenerateTargetID()
{
    return FString::Printf(TEXT("ATT_%d_%d"), ++TargetIDCounter, FMath::RandRange(1000, 9999));
}

int32 UAttentionSystem::FindTargetIndex(const FString& TargetID) const
{
    for (int32 i = 0; i < Targets.Num(); ++i)
    {
        if (Targets[i].TargetID == TargetID)
        {
            return i;
        }
    }
    return -1;
}

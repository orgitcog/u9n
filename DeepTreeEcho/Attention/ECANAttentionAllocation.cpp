// ECANAttentionAllocation.cpp
// Implementation of Economic Attention Networks for importance-based resource allocation
// Feature F1.4.3 - Hypergraph Memory System / Attention Allocation

#include "ECANAttentionAllocation.h"
#include "AttentionSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../../UnrealEcho/AtomSpace/AvatarAtomSpaceClient.h"
#include "Math/UnrealMathUtility.h"

UECANAttentionAllocation::UECANAttentionAllocation()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UECANAttentionAllocation::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeECAN();
}

void UECANAttentionAllocation::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoDynamics)
    {
        UpdateAttentionDynamics(DeltaTime);
    }

    // Reset per-cycle counters at start of each tick
    SpreadingEventsThisCycle = 0;
    ForgettingEventsThisCycle = 0;
}

void UECANAttentionAllocation::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        AttentionSystem = Owner->FindComponentByClass<UAttentionSystem>();
        AtomSpaceClient = Owner->FindComponentByClass<UAvatarAtomSpaceClient>();
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
    }
}

void UECANAttentionAllocation::InitializeECAN()
{
    Atoms.Empty();
    HebbianLinks.Empty();
    SpreadHistory.Empty();
    RecentActivations.Empty();
    AtomIDCounter = 0;
    
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    LastDecayTime = CurrentTime;
    LastRentTime = CurrentTime;
    LastForgetTime = CurrentTime;
}

// ========================================
// ATOM MANAGEMENT
// ========================================

FString UECANAttentionAllocation::CreateAtom(const FString& Name, const FString& AtomType,
                                              float InitialSTI, float InitialLTI,
                                              const TArray<FString>& Tags)
{
    // Check capacity
    if (Atoms.Num() >= Config.MaxTotalAtoms)
    {
        // Process forgetting to make room
        ProcessForgetting();
        
        if (Atoms.Num() >= Config.MaxTotalAtoms)
        {
            return TEXT("");
        }
    }

    FECANAtom Atom;
    Atom.AtomID = GenerateAtomID();
    Atom.Name = Name;
    Atom.AtomType = AtomType;
    Atom.AttentionValue.STI = FMath::Clamp(InitialSTI, Config.MinSTI, Config.MaxSTI);
    Atom.AttentionValue.LTI = FMath::Max(0.0f, InitialLTI);
    Atom.AttentionValue.FocusLevel = ComputeFocusLevel(Atom.AttentionValue.STI);
    Atom.Tags = Tags;
    
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Atom.CreatedAt = CurrentTime;
    Atom.LastAccessedAt = CurrentTime;
    Atom.AttentionValue.LastUpdateTime = CurrentTime;

    Atoms.Add(Atom.AtomID, Atom);

    return Atom.AtomID;
}

FECANAtom UECANAttentionAllocation::GetAtom(const FString& AtomID) const
{
    if (const FECANAtom* Atom = Atoms.Find(AtomID))
    {
        return *Atom;
    }
    return FECANAtom();
}

bool UECANAttentionAllocation::AtomExists(const FString& AtomID) const
{
    return Atoms.Contains(AtomID);
}

bool UECANAttentionAllocation::RemoveAtom(const FString& AtomID)
{
    if (!Atoms.Contains(AtomID))
    {
        return false;
    }

    // Remove Hebbian links involving this atom
    TArray<FString> LinksToRemove;
    for (const auto& LinkPair : HebbianLinks)
    {
        if (LinkPair.Value.SourceAtomID == AtomID || LinkPair.Value.TargetAtomID == AtomID)
        {
            LinksToRemove.Add(LinkPair.Key);
        }
    }
    for (const FString& Key : LinksToRemove)
    {
        HebbianLinks.Remove(Key);
    }

    // Remove atom links from other atoms
    for (auto& AtomPair : Atoms)
    {
        AtomPair.Value.OutgoingAtomIDs.Remove(AtomID);
        AtomPair.Value.IncomingAtomIDs.Remove(AtomID);
    }

    Atoms.Remove(AtomID);
    return true;
}

TArray<FECANAtom> UECANAttentionAllocation::GetAllAtoms() const
{
    TArray<FECANAtom> Result;
    for (const auto& AtomPair : Atoms)
    {
        Result.Add(AtomPair.Value);
    }
    return Result;
}

TArray<FECANAtom> UECANAttentionAllocation::GetAtomsByFocusLevel(EAttentionFocusLevel Level) const
{
    TArray<FECANAtom> Result;
    for (const auto& AtomPair : Atoms)
    {
        if (AtomPair.Value.AttentionValue.FocusLevel == Level)
        {
            Result.Add(AtomPair.Value);
        }
    }
    return Result;
}

TArray<FECANAtom> UECANAttentionAllocation::GetAttentionalFocus() const
{
    return GetAtomsByFocusLevel(EAttentionFocusLevel::AttentionalFocus);
}

TArray<FECANAtom> UECANAttentionAllocation::GetActiveSet() const
{
    return GetAtomsByFocusLevel(EAttentionFocusLevel::ActiveSet);
}

void UECANAttentionAllocation::AddAtomLink(const FString& SourceID, const FString& TargetID)
{
    if (!Atoms.Contains(SourceID) || !Atoms.Contains(TargetID))
    {
        return;
    }

    FECANAtom* Source = Atoms.Find(SourceID);
    FECANAtom* Target = Atoms.Find(TargetID);

    if (Source && Target)
    {
        if (!Source->OutgoingAtomIDs.Contains(TargetID))
        {
            Source->OutgoingAtomIDs.Add(TargetID);
        }
        if (!Target->IncomingAtomIDs.Contains(SourceID))
        {
            Target->IncomingAtomIDs.Add(SourceID);
        }
    }
}

// ========================================
// ATTENTION VALUE MANAGEMENT
// ========================================

FECANAttentionValue UECANAttentionAllocation::GetAttentionValue(const FString& AtomID) const
{
    if (const FECANAtom* Atom = Atoms.Find(AtomID))
    {
        return Atom->AttentionValue;
    }
    return FECANAttentionValue();
}

void UECANAttentionAllocation::SetAttentionValue(const FString& AtomID, const FECANAttentionValue& Value)
{
    FECANAtom* Atom = Atoms.Find(AtomID);
    if (!Atom)
    {
        return;
    }

    EAttentionFocusLevel OldLevel = Atom->AttentionValue.FocusLevel;
    
    Atom->AttentionValue.STI = FMath::Clamp(Value.STI, Config.MinSTI, Config.MaxSTI);
    Atom->AttentionValue.LTI = FMath::Max(0.0f, Value.LTI);
    Atom->AttentionValue.VLTI = FMath::Max(0.0f, Value.VLTI);
    Atom->AttentionValue.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Atom->AttentionValue.FocusLevel = ComputeFocusLevel(Atom->AttentionValue.STI);

    if (OldLevel != Atom->AttentionValue.FocusLevel)
    {
        BroadcastFocusChange(AtomID, OldLevel, Atom->AttentionValue.FocusLevel);
    }
}

void UECANAttentionAllocation::StimulateSTI(const FString& AtomID, float Amount)
{
    FECANAtom* Atom = Atoms.Find(AtomID);
    if (!Atom)
    {
        return;
    }

    EAttentionFocusLevel OldLevel = Atom->AttentionValue.FocusLevel;
    
    Atom->AttentionValue.STI = FMath::Clamp(
        Atom->AttentionValue.STI + Amount, 
        Config.MinSTI, 
        Config.MaxSTI
    );
    
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Atom->AttentionValue.LastUpdateTime = CurrentTime;
    Atom->LastAccessedAt = CurrentTime;
    Atom->AccessCount++;
    
    Atom->AttentionValue.FocusLevel = ComputeFocusLevel(Atom->AttentionValue.STI);

    // Record activation for Hebbian learning
    RecentActivations.Add(AtomID);
    while (RecentActivations.Num() > 100)
    {
        RecentActivations.RemoveAt(0);
    }

    OnSTIStimulus.Broadcast(AtomID, Amount);

    if (OldLevel != Atom->AttentionValue.FocusLevel)
    {
        BroadcastFocusChange(AtomID, OldLevel, Atom->AttentionValue.FocusLevel);
    }
}

void UECANAttentionAllocation::StimulateLTI(const FString& AtomID, float Amount)
{
    FECANAtom* Atom = Atoms.Find(AtomID);
    if (!Atom)
    {
        return;
    }

    Atom->AttentionValue.LTI = FMath::Max(0.0f, Atom->AttentionValue.LTI + Amount);
    Atom->AttentionValue.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UECANAttentionAllocation::SetVLTI(const FString& AtomID, float Value)
{
    FECANAtom* Atom = Atoms.Find(AtomID);
    if (!Atom)
    {
        return;
    }

    Atom->AttentionValue.VLTI = FMath::Max(0.0f, Value);
}

void UECANAttentionAllocation::TransferSTI(const FString& SourceID, const FString& TargetID, float Amount)
{
    FECANAtom* Source = Atoms.Find(SourceID);
    FECANAtom* Target = Atoms.Find(TargetID);
    
    if (!Source || !Target)
    {
        return;
    }

    // Ensure we don't transfer more than source has (can go negative but limit)
    float TransferAmount = FMath::Min(Amount, Source->AttentionValue.STI - Config.MinSTI);
    TransferAmount = FMath::Max(0.0f, TransferAmount);

    Source->AttentionValue.STI -= TransferAmount;
    Target->AttentionValue.STI = FMath::Min(Config.MaxSTI, Target->AttentionValue.STI + TransferAmount);

    Source->AttentionValue.FocusLevel = ComputeFocusLevel(Source->AttentionValue.STI);
    Target->AttentionValue.FocusLevel = ComputeFocusLevel(Target->AttentionValue.STI);
}

EAttentionFocusLevel UECANAttentionAllocation::GetFocusLevel(const FString& AtomID) const
{
    if (const FECANAtom* Atom = Atoms.Find(AtomID))
    {
        return Atom->AttentionValue.FocusLevel;
    }
    return EAttentionFocusLevel::InactiveSet;
}

// ========================================
// SPREADING ACTIVATION
// ========================================

void UECANAttentionAllocation::SpreadAttention(const FString& SourceAtomID)
{
    if (!bEnableSpreadingActivation)
    {
        return;
    }

    FECANAtom* Source = Atoms.Find(SourceAtomID);
    if (!Source || Source->AttentionValue.STI <= 0)
    {
        return;
    }

    // Calculate amount to spread
    float SpreadAmount = Source->AttentionValue.STI * Config.SpreadingFraction;
    
    // Collect spread targets (linked atoms + Hebbian neighbors)
    TArray<TPair<FString, float>> Targets;
    
    // Direct links
    for (const FString& TargetID : Source->OutgoingAtomIDs)
    {
        if (Atoms.Contains(TargetID))
        {
            Targets.Add(TPair<FString, float>(TargetID, 1.0f));
        }
    }

    // Hebbian links
    for (const auto& LinkPair : HebbianLinks)
    {
        const FHebbianLink& Link = LinkPair.Value;
        if (Link.SourceAtomID == SourceAtomID && Link.Strength >= Config.MinSpreadLinkStrength)
        {
            bool bFound = false;
            for (auto& Target : Targets)
            {
                if (Target.Key == Link.TargetAtomID)
                {
                    Target.Value += Link.Strength;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                Targets.Add(TPair<FString, float>(Link.TargetAtomID, Link.Strength));
            }
        }
        else if (Link.TargetAtomID == SourceAtomID && Link.Strength >= Config.MinSpreadLinkStrength)
        {
            bool bFound = false;
            for (auto& Target : Targets)
            {
                if (Target.Key == Link.SourceAtomID)
                {
                    Target.Value += Link.Strength;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                Targets.Add(TPair<FString, float>(Link.SourceAtomID, Link.Strength));
            }
        }
    }

    // Limit targets
    if (Targets.Num() > Config.MaxSpreadTargets)
    {
        // Sort by strength and take top N
        Targets.Sort([](const TPair<FString, float>& A, const TPair<FString, float>& B) {
            return A.Value > B.Value;
        });
        Targets.SetNum(Config.MaxSpreadTargets);
    }

    // Normalize weights
    float TotalWeight = 0.0f;
    for (const auto& Target : Targets)
    {
        TotalWeight += Target.Value;
    }

    if (TotalWeight <= 0.0f || Targets.Num() == 0)
    {
        return;
    }

    // Spread to each target
    for (const auto& Target : Targets)
    {
        float TargetAmount = SpreadAmount * (Target.Value / TotalWeight);
        TransferSTI(SourceAtomID, Target.Key, TargetAmount);
        
        RecordSpreadEvent(SourceAtomID, Target.Key, TargetAmount, EECANSpreadType::ImportanceLink);
        SpreadingEventsThisCycle++;
    }
}

void UECANAttentionAllocation::SpreadFromFocus()
{
    TArray<FECANAtom> FocusAtoms = GetAttentionalFocus();
    for (const FECANAtom& Atom : FocusAtoms)
    {
        SpreadAttention(Atom.AtomID);
    }
}

TArray<FAttentionSpreadEvent> UECANAttentionAllocation::GetRecentSpreadEvents(int32 Count) const
{
    TArray<FAttentionSpreadEvent> Result;
    int32 StartIndex = FMath::Max(0, SpreadHistory.Num() - Count);
    for (int32 i = StartIndex; i < SpreadHistory.Num(); ++i)
    {
        Result.Add(SpreadHistory[i]);
    }
    return Result;
}

// ========================================
// HEBBIAN LEARNING
// ========================================

void UECANAttentionAllocation::RecordCoActivation(const FString& AtomID1, const FString& AtomID2)
{
    if (!bEnableHebbianLearning || AtomID1 == AtomID2)
    {
        return;
    }

    if (!Atoms.Contains(AtomID1) || !Atoms.Contains(AtomID2))
    {
        return;
    }

    FString LinkKey = GenerateHebbianLinkKey(AtomID1, AtomID2);
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    FHebbianLink* Link = HebbianLinks.Find(LinkKey);
    if (Link)
    {
        // Strengthen existing link (Hebbian: "neurons that fire together wire together")
        Link->CoActivationCount++;
        Link->Strength = FMath::Min(1.0f, Link->Strength + Config.HebbianLearningRate);
        Link->LastUpdateTime = CurrentTime;
    }
    else
    {
        // Create new link
        FHebbianLink NewLink;
        NewLink.SourceAtomID = AtomID1;
        NewLink.TargetAtomID = AtomID2;
        NewLink.Strength = Config.HebbianLearningRate;
        NewLink.CoActivationCount = 1;
        NewLink.LastUpdateTime = CurrentTime;

        HebbianLinks.Add(LinkKey, NewLink);
        OnHebbianLinkCreated.Broadcast(NewLink);
    }
}

float UECANAttentionAllocation::GetHebbianLinkStrength(const FString& AtomID1, const FString& AtomID2) const
{
    FString LinkKey = GenerateHebbianLinkKey(AtomID1, AtomID2);
    if (const FHebbianLink* Link = HebbianLinks.Find(LinkKey))
    {
        return Link->Strength;
    }
    return 0.0f;
}

TArray<FHebbianLink> UECANAttentionAllocation::GetHebbianLinks(const FString& AtomID) const
{
    TArray<FHebbianLink> Result;
    for (const auto& LinkPair : HebbianLinks)
    {
        if (LinkPair.Value.SourceAtomID == AtomID || LinkPair.Value.TargetAtomID == AtomID)
        {
            Result.Add(LinkPair.Value);
        }
    }
    return Result;
}

TArray<FHebbianLink> UECANAttentionAllocation::GetAllHebbianLinks() const
{
    TArray<FHebbianLink> Result;
    for (const auto& LinkPair : HebbianLinks)
    {
        Result.Add(LinkPair.Value);
    }
    return Result;
}

// ========================================
// ECONOMIC DYNAMICS
// ========================================

void UECANAttentionAllocation::CollectRent()
{
    if (!bEnableEconomicDynamics)
    {
        return;
    }

    for (auto& AtomPair : Atoms)
    {
        FECANAtom& Atom = AtomPair.Value;
        
        // Rent is proportional to STI (higher attention = higher rent)
        if (Atom.AttentionValue.STI > 0)
        {
            float Rent = Atom.AttentionValue.STI * Config.STIRentRate;
            Atom.AttentionValue.STI = FMath::Max(Config.MinSTI, Atom.AttentionValue.STI - Rent);
        }
    }

    LastRentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UECANAttentionAllocation::AwardWages(const FString& AtomID, float UsageScore)
{
    if (!bEnableEconomicDynamics)
    {
        return;
    }

    FECANAtom* Atom = Atoms.Find(AtomID);
    if (!Atom)
    {
        return;
    }

    // LTI wages based on usage
    float Wage = UsageScore * Config.LTIWageRate;
    Atom->AttentionValue.LTI = FMath::Max(0.0f, Atom->AttentionValue.LTI + Wage);
}

float UECANAttentionAllocation::GetTotalSystemSTI() const
{
    float Total = 0.0f;
    for (const auto& AtomPair : Atoms)
    {
        Total += AtomPair.Value.AttentionValue.STI;
    }
    return Total;
}

void UECANAttentionAllocation::NormalizeSTI()
{
    float CurrentTotal = GetTotalSystemSTI();
    if (FMath::IsNearlyZero(CurrentTotal))
    {
        return;
    }

    float ScaleFactor = Config.TotalSTIFunds / CurrentTotal;
    
    for (auto& AtomPair : Atoms)
    {
        FECANAtom& Atom = AtomPair.Value;
        Atom.AttentionValue.STI = FMath::Clamp(
            Atom.AttentionValue.STI * ScaleFactor,
            Config.MinSTI,
            Config.MaxSTI
        );
        Atom.AttentionValue.FocusLevel = ComputeFocusLevel(Atom.AttentionValue.STI);
    }
}

// ========================================
// FORGETTING
// ========================================

TArray<FString> UECANAttentionAllocation::ProcessForgetting()
{
    TArray<FString> ForgottenAtoms;
    
    if (!bEnableForgetting)
    {
        return ForgottenAtoms;
    }

    TArray<FString> ToRemove;
    for (const auto& AtomPair : Atoms)
    {
        if (IsForgettingCandidate(AtomPair.Key))
        {
            ToRemove.Add(AtomPair.Key);
        }
    }

    for (const FString& AtomID : ToRemove)
    {
        RemoveAtom(AtomID);
        ForgottenAtoms.Add(AtomID);
        OnAtomForgotten.Broadcast(AtomID);
        ForgettingEventsThisCycle++;
    }

    LastForgetTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    return ForgottenAtoms;
}

bool UECANAttentionAllocation::IsForgettingCandidate(const FString& AtomID) const
{
    const FECANAtom* Atom = Atoms.Find(AtomID);
    if (!Atom)
    {
        return false;
    }

    // Protected by VLTI
    if (Atom->AttentionValue.VLTI > 0.0f)
    {
        return false;
    }

    // Below forget threshold
    if (Atom->AttentionValue.STI < Config.ForgetThreshold)
    {
        return true;
    }

    return false;
}

void UECANAttentionAllocation::ProtectFromForgetting(const FString& AtomID)
{
    SetVLTI(AtomID, 1.0f);
}

// ========================================
// STATISTICS & QUERIES
// ========================================

FECANStatistics UECANAttentionAllocation::GetStatistics() const
{
    FECANStatistics Stats;
    
    Stats.TotalAtomCount = Atoms.Num();
    Stats.HebbianLinkCount = HebbianLinks.Num();
    Stats.SpreadingEventsThisCycle = SpreadingEventsThisCycle;
    Stats.ForgettingEventsThisCycle = ForgettingEventsThisCycle;

    float TotalSTI = 0.0f;
    float TotalLTI = 0.0f;

    for (const auto& AtomPair : Atoms)
    {
        const FECANAtom& Atom = AtomPair.Value;
        TotalSTI += Atom.AttentionValue.STI;
        TotalLTI += Atom.AttentionValue.LTI;

        switch (Atom.AttentionValue.FocusLevel)
        {
            case EAttentionFocusLevel::AttentionalFocus:
                Stats.AttentionalFocusCount++;
                break;
            case EAttentionFocusLevel::ActiveSet:
                Stats.ActiveSetCount++;
                break;
            case EAttentionFocusLevel::PassiveSet:
                Stats.PassiveSetCount++;
                break;
            case EAttentionFocusLevel::InactiveSet:
                Stats.InactiveSetCount++;
                break;
            case EAttentionFocusLevel::ForgetSet:
                Stats.ForgetSetCount++;
                break;
        }
    }

    Stats.TotalSTI = TotalSTI;
    Stats.TotalLTI = TotalLTI;
    Stats.AverageSTI = Stats.TotalAtomCount > 0 ? TotalSTI / Stats.TotalAtomCount : 0.0f;
    Stats.AverageLTI = Stats.TotalAtomCount > 0 ? TotalLTI / Stats.TotalAtomCount : 0.0f;

    return Stats;
}

TArray<FECANAtom> UECANAttentionAllocation::GetTopAtomsBySTI(int32 Count) const
{
    TArray<FECANAtom> AllAtoms = GetAllAtoms();
    AllAtoms.Sort([](const FECANAtom& A, const FECANAtom& B) {
        return A.AttentionValue.STI > B.AttentionValue.STI;
    });

    if (AllAtoms.Num() > Count)
    {
        AllAtoms.SetNum(Count);
    }

    return AllAtoms;
}

TArray<FECANAtom> UECANAttentionAllocation::GetTopAtomsByLTI(int32 Count) const
{
    TArray<FECANAtom> AllAtoms = GetAllAtoms();
    AllAtoms.Sort([](const FECANAtom& A, const FECANAtom& B) {
        return A.AttentionValue.LTI > B.AttentionValue.LTI;
    });

    if (AllAtoms.Num() > Count)
    {
        AllAtoms.SetNum(Count);
    }

    return AllAtoms;
}

TArray<FECANAtom> UECANAttentionAllocation::FindAtomsByTag(const FString& Tag) const
{
    TArray<FECANAtom> Result;
    for (const auto& AtomPair : Atoms)
    {
        if (AtomPair.Value.Tags.Contains(Tag))
        {
            Result.Add(AtomPair.Value);
        }
    }
    return Result;
}

TArray<FECANAtom> UECANAttentionAllocation::GetCoActiveAtoms(const FString& AtomID, float MinLinkStrength) const
{
    TArray<FECANAtom> Result;
    
    for (const auto& LinkPair : HebbianLinks)
    {
        const FHebbianLink& Link = LinkPair.Value;
        if (Link.Strength < MinLinkStrength)
        {
            continue;
        }

        FString CoActiveID;
        if (Link.SourceAtomID == AtomID)
        {
            CoActiveID = Link.TargetAtomID;
        }
        else if (Link.TargetAtomID == AtomID)
        {
            CoActiveID = Link.SourceAtomID;
        }
        else
        {
            continue;
        }

        if (const FECANAtom* Atom = Atoms.Find(CoActiveID))
        {
            Result.Add(*Atom);
        }
    }

    return Result;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UECANAttentionAllocation::UpdateAttentionDynamics(float DeltaTime)
{
    // Update STI decay
    UpdateSTIDecay(DeltaTime);

    // Update LTI decay
    UpdateLTIDecay(DeltaTime);

    // Update Hebbian link decay
    UpdateHebbianLinks(DeltaTime);

    // Update focus levels
    UpdateFocusLevels();

    // Process Hebbian co-activations from recent stimulations
    if (bEnableHebbianLearning && RecentActivations.Num() >= 2)
    {
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        
        // Find pairs within co-activation window
        for (int32 i = 0; i < RecentActivations.Num(); ++i)
        {
            for (int32 j = i + 1; j < RecentActivations.Num(); ++j)
            {
                const FString& ID1 = RecentActivations[i];
                const FString& ID2 = RecentActivations[j];
                
                if (ID1 != ID2)
                {
                    const FECANAtom* Atom1 = Atoms.Find(ID1);
                    const FECANAtom* Atom2 = Atoms.Find(ID2);
                    
                    if (Atom1 && Atom2)
                    {
                        float TimeDiff = FMath::Abs(Atom1->AttentionValue.LastUpdateTime - 
                                                    Atom2->AttentionValue.LastUpdateTime);
                        if (TimeDiff < Config.CoActivationWindow)
                        {
                            RecordCoActivation(ID1, ID2);
                        }
                    }
                }
            }
        }

        // Clear old activations
        RecentActivations.Empty();
    }
}

void UECANAttentionAllocation::UpdateSTIDecay(float DeltaTime)
{
    for (auto& AtomPair : Atoms)
    {
        FECANAtom& Atom = AtomPair.Value;
        
        // STI decays over time
        float Decay = Config.STIDecayRate * DeltaTime;
        Atom.AttentionValue.STI = FMath::Max(Config.MinSTI, Atom.AttentionValue.STI - Decay);
    }
}

void UECANAttentionAllocation::UpdateLTIDecay(float DeltaTime)
{
    for (auto& AtomPair : Atoms)
    {
        FECANAtom& Atom = AtomPair.Value;
        
        // LTI decays slowly (but not below 0)
        float Decay = Config.LTIDecayRate * DeltaTime;
        Atom.AttentionValue.LTI = FMath::Max(0.0f, Atom.AttentionValue.LTI - Decay);
        
        // VLTI decays very slowly
        Decay = Config.VLTIDecayRate * DeltaTime;
        Atom.AttentionValue.VLTI = FMath::Max(0.0f, Atom.AttentionValue.VLTI - Decay);
    }
}

void UECANAttentionAllocation::UpdateHebbianLinks(float DeltaTime)
{
    TArray<FString> LinksToRemove;
    
    for (auto& LinkPair : HebbianLinks)
    {
        FHebbianLink& Link = LinkPair.Value;
        
        // Decay link strength
        Link.Strength = FMath::Max(0.0f, Link.Strength - Config.HebbianDecayRate * DeltaTime);
        
        // Remove very weak links
        if (Link.Strength < 0.01f)
        {
            LinksToRemove.Add(LinkPair.Key);
        }
    }

    for (const FString& Key : LinksToRemove)
    {
        HebbianLinks.Remove(Key);
    }
}

void UECANAttentionAllocation::UpdateFocusLevels()
{
    TArray<FString> FocusChangedAtoms;

    for (auto& AtomPair : Atoms)
    {
        FECANAtom& Atom = AtomPair.Value;
        EAttentionFocusLevel NewLevel = ComputeFocusLevel(Atom.AttentionValue.STI);
        
        if (NewLevel != Atom.AttentionValue.FocusLevel)
        {
            EAttentionFocusLevel OldLevel = Atom.AttentionValue.FocusLevel;
            Atom.AttentionValue.FocusLevel = NewLevel;
            BroadcastFocusChange(AtomPair.Key, OldLevel, NewLevel);
        }
    }
}

EAttentionFocusLevel UECANAttentionAllocation::ComputeFocusLevel(float STI) const
{
    if (STI >= Config.AttentionalFocusThreshold)
    {
        return EAttentionFocusLevel::AttentionalFocus;
    }
    else if (STI >= Config.ActiveSetThreshold)
    {
        return EAttentionFocusLevel::ActiveSet;
    }
    else if (STI >= Config.PassiveSetThreshold)
    {
        return EAttentionFocusLevel::PassiveSet;
    }
    else if (STI >= Config.ForgetThreshold)
    {
        return EAttentionFocusLevel::InactiveSet;
    }
    else
    {
        return EAttentionFocusLevel::ForgetSet;
    }
}

FString UECANAttentionAllocation::GenerateAtomID()
{
    return FString::Printf(TEXT("ECAN_%d_%d"), ++AtomIDCounter, FMath::RandRange(1000, 9999));
}

FString UECANAttentionAllocation::GenerateHebbianLinkKey(const FString& AtomID1, const FString& AtomID2) const
{
    // Ensure consistent key regardless of argument order
    if (AtomID1 < AtomID2)
    {
        return FString::Printf(TEXT("%s<->%s"), *AtomID1, *AtomID2);
    }
    else
    {
        return FString::Printf(TEXT("%s<->%s"), *AtomID2, *AtomID1);
    }
}

void UECANAttentionAllocation::RecordSpreadEvent(const FString& SourceID, const FString& TargetID,
                                                  float Amount, EECANSpreadType Type)
{
    FAttentionSpreadEvent Event;
    Event.SourceAtomID = SourceID;
    Event.TargetAtomID = TargetID;
    Event.SpreadAmount = Amount;
    Event.SpreadType = Type;
    Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    SpreadHistory.Add(Event);

    // Limit history size
    while (SpreadHistory.Num() > 100)
    {
        SpreadHistory.RemoveAt(0);
    }
}

void UECANAttentionAllocation::BroadcastFocusChange(const FString& AtomID, 
                                                     EAttentionFocusLevel OldLevel,
                                                     EAttentionFocusLevel NewLevel)
{
    OnAtomFocusChanged.Broadcast(AtomID, NewLevel);

    // If entering or leaving Attentional Focus, broadcast focus set change
    if (OldLevel == EAttentionFocusLevel::AttentionalFocus || 
        NewLevel == EAttentionFocusLevel::AttentionalFocus)
    {
        TArray<FString> FocusAtomIDs;
        for (const auto& AtomPair : Atoms)
        {
            if (AtomPair.Value.AttentionValue.FocusLevel == EAttentionFocusLevel::AttentionalFocus)
            {
                FocusAtomIDs.Add(AtomPair.Key);
            }
        }
        OnAttentionalFocusChanged.Broadcast(FocusAtomIDs);
    }
}

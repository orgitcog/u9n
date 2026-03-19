/**
 * @file MembraneHierarchyManager.cpp
 * @brief Feature F1.3.1: Membrane Hierarchy Manager Implementation
 * 
 * @author Deep Tree Echo Team
 * @date March 2026
 * @version 1.0.0-alpha
 */

#include "MembraneHierarchyManager.h"
#include "GameFramework/Actor.h"

UMembraneHierarchyManager::UMembraneHierarchyManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMembraneHierarchyManager::BeginPlay()
{
    Super::BeginPlay();
    CurrentTime = 0.0f;
}

void UMembraneHierarchyManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableProcessing)
    {
        return;
    }

    CurrentTime += DeltaTime;

    // Process pending messages
    ProcessMessageQueue();

    // Update membrane timestamps
    for (auto& Pair : Membranes)
    {
        if (Pair.Value.State == EMembraneState::Active)
        {
            Pair.Value.LastUpdateTime = CurrentTime;
        }
    }
}

// ========================================
// MEMBRANE LIFECYCLE
// ========================================

FString UMembraneHierarchyManager::CreateRootMembrane(const FString& Label, const FMembraneConfig& Config)
{
    if (!ValidateConfig(Config))
    {
        return FString();
    }

    FMembraneStateData NewMembrane;
    NewMembrane.MembraneID = GenerateMembraneID();
    NewMembrane.Label = Label;
    NewMembrane.State = EMembraneState::Active;
    NewMembrane.Type = EMembraneType::Root;
    NewMembrane.ParentID = FString();  // Root has no parent
    NewMembrane.NestingDepth = 0;
    NewMembrane.CreationTime = CurrentTime;
    NewMembrane.LastUpdateTime = CurrentTime;
    NewMembrane.EnergyLevel = 100.0f;

    Membranes.Add(NewMembrane.MembraneID, NewMembrane);

    // Broadcast creation event
    OnMembraneCreated.Broadcast(NewMembrane.MembraneID);

    return NewMembrane.MembraneID;
}

FString UMembraneHierarchyManager::CreateChildMembrane(const FString& ParentID, const FString& Label, const FMembraneConfig& Config)
{
    // Validate parent exists
    if (!MembraneExists(ParentID))
    {
        UE_LOG(LogTemp, Warning, TEXT("MembraneHierarchyManager: Parent membrane '%s' not found"), *ParentID);
        return FString();
    }

    // Check if parent can accept children
    if (!CanAddChild(ParentID))
    {
        UE_LOG(LogTemp, Warning, TEXT("MembraneHierarchyManager: Parent membrane '%s' cannot accept more children"), *ParentID);
        return FString();
    }

    FMembraneStateData& Parent = Membranes[ParentID];

    // Check nesting depth
    if (GlobalConfig.MaxNestingDepth > 0 && Parent.NestingDepth >= GlobalConfig.MaxNestingDepth)
    {
        UE_LOG(LogTemp, Warning, TEXT("MembraneHierarchyManager: Maximum nesting depth reached"));
        return FString();
    }

    if (!ValidateConfig(Config))
    {
        return FString();
    }

    FMembraneStateData NewMembrane;
    NewMembrane.MembraneID = GenerateMembraneID();
    NewMembrane.Label = Label;
    NewMembrane.State = EMembraneState::Active;
    NewMembrane.Type = Config.MembraneType;
    NewMembrane.ParentID = ParentID;
    NewMembrane.NestingDepth = Parent.NestingDepth + 1;
    NewMembrane.CreationTime = CurrentTime;
    NewMembrane.LastUpdateTime = CurrentTime;
    NewMembrane.EnergyLevel = 100.0f;

    // Add to membranes
    Membranes.Add(NewMembrane.MembraneID, NewMembrane);

    // Update parent to include child
    Parent.ChildIDs.Add(NewMembrane.MembraneID);

    // If parent was elementary, upgrade to composite
    if (Parent.Type == EMembraneType::Elementary)
    {
        Parent.Type = EMembraneType::Composite;
    }

    // Broadcast creation event
    OnMembraneCreated.Broadcast(NewMembrane.MembraneID);

    return NewMembrane.MembraneID;
}

FString UMembraneHierarchyManager::CreateMembraneForActor(AActor* Actor, const FString& ParentID)
{
    if (!Actor)
    {
        return FString();
    }

    FString Label = Actor->GetName();
    FMembraneConfig Config = GlobalConfig;

    FString NewMembraneID;
    if (ParentID.IsEmpty())
    {
        NewMembraneID = CreateRootMembrane(Label, Config);
    }
    else
    {
        NewMembraneID = CreateChildMembrane(ParentID, Label, Config);
    }

    if (!NewMembraneID.IsEmpty() && Membranes.Contains(NewMembraneID))
    {
        Membranes[NewMembraneID].AssociatedActor = Actor;
    }

    return NewMembraneID;
}

bool UMembraneHierarchyManager::DissolveMembrane(const FString& MembraneID)
{
    if (!MembraneExists(MembraneID))
    {
        return false;
    }

    FMembraneStateData& Membrane = Membranes[MembraneID];

    // Set state to dissolving
    EMembraneState OldState = Membrane.State;
    Membrane.State = EMembraneState::Dissolving;
    OnMembraneStateChanged.Broadcast(MembraneID, EMembraneState::Dissolving);

    // Handle children: make them children of parent
    if (!Membrane.ParentID.IsEmpty() && MembraneExists(Membrane.ParentID))
    {
        FMembraneStateData& Parent = Membranes[Membrane.ParentID];

        // Transfer children to parent
        for (const FString& ChildID : Membrane.ChildIDs)
        {
            if (MembraneExists(ChildID))
            {
                Membranes[ChildID].ParentID = Membrane.ParentID;
                Membranes[ChildID].NestingDepth = Membrane.NestingDepth;
                Parent.ChildIDs.Add(ChildID);
            }
        }

        // Transfer objects to parent
        for (const FMembraneObject& Obj : Membrane.Objects)
        {
            Parent.Objects.Add(Obj);
            OnObjectTransferred.Broadcast(MembraneID, Membrane.ParentID);
        }

        // Remove from parent's children
        Parent.ChildIDs.Remove(MembraneID);
        UpdateMembraneStats(Parent);
    }
    else
    {
        // Root membrane: children become roots
        for (const FString& ChildID : Membrane.ChildIDs)
        {
            if (MembraneExists(ChildID))
            {
                Membranes[ChildID].ParentID = FString();
                Membranes[ChildID].NestingDepth = 0;
                Membranes[ChildID].Type = EMembraneType::Root;
            }
        }
    }

    // Cleanup and remove
    CleanupDissolvedMembrane(MembraneID);
    Membranes.Remove(MembraneID);

    // Broadcast destruction event
    OnMembraneDestroyed.Broadcast(MembraneID);

    return true;
}

TArray<FString> UMembraneHierarchyManager::DivideMembrane(const FString& MembraneID, float SplitRatio)
{
    TArray<FString> Result;

    if (!MembraneExists(MembraneID))
    {
        return Result;
    }

    FMembraneStateData& Original = Membranes[MembraneID];

    // Set state to dividing
    Original.State = EMembraneState::Dividing;
    OnMembraneStateChanged.Broadcast(MembraneID, EMembraneState::Dividing);

    // Create two new membranes
    FMembraneConfig Config;
    Config.MembraneType = Original.Type;

    FString ParentID = Original.ParentID;
    FString Membrane1ID = CreateChildMembrane(ParentID.IsEmpty() ? MembraneID : ParentID, 
                                               Original.Label + TEXT("_A"), Config);
    FString Membrane2ID = CreateChildMembrane(ParentID.IsEmpty() ? MembraneID : ParentID, 
                                               Original.Label + TEXT("_B"), Config);

    if (Membrane1ID.IsEmpty() || Membrane2ID.IsEmpty())
    {
        // Revert state
        Original.State = EMembraneState::Active;
        return Result;
    }

    // Split objects
    int32 SplitIndex = FMath::RoundToInt(Original.Objects.Num() * FMath::Clamp(SplitRatio, 0.0f, 1.0f));

    for (int32 i = 0; i < Original.Objects.Num(); i++)
    {
        if (i < SplitIndex)
        {
            AddObject(Membrane1ID, Original.Objects[i]);
        }
        else
        {
            AddObject(Membrane2ID, Original.Objects[i]);
        }
    }

    // Split children (equally)
    for (int32 i = 0; i < Original.ChildIDs.Num(); i++)
    {
        const FString& ChildID = Original.ChildIDs[i];
        if (MembraneExists(ChildID))
        {
            FString NewParent = (i % 2 == 0) ? Membrane1ID : Membrane2ID;
            Membranes[ChildID].ParentID = NewParent;
            Membranes[NewParent].ChildIDs.Add(ChildID);
        }
    }

    // Split energy
    float Energy1 = Original.EnergyLevel * SplitRatio;
    float Energy2 = Original.EnergyLevel * (1.0f - SplitRatio);
    if (MembraneExists(Membrane1ID)) Membranes[Membrane1ID].EnergyLevel = Energy1;
    if (MembraneExists(Membrane2ID)) Membranes[Membrane2ID].EnergyLevel = Energy2;

    // Remove original
    Original.Objects.Empty();
    Original.ChildIDs.Empty();
    Membranes.Remove(MembraneID);
    OnMembraneDestroyed.Broadcast(MembraneID);

    Result.Add(Membrane1ID);
    Result.Add(Membrane2ID);

    UpdateHierarchyAfterChange();

    return Result;
}

// ========================================
// HIERARCHY MANAGEMENT
// ========================================

FMembraneStateData UMembraneHierarchyManager::GetMembraneState(const FString& MembraneID) const
{
    if (Membranes.Contains(MembraneID))
    {
        return Membranes[MembraneID];
    }
    return FMembraneStateData();
}

FString UMembraneHierarchyManager::GetParentMembrane(const FString& MembraneID) const
{
    if (Membranes.Contains(MembraneID))
    {
        return Membranes[MembraneID].ParentID;
    }
    return FString();
}

TArray<FString> UMembraneHierarchyManager::GetChildMembranes(const FString& MembraneID) const
{
    if (Membranes.Contains(MembraneID))
    {
        return Membranes[MembraneID].ChildIDs;
    }
    return TArray<FString>();
}

TArray<FString> UMembraneHierarchyManager::GetAllDescendants(const FString& MembraneID) const
{
    TArray<FString> Descendants;
    CollectDescendants(MembraneID, Descendants);
    return Descendants;
}

TArray<FString> UMembraneHierarchyManager::GetAncestorPath(const FString& MembraneID) const
{
    TArray<FString> Ancestors;
    
    if (!Membranes.Contains(MembraneID))
    {
        return Ancestors;
    }

    FString CurrentID = Membranes[MembraneID].ParentID;
    while (!CurrentID.IsEmpty() && Membranes.Contains(CurrentID))
    {
        Ancestors.Add(CurrentID);
        CurrentID = Membranes[CurrentID].ParentID;
    }

    return Ancestors;
}

TArray<FString> UMembraneHierarchyManager::GetSiblingMembranes(const FString& MembraneID) const
{
    TArray<FString> Siblings;

    if (!Membranes.Contains(MembraneID))
    {
        return Siblings;
    }

    const FString& ParentID = Membranes[MembraneID].ParentID;
    
    if (ParentID.IsEmpty())
    {
        // Root membrane: siblings are other roots
        for (const auto& Pair : Membranes)
        {
            if (Pair.Value.ParentID.IsEmpty() && Pair.Key != MembraneID)
            {
                Siblings.Add(Pair.Key);
            }
        }
    }
    else if (Membranes.Contains(ParentID))
    {
        for (const FString& ChildID : Membranes[ParentID].ChildIDs)
        {
            if (ChildID != MembraneID)
            {
                Siblings.Add(ChildID);
            }
        }
    }

    return Siblings;
}

TArray<FString> UMembraneHierarchyManager::GetRootMembranes() const
{
    TArray<FString> Roots;
    for (const auto& Pair : Membranes)
    {
        if (Pair.Value.ParentID.IsEmpty())
        {
            Roots.Add(Pair.Key);
        }
    }
    return Roots;
}

TArray<FString> UMembraneHierarchyManager::GetMembranesAtDepth(int32 Depth) const
{
    TArray<FString> Result;
    for (const auto& Pair : Membranes)
    {
        if (Pair.Value.NestingDepth == Depth)
        {
            Result.Add(Pair.Key);
        }
    }
    return Result;
}

bool UMembraneHierarchyManager::MembraneExists(const FString& MembraneID) const
{
    return Membranes.Contains(MembraneID);
}

int32 UMembraneHierarchyManager::GetNestingDepth(const FString& MembraneID) const
{
    if (Membranes.Contains(MembraneID))
    {
        return Membranes[MembraneID].NestingDepth;
    }
    return -1;
}

bool UMembraneHierarchyManager::IsAncestorOf(const FString& AncestorID, const FString& DescendantID) const
{
    if (!Membranes.Contains(AncestorID) || !Membranes.Contains(DescendantID))
    {
        return false;
    }

    TArray<FString> Ancestors = GetAncestorPath(DescendantID);
    return Ancestors.Contains(AncestorID);
}

// ========================================
// OBJECT MANAGEMENT
// ========================================

bool UMembraneHierarchyManager::AddObject(const FString& MembraneID, const FMembraneObject& Object)
{
    if (!MembraneExists(MembraneID))
    {
        return false;
    }

    FMembraneStateData& Membrane = Membranes[MembraneID];

    // Check capacity
    if (GlobalConfig.MaxObjectsPerMembrane > 0 && 
        Membrane.Objects.Num() >= GlobalConfig.MaxObjectsPerMembrane)
    {
        UE_LOG(LogTemp, Warning, TEXT("MembraneHierarchyManager: Membrane '%s' at object capacity"), *MembraneID);
        return false;
    }

    // Create a copy with assigned ID if needed
    FMembraneObject NewObject = Object;
    if (NewObject.ObjectID.IsEmpty())
    {
        NewObject.ObjectID = GenerateObjectID();
    }
    NewObject.CreationTime = CurrentTime;

    Membrane.Objects.Add(NewObject);
    UpdateMembraneStats(Membrane);

    return true;
}

bool UMembraneHierarchyManager::RemoveObject(const FString& MembraneID, const FString& ObjectID)
{
    if (!MembraneExists(MembraneID))
    {
        return false;
    }

    FMembraneStateData& Membrane = Membranes[MembraneID];

    for (int32 i = Membrane.Objects.Num() - 1; i >= 0; i--)
    {
        if (Membrane.Objects[i].ObjectID == ObjectID)
        {
            Membrane.Objects.RemoveAt(i);
            UpdateMembraneStats(Membrane);
            return true;
        }
    }

    return false;
}

bool UMembraneHierarchyManager::TransferObject(const FString& SourceID, const FString& TargetID, const FString& ObjectID)
{
    if (!MembraneExists(SourceID) || !MembraneExists(TargetID))
    {
        return false;
    }

    FMembraneStateData& Source = Membranes[SourceID];
    
    // Find the object
    int32 ObjectIndex = -1;
    for (int32 i = 0; i < Source.Objects.Num(); i++)
    {
        if (Source.Objects[i].ObjectID == ObjectID)
        {
            ObjectIndex = i;
            break;
        }
    }

    if (ObjectIndex == -1)
    {
        return false;
    }

    const FMembraneObject& Object = Source.Objects[ObjectIndex];

    // Check permeability for source (outward) and target (inward)
    bool bIsOutward = IsAncestorOf(TargetID, SourceID) || GetParentMembrane(SourceID) == TargetID;
    
    if (!CanPassThrough(SourceID, Object, true) || !CanPassThrough(TargetID, Object, false))
    {
        return false;
    }

    // Perform transfer
    if (AddObject(TargetID, Object))
    {
        Source.Objects.RemoveAt(ObjectIndex);
        UpdateMembraneStats(Source);
        OnObjectTransferred.Broadcast(SourceID, TargetID);
        return true;
    }

    return false;
}

TArray<FMembraneObject> UMembraneHierarchyManager::GetObjects(const FString& MembraneID) const
{
    if (Membranes.Contains(MembraneID))
    {
        return Membranes[MembraneID].Objects;
    }
    return TArray<FMembraneObject>();
}

TArray<FMembraneObject> UMembraneHierarchyManager::GetObjectsBySymbol(const FString& MembraneID, const FString& Symbol) const
{
    TArray<FMembraneObject> Result;
    
    if (!Membranes.Contains(MembraneID))
    {
        return Result;
    }

    for (const FMembraneObject& Obj : Membranes[MembraneID].Objects)
    {
        if (Obj.Symbol == Symbol)
        {
            Result.Add(Obj);
        }
    }

    return Result;
}

FString UMembraneHierarchyManager::FindObjectScope(const FString& MembraneID, const FString& ObjectID, bool bSearchDescendants) const
{
    if (!Membranes.Contains(MembraneID))
    {
        return FString();
    }

    // Check current membrane
    for (const FMembraneObject& Obj : Membranes[MembraneID].Objects)
    {
        if (Obj.ObjectID == ObjectID)
        {
            return MembraneID;
        }
    }

    // Check descendants
    if (bSearchDescendants)
    {
        for (const FString& ChildID : Membranes[MembraneID].ChildIDs)
        {
            FString Found = FindObjectScope(ChildID, ObjectID, true);
            if (!Found.IsEmpty())
            {
                return Found;
            }
        }
    }

    return FString();
}

// ========================================
// PERMEABILITY
// ========================================

bool UMembraneHierarchyManager::CanPassThrough(const FString& MembraneID, const FMembraneObject& Object, bool bOutward) const
{
    if (!Membranes.Contains(MembraneID))
    {
        return false;
    }

    const FMembraneStateData& Membrane = Membranes[MembraneID];
    const FPermeabilityRules& Rules = GlobalConfig.PermeabilityRules;

    // Check direction
    if (bOutward && !Rules.bAllowOutward)
    {
        return false;
    }
    if (!bOutward && !Rules.bAllowInward)
    {
        return false;
    }

    // Check permeability type
    switch (Rules.PermeabilityType)
    {
    case EPermeabilityType::Impermeable:
        return false;

    case EPermeabilityType::Permeable:
        return true;

    case EPermeabilityType::Semipermeable:
        return bOutward;  // Only allow outward passage

    case EPermeabilityType::Selective:
        // Check if symbol is blocked
        if (Rules.BlockedSymbols.Contains(Object.Symbol))
        {
            return false;
        }
        // Check if symbol is allowed (if allowlist is not empty)
        if (Rules.AllowedSymbols.Num() > 0 && !Rules.AllowedSymbols.Contains(Object.Symbol))
        {
            return false;
        }
        return true;

    default:
        return true;
    }
}

void UMembraneHierarchyManager::SetPermeabilityRules(const FString& MembraneID, const FPermeabilityRules& Rules)
{
    // Note: Per-membrane rules would require storing rules in FMembraneStateData
    // For now, this sets global config
    GlobalConfig.PermeabilityRules = Rules;
}

// ========================================
// STATE MANAGEMENT
// ========================================

void UMembraneHierarchyManager::SetMembraneState(const FString& MembraneID, EMembraneState NewState)
{
    if (!MembraneExists(MembraneID))
    {
        return;
    }

    EMembraneState OldState = Membranes[MembraneID].State;
    if (OldState != NewState)
    {
        Membranes[MembraneID].State = NewState;
        OnMembraneStateChanged.Broadcast(MembraneID, NewState);
    }
}

void UMembraneHierarchyManager::UpdateMembraneEnergy(const FString& MembraneID, float DeltaEnergy)
{
    if (!MembraneExists(MembraneID))
    {
        return;
    }

    FMembraneStateData& Membrane = Membranes[MembraneID];
    Membrane.EnergyLevel = FMath::Max(0.0f, Membrane.EnergyLevel + DeltaEnergy);
}

// ========================================
// STATISTICS
// ========================================

FMembraneHierarchyStats UMembraneHierarchyManager::GetHierarchyStats() const
{
    FMembraneHierarchyStats Stats;
    Stats.TotalMembraneCount = Membranes.Num();

    int32 TotalObjects = 0;
    int32 MaxDepth = 0;
    int32 ActiveCount = 0;
    int32 CompositeCount = 0;
    int32 TotalChildren = 0;

    for (const auto& Pair : Membranes)
    {
        const FMembraneStateData& M = Pair.Value;
        TotalObjects += M.TotalObjectCount;
        MaxDepth = FMath::Max(MaxDepth, M.NestingDepth);
        
        if (M.State == EMembraneState::Active)
        {
            ActiveCount++;
        }

        if (M.Type == EMembraneType::Composite || M.ChildIDs.Num() > 0)
        {
            CompositeCount++;
            TotalChildren += M.ChildIDs.Num();
        }
    }

    Stats.TotalObjectCount = TotalObjects;
    Stats.MaxNestingDepthAchieved = MaxDepth;
    Stats.ActiveMembraneCount = ActiveCount;
    Stats.AverageObjectsPerMembrane = Stats.TotalMembraneCount > 0 ? 
        (float)TotalObjects / Stats.TotalMembraneCount : 0.0f;
    Stats.AverageChildrenPerMembrane = CompositeCount > 0 ?
        (float)TotalChildren / CompositeCount : 0.0f;

    return Stats;
}

int32 UMembraneHierarchyManager::GetMembraneCount() const
{
    return Membranes.Num();
}

FString UMembraneHierarchyManager::GetHierarchyString() const
{
    FString Result;
    
    // Find all root membranes and print their trees
    TArray<FString> Roots = GetRootMembranes();
    for (const FString& RootID : Roots)
    {
        BuildHierarchyString(RootID, 0, Result);
    }

    return Result;
}

// ========================================
// INTERNAL METHODS
// ========================================

FString UMembraneHierarchyManager::GenerateMembraneID()
{
    return FString::Printf(TEXT("MEM_%d_%lld"), ++MembraneIDCounter, FDateTime::Now().GetTicks());
}

FString UMembraneHierarchyManager::GenerateObjectID()
{
    return FString::Printf(TEXT("OBJ_%d_%lld"), ++ObjectIDCounter, FDateTime::Now().GetTicks());
}

void UMembraneHierarchyManager::UpdateMembraneStats(FMembraneStateData& Membrane)
{
    int32 TotalCount = 0;
    for (const FMembraneObject& Obj : Membrane.Objects)
    {
        TotalCount += Obj.Multiplicity;
    }
    Membrane.TotalObjectCount = TotalCount;

    // Update type if needed
    if (Membrane.ChildIDs.Num() > 0 && Membrane.Type == EMembraneType::Elementary)
    {
        Membrane.Type = EMembraneType::Composite;
    }
    else if (Membrane.ChildIDs.Num() == 0 && Membrane.Type == EMembraneType::Composite && 
             Membrane.ParentID.IsEmpty() == false)
    {
        Membrane.Type = EMembraneType::Elementary;
    }
}

void UMembraneHierarchyManager::ProcessMessageQueue()
{
    // Process all pending messages
    while (MessageQueue.Num() > 0)
    {
        FMembraneMessage Message = MessageQueue[0];
        MessageQueue.RemoveAt(0);

        // Transfer each object in the message
        for (const FMembraneObject& Obj : Message.Objects)
        {
            if (Message.bOutward)
            {
                // Outward: to parent
                FString TargetID = Message.TargetID.IsEmpty() ? 
                    GetParentMembrane(Message.SourceID) : Message.TargetID;
                if (!TargetID.IsEmpty())
                {
                    AddObject(TargetID, Obj);
                }
            }
            else
            {
                // Inward: to specified target
                if (!Message.TargetID.IsEmpty())
                {
                    AddObject(Message.TargetID, Obj);
                }
            }
        }
    }
}

void UMembraneHierarchyManager::CollectDescendants(const FString& MembraneID, TArray<FString>& OutDescendants) const
{
    if (!Membranes.Contains(MembraneID))
    {
        return;
    }

    for (const FString& ChildID : Membranes[MembraneID].ChildIDs)
    {
        OutDescendants.Add(ChildID);
        CollectDescendants(ChildID, OutDescendants);
    }
}

void UMembraneHierarchyManager::BuildHierarchyString(const FString& MembraneID, int32 Indent, FString& OutString) const
{
    if (!Membranes.Contains(MembraneID))
    {
        return;
    }

    const FMembraneStateData& Membrane = Membranes[MembraneID];

    // Add indentation
    for (int32 i = 0; i < Indent; i++)
    {
        OutString += TEXT("  ");
    }

    // Add membrane info
    OutString += FString::Printf(TEXT("[%s] %s (depth=%d, objects=%d, children=%d)\n"),
        *MembraneID,
        *Membrane.Label,
        Membrane.NestingDepth,
        Membrane.Objects.Num(),
        Membrane.ChildIDs.Num());

    // Recursively add children
    for (const FString& ChildID : Membrane.ChildIDs)
    {
        BuildHierarchyString(ChildID, Indent + 1, OutString);
    }
}

bool UMembraneHierarchyManager::ValidateConfig(const FMembraneConfig& Config) const
{
    // Basic validation
    if (Config.MaxNestingDepth < 0)
    {
        return false;
    }
    return true;
}

bool UMembraneHierarchyManager::CanAddChild(const FString& ParentID) const
{
    if (!Membranes.Contains(ParentID))
    {
        return false;
    }

    const FMembraneStateData& Parent = Membranes[ParentID];

    // Check max children limit
    if (GlobalConfig.MaxChildMembranes > 0 && 
        Parent.ChildIDs.Num() >= GlobalConfig.MaxChildMembranes)
    {
        return false;
    }

    // Check state
    if (Parent.State != EMembraneState::Active)
    {
        return false;
    }

    return true;
}

void UMembraneHierarchyManager::CleanupDissolvedMembrane(const FString& MembraneID)
{
    // Any additional cleanup needed before removal
    // Currently handled in DissolveMembrane
}

void UMembraneHierarchyManager::UpdateHierarchyAfterChange()
{
    // Update nesting depths for all membranes
    for (auto& Pair : Membranes)
    {
        if (Pair.Value.ParentID.IsEmpty())
        {
            Pair.Value.NestingDepth = 0;
        }
        else if (Membranes.Contains(Pair.Value.ParentID))
        {
            Pair.Value.NestingDepth = Membranes[Pair.Value.ParentID].NestingDepth + 1;
        }
    }
}

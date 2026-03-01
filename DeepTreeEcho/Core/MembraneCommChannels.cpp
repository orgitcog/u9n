/**
 * Membrane Communication Channels - P-System Symport/Antiport Transport
 * 
 * Implementation of Feature F1.3.4: Communication Channels
 * Provides symport/antiport communication channels between adjacent membranes.
 */

#include "MembraneCommChannels.h"

UMembraneCommChannels::UMembraneCommChannels()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UMembraneCommChannels::BeginPlay()
{
    Super::BeginPlay();
    
    // Create root/skin membrane if none exists
    if (Membranes.Num() == 0)
    {
        CreateMembrane(TEXT(""));
    }
}

void UMembraneCommChannels::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableChannelProcessing)
    {
        return;
    }

    CurrentTime += DeltaTime;

    // Update membrane energy levels
    UpdateMembraneEnergy(DeltaTime);

    // Update channel states (refractory timers, etc.)
    UpdateChannelStates(DeltaTime);

    // Process pending transports
    ProcessPendingTransports();
}

// ========================================
// PUBLIC API - MEMBRANE MANAGEMENT
// ========================================

FString UMembraneCommChannels::CreateMembrane(const FString& ParentMembraneID)
{
    if (Membranes.Num() >= Config.MaxMembranes)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create membrane: maximum limit reached (%d)"), Config.MaxMembranes);
        return TEXT("");
    }

    FMembraneState NewMembrane;
    NewMembrane.MembraneID = GenerateUniqueID(TEXT("Membrane"));
    NewMembrane.ParentMembraneID = ParentMembraneID;
    NewMembrane.EnergyLevel = NewMembrane.MaxEnergy;
    NewMembrane.bIsActive = true;

    // Calculate nesting depth
    if (ParentMembraneID.IsEmpty())
    {
        NewMembrane.NestingDepth = 0;
    }
    else
    {
        FMembraneState* Parent = Membranes.Find(ParentMembraneID);
        if (Parent)
        {
            NewMembrane.NestingDepth = Parent->NestingDepth + 1;
            Parent->ChildMembraneIDs.Add(NewMembrane.MembraneID);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Parent membrane not found: %s"), *ParentMembraneID);
            return TEXT("");
        }
    }

    Membranes.Add(NewMembrane.MembraneID, NewMembrane);
    
    UE_LOG(LogTemp, Log, TEXT("Created membrane: %s (parent: %s, depth: %d)"), 
           *NewMembrane.MembraneID, *ParentMembraneID, NewMembrane.NestingDepth);

    return NewMembrane.MembraneID;
}

bool UMembraneCommChannels::RemoveMembrane(const FString& MembraneID)
{
    FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (!Membrane)
    {
        return false;
    }

    // Move contained objects to parent membrane
    if (!Membrane->ParentMembraneID.IsEmpty())
    {
        FMembraneState* Parent = Membranes.Find(Membrane->ParentMembraneID);
        if (Parent)
        {
            Parent->ContainedObjects.Append(Membrane->ContainedObjects);
            Parent->ChildMembraneIDs.Remove(MembraneID);
        }
    }

    // Recursively remove child membranes
    TArray<FString> ChildrenToRemove = Membrane->ChildMembraneIDs;
    for (const FString& ChildID : ChildrenToRemove)
    {
        RemoveMembrane(ChildID);
    }

    // Remove associated channels
    TArray<FString> ChannelsToRemove;
    for (const auto& ChannelPair : Channels)
    {
        if (ChannelPair.Value.MembraneA_ID == MembraneID || 
            ChannelPair.Value.MembraneB_ID == MembraneID)
        {
            ChannelsToRemove.Add(ChannelPair.Key);
        }
    }
    for (const FString& ChannelID : ChannelsToRemove)
    {
        RemoveChannel(ChannelID);
    }

    Membranes.Remove(MembraneID);
    
    UE_LOG(LogTemp, Log, TEXT("Removed membrane: %s"), *MembraneID);
    return true;
}

bool UMembraneCommChannels::GetMembraneState(const FString& MembraneID, FMembraneState& OutState) const
{
    const FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (Membrane)
    {
        OutState = *Membrane;
        return true;
    }
    return false;
}

bool UMembraneCommChannels::AddObjectToMembrane(const FString& MembraneID, const FMembraneObject& Object)
{
    FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (!Membrane)
    {
        return false;
    }

    FMembraneObject NewObject = Object;
    if (NewObject.ObjectID.IsEmpty())
    {
        NewObject.ObjectID = GenerateUniqueID(TEXT("Obj"));
    }
    NewObject.CreationTime = CurrentTime;

    Membrane->ContainedObjects.Add(NewObject);
    return true;
}

bool UMembraneCommChannels::RemoveObjectFromMembrane(const FString& MembraneID, const FString& ObjectID)
{
    FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (!Membrane)
    {
        return false;
    }

    for (int32 i = Membrane->ContainedObjects.Num() - 1; i >= 0; --i)
    {
        if (Membrane->ContainedObjects[i].ObjectID == ObjectID)
        {
            Membrane->ContainedObjects.RemoveAt(i);
            return true;
        }
    }
    return false;
}

TArray<FMembraneObject> UMembraneCommChannels::GetObjectsBySymbol(const FString& MembraneID, const FString& Symbol) const
{
    TArray<FMembraneObject> Result;
    const FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (Membrane)
    {
        for (const FMembraneObject& Obj : Membrane->ContainedObjects)
        {
            if (Obj.Symbol == Symbol)
            {
                Result.Add(Obj);
            }
        }
    }
    return Result;
}

int32 UMembraneCommChannels::CountObjectsInMembrane(const FString& MembraneID, const FString& Symbol) const
{
    int32 Count = 0;
    const FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (Membrane)
    {
        for (const FMembraneObject& Obj : Membrane->ContainedObjects)
        {
            if (Symbol.IsEmpty() || Obj.Symbol == Symbol)
            {
                Count += Obj.Multiplicity;
            }
        }
    }
    return Count;
}

// ========================================
// PUBLIC API - CHANNEL MANAGEMENT
// ========================================

FString UMembraneCommChannels::CreateSymportChannel(const FString& MembraneA_ID, const FString& MembraneB_ID,
                                                     const TArray<FString>& SelectiveFor,
                                                     EChannelDirection Direction)
{
    if (Channels.Num() >= Config.MaxChannels)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create channel: maximum limit reached (%d)"), Config.MaxChannels);
        return TEXT("");
    }

    if (!AreMembranesAdjacent(MembraneA_ID, MembraneB_ID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create symport channel: membranes are not adjacent"));
        return TEXT("");
    }

    FCommunicationChannel NewChannel;
    NewChannel.ChannelID = GenerateUniqueID(TEXT("Symport"));
    NewChannel.ChannelType = EChannelType::Symport;
    NewChannel.MembraneA_ID = MembraneA_ID;
    NewChannel.MembraneB_ID = MembraneB_ID;
    NewChannel.SelectiveFor = SelectiveFor;
    NewChannel.DefaultDirection = Direction;
    NewChannel.State = EChannelState::Closed;
    NewChannel.MaxTransportRate = Config.DefaultTransportRate;

    Channels.Add(NewChannel.ChannelID, NewChannel);

    // Register channel with membranes
    if (FMembraneState* MemA = Membranes.Find(MembraneA_ID))
    {
        MemA->BoundaryChannelIDs.Add(NewChannel.ChannelID);
    }
    if (FMembraneState* MemB = Membranes.Find(MembraneB_ID))
    {
        MemB->BoundaryChannelIDs.Add(NewChannel.ChannelID);
    }

    UE_LOG(LogTemp, Log, TEXT("Created symport channel: %s between %s and %s"), 
           *NewChannel.ChannelID, *MembraneA_ID, *MembraneB_ID);

    return NewChannel.ChannelID;
}

FString UMembraneCommChannels::CreateAntiportChannel(const FString& MembraneA_ID, const FString& MembraneB_ID,
                                                      const TArray<FString>& SelectiveFor,
                                                      EChannelDirection Direction)
{
    if (Channels.Num() >= Config.MaxChannels)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create channel: maximum limit reached (%d)"), Config.MaxChannels);
        return TEXT("");
    }

    if (!AreMembranesAdjacent(MembraneA_ID, MembraneB_ID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create antiport channel: membranes are not adjacent"));
        return TEXT("");
    }

    FCommunicationChannel NewChannel;
    NewChannel.ChannelID = GenerateUniqueID(TEXT("Antiport"));
    NewChannel.ChannelType = EChannelType::Antiport;
    NewChannel.MembraneA_ID = MembraneA_ID;
    NewChannel.MembraneB_ID = MembraneB_ID;
    NewChannel.SelectiveFor = SelectiveFor;
    NewChannel.DefaultDirection = Direction;
    NewChannel.State = EChannelState::Closed;
    NewChannel.MaxTransportRate = Config.DefaultTransportRate;

    Channels.Add(NewChannel.ChannelID, NewChannel);

    // Register channel with membranes
    if (FMembraneState* MemA = Membranes.Find(MembraneA_ID))
    {
        MemA->BoundaryChannelIDs.Add(NewChannel.ChannelID);
    }
    if (FMembraneState* MemB = Membranes.Find(MembraneB_ID))
    {
        MemB->BoundaryChannelIDs.Add(NewChannel.ChannelID);
    }

    UE_LOG(LogTemp, Log, TEXT("Created antiport channel: %s between %s and %s"), 
           *NewChannel.ChannelID, *MembraneA_ID, *MembraneB_ID);

    return NewChannel.ChannelID;
}

FString UMembraneCommChannels::CreateUniportChannel(const FString& MembraneA_ID, const FString& MembraneB_ID,
                                                     const TArray<FString>& SelectiveFor)
{
    if (Channels.Num() >= Config.MaxChannels)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create channel: maximum limit reached (%d)"), Config.MaxChannels);
        return TEXT("");
    }

    if (!AreMembranesAdjacent(MembraneA_ID, MembraneB_ID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create uniport channel: membranes are not adjacent"));
        return TEXT("");
    }

    FCommunicationChannel NewChannel;
    NewChannel.ChannelID = GenerateUniqueID(TEXT("Uniport"));
    NewChannel.ChannelType = EChannelType::Uniport;
    NewChannel.MembraneA_ID = MembraneA_ID;
    NewChannel.MembraneB_ID = MembraneB_ID;
    NewChannel.SelectiveFor = SelectiveFor;
    NewChannel.DefaultDirection = EChannelDirection::Bidirectional;
    NewChannel.State = EChannelState::Closed;
    NewChannel.MaxTransportRate = Config.DefaultTransportRate;

    Channels.Add(NewChannel.ChannelID, NewChannel);

    // Register channel with membranes
    if (FMembraneState* MemA = Membranes.Find(MembraneA_ID))
    {
        MemA->BoundaryChannelIDs.Add(NewChannel.ChannelID);
    }
    if (FMembraneState* MemB = Membranes.Find(MembraneB_ID))
    {
        MemB->BoundaryChannelIDs.Add(NewChannel.ChannelID);
    }

    UE_LOG(LogTemp, Log, TEXT("Created uniport channel: %s between %s and %s"), 
           *NewChannel.ChannelID, *MembraneA_ID, *MembraneB_ID);

    return NewChannel.ChannelID;
}

bool UMembraneCommChannels::RemoveChannel(const FString& ChannelID)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel)
    {
        return false;
    }

    // Remove channel reference from membranes
    if (FMembraneState* MemA = Membranes.Find(Channel->MembraneA_ID))
    {
        MemA->BoundaryChannelIDs.Remove(ChannelID);
    }
    if (FMembraneState* MemB = Membranes.Find(Channel->MembraneB_ID))
    {
        MemB->BoundaryChannelIDs.Remove(ChannelID);
    }

    Channels.Remove(ChannelID);
    
    UE_LOG(LogTemp, Log, TEXT("Removed channel: %s"), *ChannelID);
    return true;
}

bool UMembraneCommChannels::OpenChannel(const FString& ChannelID)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel)
    {
        return false;
    }

    if (Channel->State != EChannelState::Blocked)
    {
        SetChannelState(ChannelID, EChannelState::Open);
        return true;
    }
    return false;
}

bool UMembraneCommChannels::CloseChannel(const FString& ChannelID)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel)
    {
        return false;
    }

    SetChannelState(ChannelID, EChannelState::Closed);
    return true;
}

bool UMembraneCommChannels::GetChannelState(const FString& ChannelID, FCommunicationChannel& OutChannel) const
{
    const FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (Channel)
    {
        OutChannel = *Channel;
        return true;
    }
    return false;
}

bool UMembraneCommChannels::AddTransportRule(const FString& ChannelID, const FTransportRule& Rule)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel)
    {
        return false;
    }

    FTransportRule NewRule = Rule;
    if (NewRule.RuleID.IsEmpty())
    {
        NewRule.RuleID = GenerateUniqueID(TEXT("Rule"));
    }

    Channel->TransportRules.Add(NewRule);
    
    // Sort rules by priority (higher first)
    Channel->TransportRules.Sort([](const FTransportRule& A, const FTransportRule& B) {
        return A.Priority > B.Priority;
    });

    return true;
}

bool UMembraneCommChannels::RemoveTransportRule(const FString& ChannelID, const FString& RuleID)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel)
    {
        return false;
    }

    for (int32 i = Channel->TransportRules.Num() - 1; i >= 0; --i)
    {
        if (Channel->TransportRules[i].RuleID == RuleID)
        {
            Channel->TransportRules.RemoveAt(i);
            return true;
        }
    }
    return false;
}

// ========================================
// PUBLIC API - TRANSPORT OPERATIONS
// ========================================

bool UMembraneCommChannels::ExecuteSymport(const FString& ChannelID, const TArray<FString>& ObjectSymbols)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel || Channel->State != EChannelState::Open)
    {
        return false;
    }

    if (Channel->ChannelType != EChannelType::Symport && Channel->ChannelType != EChannelType::Uniport)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExecuteSymport called on non-symport channel: %s"), *ChannelID);
        return false;
    }

    // Determine source and target based on direction
    FString SourceID, TargetID;
    if (Channel->DefaultDirection == EChannelDirection::Inward)
    {
        SourceID = Channel->MembraneA_ID; // parent
        TargetID = Channel->MembraneB_ID; // child
    }
    else
    {
        SourceID = Channel->MembraneB_ID; // child
        TargetID = Channel->MembraneA_ID; // parent
    }

    // Check if objects are available
    if (!AreObjectsAvailable(SourceID, ObjectSymbols))
    {
        return false;
    }

    // Find applicable rule
    const FTransportRule* Rule = FindApplicableRule(*Channel, ObjectSymbols, TArray<FString>());
    if (!Rule && !Channel->SelectiveFor.Contains(ObjectSymbols[0]))
    {
        // Use default selectivity if no rule matches
        bool bAllowedBySelectivity = true;
        for (const FString& Symbol : ObjectSymbols)
        {
            if (Channel->SelectiveFor.Num() > 0 && !Channel->SelectiveFor.Contains(Symbol))
            {
                bAllowedBySelectivity = false;
                break;
            }
        }
        if (!bAllowedBySelectivity)
        {
            return false;
        }
    }

    // Check energy requirements
    float EnergyCost = ObjectSymbols.Num() * 1.0f; // Default cost per object
    if (Rule)
    {
        EnergyCost = Rule->MinimumEnergy;
    }
    
    if (Config.bEnableEnergyConstraints && !ConsumeMembraneEnergy(SourceID, EnergyCost))
    {
        return false;
    }

    // Execute transport
    TArray<FMembraneObject> MovedObjects;
    if (!MoveObjectsBetweenMembranes(SourceID, TargetID, ObjectSymbols, MovedObjects))
    {
        return false;
    }

    // Create transport event
    FTransportEvent Event;
    Event.EventID = GenerateUniqueID(TEXT("Event"));
    Event.ChannelID = ChannelID;
    Event.RuleID = Rule ? Rule->RuleID : TEXT("Default");
    Event.SourceMembraneID = SourceID;
    Event.TargetMembraneID = TargetID;
    Event.TransportedToTarget = MovedObjects;
    Event.Timestamp = CurrentTime;
    Event.EnergyConsumed = EnergyCost;
    Event.bSuccessful = true;

    LogTransportEvent(Event);

    // Update channel state
    Channel->TotalTransportCount += MovedObjects.Num();
    Channel->LastTransportTime = CurrentTime;
    Channel->RefractoryTimer = Channel->RefractoryPeriod;
    SetChannelState(ChannelID, EChannelState::Refractory);

    return true;
}

bool UMembraneCommChannels::ExecuteAntiport(const FString& ChannelID, 
                                             const TArray<FString>& SourceObjectSymbols,
                                             const TArray<FString>& TargetObjectSymbols)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel || Channel->State != EChannelState::Open)
    {
        return false;
    }

    if (Channel->ChannelType != EChannelType::Antiport)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExecuteAntiport called on non-antiport channel: %s"), *ChannelID);
        return false;
    }

    FString SourceID = Channel->MembraneB_ID; // typically child
    FString TargetID = Channel->MembraneA_ID; // typically parent

    // Check if objects are available on both sides
    if (!AreObjectsAvailable(SourceID, SourceObjectSymbols))
    {
        return false;
    }
    if (!AreObjectsAvailable(TargetID, TargetObjectSymbols))
    {
        return false;
    }

    // Find applicable rule
    const FTransportRule* Rule = FindApplicableRule(*Channel, SourceObjectSymbols, TargetObjectSymbols);
    if (Rule && !ShouldApplyRule(*Rule))
    {
        return false;
    }

    // Check energy requirements
    float EnergyCost = (SourceObjectSymbols.Num() + TargetObjectSymbols.Num()) * 1.0f;
    if (Rule)
    {
        EnergyCost = Rule->MinimumEnergy;
    }

    if (Config.bEnableEnergyConstraints && !ConsumeMembraneEnergy(SourceID, EnergyCost))
    {
        return false;
    }

    // Execute counter-transport: source->target and target->source
    TArray<FMembraneObject> MovedToTarget, MovedToSource;
    
    if (!MoveObjectsBetweenMembranes(SourceID, TargetID, SourceObjectSymbols, MovedToTarget))
    {
        return false;
    }
    
    if (!MoveObjectsBetweenMembranes(TargetID, SourceID, TargetObjectSymbols, MovedToSource))
    {
        // Rollback first transport
        for (const FMembraneObject& Obj : MovedToTarget)
        {
            AddObjectToMembrane(SourceID, Obj);
        }
        return false;
    }

    // Create transport event
    FTransportEvent Event;
    Event.EventID = GenerateUniqueID(TEXT("Event"));
    Event.ChannelID = ChannelID;
    Event.RuleID = Rule ? Rule->RuleID : TEXT("Default");
    Event.SourceMembraneID = SourceID;
    Event.TargetMembraneID = TargetID;
    Event.TransportedToTarget = MovedToTarget;
    Event.TransportedToSource = MovedToSource;
    Event.Timestamp = CurrentTime;
    Event.EnergyConsumed = EnergyCost;
    Event.bSuccessful = true;

    LogTransportEvent(Event);

    // Update channel state
    Channel->TotalTransportCount += MovedToTarget.Num() + MovedToSource.Num();
    Channel->LastTransportTime = CurrentTime;
    Channel->RefractoryTimer = Channel->RefractoryPeriod;
    SetChannelState(ChannelID, EChannelState::Refractory);

    return true;
}

bool UMembraneCommChannels::QueueObjectForTransport(const FString& ChannelID, const FMembraneObject& Object)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel)
    {
        return false;
    }

    Channel->TransportQueue.Add(Object);
    return true;
}

int32 UMembraneCommChannels::ProcessPendingTransports()
{
    int32 TotalProcessed = 0;

    for (auto& ChannelPair : Channels)
    {
        TotalProcessed += ProcessChannelQueue(ChannelPair.Key);
    }

    return TotalProcessed;
}

bool UMembraneCommChannels::CanTransport(const FString& ChannelID, const TArray<FString>& ObjectSymbols) const
{
    const FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel || Channel->State != EChannelState::Open)
    {
        return false;
    }

    // Check selectivity
    for (const FString& Symbol : ObjectSymbols)
    {
        if (Channel->SelectiveFor.Num() > 0 && !Channel->SelectiveFor.Contains(Symbol))
        {
            return false;
        }
    }

    // Check rate limit
    if (Channel->CurrentTransportRate >= Channel->MaxTransportRate)
    {
        return false;
    }

    return true;
}

// ========================================
// PUBLIC API - ANALYSIS
// ========================================

float UMembraneCommChannels::GetChannelTransportRate(const FString& ChannelID) const
{
    const FCommunicationChannel* Channel = Channels.Find(ChannelID);
    return Channel ? Channel->CurrentTransportRate : 0.0f;
}

int32 UMembraneCommChannels::GetTotalObjectCount() const
{
    int32 Total = 0;
    for (const auto& MembranePair : Membranes)
    {
        for (const FMembraneObject& Obj : MembranePair.Value.ContainedObjects)
        {
            Total += Obj.Multiplicity;
        }
    }
    return Total;
}

int32 UMembraneCommChannels::GetMaxNestingDepth() const
{
    int32 MaxDepth = 0;
    for (const auto& MembranePair : Membranes)
    {
        MaxDepth = FMath::Max(MaxDepth, MembranePair.Value.NestingDepth);
    }
    return MaxDepth;
}

TArray<FString> UMembraneCommChannels::GetChannelsForMembrane(const FString& MembraneID) const
{
    TArray<FString> Result;
    const FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (Membrane)
    {
        Result = Membrane->BoundaryChannelIDs;
    }
    return Result;
}

TArray<FTransportEvent> UMembraneCommChannels::GetRecentTransportEvents(const FString& ChannelID, int32 MaxEvents) const
{
    TArray<FTransportEvent> Result;
    
    for (int32 i = TransportEventLog.Num() - 1; i >= 0 && Result.Num() < MaxEvents; --i)
    {
        if (ChannelID.IsEmpty() || TransportEventLog[i].ChannelID == ChannelID)
        {
            Result.Add(TransportEventLog[i]);
        }
    }
    
    return Result;
}

void UMembraneCommChannels::ResetChannelManager()
{
    Membranes.Empty();
    Channels.Empty();
    TransportEventLog.Empty();
    CurrentTime = 0.0f;
    IDCounter = 0;
    
    // Create root membrane
    CreateMembrane(TEXT(""));
    
    UE_LOG(LogTemp, Log, TEXT("Membrane channel manager reset"));
}

// ========================================
// INTERNAL METHODS
// ========================================

FString UMembraneCommChannels::GenerateUniqueID(const FString& Prefix)
{
    return FString::Printf(TEXT("%s_%d"), *Prefix, ++IDCounter);
}

bool UMembraneCommChannels::AreMembranesAdjacent(const FString& MembraneA_ID, const FString& MembraneB_ID) const
{
    const FMembraneState* MemA = Membranes.Find(MembraneA_ID);
    const FMembraneState* MemB = Membranes.Find(MembraneB_ID);
    
    if (!MemA || !MemB)
    {
        return false;
    }

    // Adjacent if one is the parent of the other
    return (MemA->ParentMembraneID == MembraneB_ID || MemB->ParentMembraneID == MembraneA_ID);
}

void UMembraneCommChannels::UpdateMembraneEnergy(float DeltaTime)
{
    for (auto& MembranePair : Membranes)
    {
        FMembraneState& Membrane = MembranePair.Value;
        if (Membrane.bIsActive)
        {
            // Regenerate energy
            float Regen = Membrane.EnergyRegenRate * Config.EnergyRegenMultiplier * DeltaTime;
            Membrane.EnergyLevel = FMath::Min(Membrane.MaxEnergy, Membrane.EnergyLevel + Regen);
        }
    }
}

bool UMembraneCommChannels::ConsumeMembraneEnergy(const FString& MembraneID, float Amount)
{
    FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (!Membrane)
    {
        return false;
    }

    if (Membrane->EnergyLevel >= Amount)
    {
        Membrane->EnergyLevel -= Amount;
        return true;
    }
    else
    {
        OnMembraneEnergyDepleted.Broadcast(MembraneID);
        return false;
    }
}

void UMembraneCommChannels::UpdateChannelStates(float DeltaTime)
{
    for (auto& ChannelPair : Channels)
    {
        FCommunicationChannel& Channel = ChannelPair.Value;
        
        // Update refractory timer
        if (Channel.State == EChannelState::Refractory)
        {
            Channel.RefractoryTimer -= DeltaTime;
            if (Channel.RefractoryTimer <= 0.0f)
            {
                Channel.RefractoryTimer = 0.0f;
                SetChannelState(ChannelPair.Key, EChannelState::Open);
            }
        }

        // Calculate current transport rate (objects per second, exponential decay)
        float TimeSinceTransport = CurrentTime - Channel.LastTransportTime;
        float DecayFactor = FMath::Exp(-TimeSinceTransport);
        Channel.CurrentTransportRate = Channel.CurrentTransportRate * DecayFactor;
    }
}

void UMembraneCommChannels::SetChannelState(const FString& ChannelID, EChannelState NewState)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (Channel && Channel->State != NewState)
    {
        EChannelState OldState = Channel->State;
        Channel->State = NewState;
        OnChannelStateChanged.Broadcast(ChannelID, NewState);
        
        UE_LOG(LogTemp, Verbose, TEXT("Channel %s state: %d -> %d"), 
               *ChannelID, (int32)OldState, (int32)NewState);
    }
}

const FTransportRule* UMembraneCommChannels::FindApplicableRule(const FCommunicationChannel& Channel,
                                                                  const TArray<FString>& SourceObjects,
                                                                  const TArray<FString>& TargetObjects) const
{
    for (const FTransportRule& Rule : Channel.TransportRules)
    {
        if (!Rule.bEnabled)
        {
            continue;
        }

        // Check if source objects match
        bool bSourceMatch = true;
        for (const FString& Required : Rule.SourceObjects)
        {
            if (!SourceObjects.Contains(Required))
            {
                bSourceMatch = false;
                break;
            }
        }
        
        if (!bSourceMatch)
        {
            continue;
        }

        // For antiport, check target objects
        if (Rule.ChannelType == EChannelType::Antiport)
        {
            bool bTargetMatch = true;
            for (const FString& Required : Rule.TargetObjects)
            {
                if (!TargetObjects.Contains(Required))
                {
                    bTargetMatch = false;
                    break;
                }
            }
            if (!bTargetMatch)
            {
                continue;
            }
        }

        return &Rule;
    }
    
    return nullptr;
}

FTransportEvent UMembraneCommChannels::ExecuteTransportRule(const FString& ChannelID, 
                                                             const FTransportRule& Rule,
                                                             const FString& SourceMembraneID,
                                                             const FString& TargetMembraneID)
{
    FTransportEvent Event;
    Event.EventID = GenerateUniqueID(TEXT("Event"));
    Event.RuleID = Rule.RuleID;
    Event.ChannelID = ChannelID;
    Event.SourceMembraneID = SourceMembraneID;
    Event.TargetMembraneID = TargetMembraneID;
    Event.Timestamp = CurrentTime;
    Event.bSuccessful = false;

    // This is a simplified implementation; actual rule execution would be more complex
    TArray<FMembraneObject> MovedObjects;
    if (MoveObjectsBetweenMembranes(SourceMembraneID, TargetMembraneID, Rule.SourceObjects, MovedObjects))
    {
        Event.TransportedToTarget = MovedObjects;
        Event.bSuccessful = true;
    }

    return Event;
}

bool UMembraneCommChannels::MoveObjectsBetweenMembranes(const FString& SourceID, 
                                                         const FString& TargetID,
                                                         const TArray<FString>& Symbols,
                                                         TArray<FMembraneObject>& OutMovedObjects)
{
    FMembraneState* Source = Membranes.Find(SourceID);
    FMembraneState* Target = Membranes.Find(TargetID);
    
    if (!Source || !Target)
    {
        return false;
    }

    OutMovedObjects.Empty();

    // Find and move objects
    for (const FString& Symbol : Symbols)
    {
        bool bFound = false;
        for (int32 i = Source->ContainedObjects.Num() - 1; i >= 0; --i)
        {
            FMembraneObject& Obj = Source->ContainedObjects[i];
            if (Obj.Symbol == Symbol && !Obj.bIsCatalyst)
            {
                if (Obj.Multiplicity > 1)
                {
                    // Split off one object
                    FMembraneObject NewObj = Obj;
                    NewObj.Multiplicity = 1;
                    NewObj.ObjectID = GenerateUniqueID(TEXT("Obj"));
                    OutMovedObjects.Add(NewObj);
                    Obj.Multiplicity--;
                }
                else
                {
                    OutMovedObjects.Add(Obj);
                    Source->ContainedObjects.RemoveAt(i);
                }
                bFound = true;
                break;
            }
        }
        
        if (!bFound)
        {
            // Rollback - return already moved objects
            for (const FMembraneObject& MovedObj : OutMovedObjects)
            {
                Source->ContainedObjects.Add(MovedObj);
            }
            OutMovedObjects.Empty();
            return false;
        }
    }

    // Add objects to target
    for (const FMembraneObject& Obj : OutMovedObjects)
    {
        Target->ContainedObjects.Add(Obj);
    }

    return true;
}

bool UMembraneCommChannels::AreObjectsAvailable(const FString& MembraneID, const TArray<FString>& Symbols) const
{
    const FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (!Membrane)
    {
        return false;
    }

    // Count required symbols
    TMap<FString, int32> RequiredCounts;
    for (const FString& Symbol : Symbols)
    {
        RequiredCounts.FindOrAdd(Symbol)++;
    }

    // Count available objects
    TMap<FString, int32> AvailableCounts;
    for (const FMembraneObject& Obj : Membrane->ContainedObjects)
    {
        if (!Obj.bIsCatalyst)
        {
            AvailableCounts.FindOrAdd(Obj.Symbol) += Obj.Multiplicity;
        }
    }

    // Check if enough objects available
    for (const auto& Required : RequiredCounts)
    {
        int32* Available = AvailableCounts.Find(Required.Key);
        if (!Available || *Available < Required.Value)
        {
            return false;
        }
    }

    return true;
}

bool UMembraneCommChannels::AreCatalystsPresent(const FString& MembraneID, const TArray<FString>& CatalystSymbols) const
{
    const FMembraneState* Membrane = Membranes.Find(MembraneID);
    if (!Membrane)
    {
        return false;
    }

    for (const FString& CatalystSymbol : CatalystSymbols)
    {
        bool bFound = false;
        for (const FMembraneObject& Obj : Membrane->ContainedObjects)
        {
            if (Obj.Symbol == CatalystSymbol && Obj.bIsCatalyst)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            return false;
        }
    }

    return true;
}

void UMembraneCommChannels::LogTransportEvent(const FTransportEvent& Event)
{
    if (!Config.bEnableEventLogging)
    {
        return;
    }

    TransportEventLog.Add(Event);
    
    // Trim log if needed
    while (TransportEventLog.Num() > Config.MaxEventLogSize)
    {
        TransportEventLog.RemoveAt(0);
    }

    OnTransportOccurred.Broadcast(Event);
}

bool UMembraneCommChannels::ShouldApplyRule(const FTransportRule& Rule) const
{
    if (!Config.bEnableStochasticRules)
    {
        return true;
    }

    return FMath::FRand() <= Rule.ApplicationProbability;
}

int32 UMembraneCommChannels::ProcessChannelQueue(const FString& ChannelID)
{
    FCommunicationChannel* Channel = Channels.Find(ChannelID);
    if (!Channel || Channel->State != EChannelState::Open)
    {
        return 0;
    }

    int32 Processed = 0;
    
    while (Channel->TransportQueue.Num() > 0 && Channel->CurrentTransportRate < Channel->MaxTransportRate)
    {
        FMembraneObject Obj = Channel->TransportQueue[0];
        Channel->TransportQueue.RemoveAt(0);
        
        TArray<FString> Symbols;
        Symbols.Add(Obj.Symbol);
        
        if (ExecuteSymport(ChannelID, Symbols))
        {
            Processed++;
            Channel->CurrentTransportRate += 1.0f;
        }
    }

    return Processed;
}

// SocialCognitionSystem.cpp
// Implementation of Social Cognition and Theory of Mind

#include "SocialCognitionSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Memory/MemorySystems.h"
#include "../Emotion/EmotionalSystem.h"
#include "Math/UnrealMathUtility.h"

USocialCognitionSystem::USocialCognitionSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USocialCognitionSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeSocialSystem();
}

void USocialCognitionSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateRelationshipTypes();
}

void USocialCognitionSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        MemoryComponent = Owner->FindComponentByClass<UMemorySystems>();
        EmotionalComponent = Owner->FindComponentByClass<UEmotionalSystem>();
    }
}

void USocialCognitionSystem::InitializeSocialSystem()
{
    KnownAgents.Empty();
    Relationships.Empty();
    Interactions.Empty();
}

// ========================================
// AGENT MODELING
// ========================================

FSocialAgent USocialCognitionSystem::RegisterAgent(const FString& Name)
{
    // Check if already exists
    for (const FSocialAgent& Agent : KnownAgents)
    {
        if (Agent.AgentName == Name)
        {
            return Agent;
        }
    }

    FSocialAgent Agent;
    Agent.AgentID = GenerateAgentID();
    Agent.AgentName = Name;
    Agent.Trustworthiness = 0.5f;
    Agent.Competence = 0.5f;
    Agent.Warmth = 0.5f;
    Agent.InteractionCount = 0;
    Agent.LastInteractionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    KnownAgents.Add(Agent);

    // Create relationship
    FSocialRelationship Rel;
    Rel.RelationshipID = GenerateRelationshipID();
    Rel.AgentID = Agent.AgentID;
    Rel.Type = ERelationshipType::Stranger;
    Rel.Affinity = 0.0f;
    Rel.Trust = 0.5f;
    Rel.Familiarity = 0.0f;
    Rel.Reciprocity = 0.5f;
    Rel.RelationshipStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    Relationships.Add(Rel);

    OnAgentDiscovered.Broadcast(Agent);

    return Agent;
}

FSocialAgent USocialCognitionSystem::GetAgent(const FString& AgentID) const
{
    int32 Index = FindAgentIndex(AgentID);
    return Index >= 0 ? KnownAgents[Index] : FSocialAgent();
}

TArray<FSocialAgent> USocialCognitionSystem::GetAllAgents() const
{
    return KnownAgents;
}

void USocialCognitionSystem::UpdateAgentTrait(const FString& AgentID, const FString& Trait, float Value)
{
    int32 Index = FindAgentIndex(AgentID);
    if (Index >= 0)
    {
        KnownAgents[Index].Traits.Add(Trait, FMath::Clamp(Value, 0.0f, 1.0f));
    }
}

FAgentMentalState USocialCognitionSystem::InferMentalState(const FString& AgentID, 
                                                           const TArray<FString>& ObservedBehaviors)
{
    FAgentMentalState State;
    State.Confidence = 0.3f;

    int32 Index = FindAgentIndex(AgentID);
    if (Index < 0)
    {
        return State;
    }

    // Simple inference based on observed behaviors
    for (const FString& Behavior : ObservedBehaviors)
    {
        FString LowerBehavior = Behavior.ToLower();

        // Infer beliefs
        if (LowerBehavior.Contains(TEXT("look")) || LowerBehavior.Contains(TEXT("observe")))
        {
            State.Beliefs.AddUnique(TEXT("Interested in environment"));
        }
        if (LowerBehavior.Contains(TEXT("speak")) || LowerBehavior.Contains(TEXT("talk")))
        {
            State.Beliefs.AddUnique(TEXT("Wants to communicate"));
        }

        // Infer desires
        if (LowerBehavior.Contains(TEXT("approach")))
        {
            State.Desires.AddUnique(TEXT("Wants interaction"));
        }
        if (LowerBehavior.Contains(TEXT("avoid")) || LowerBehavior.Contains(TEXT("leave")))
        {
            State.Desires.AddUnique(TEXT("Wants distance"));
        }

        // Infer intentions
        if (LowerBehavior.Contains(TEXT("help")))
        {
            State.Intentions.AddUnique(TEXT("To assist"));
        }
        if (LowerBehavior.Contains(TEXT("ask")))
        {
            State.Intentions.AddUnique(TEXT("To request information"));
        }

        // Estimate emotional state
        if (LowerBehavior.Contains(TEXT("smile")) || LowerBehavior.Contains(TEXT("laugh")))
        {
            State.EstimatedValence += 0.3f;
        }
        if (LowerBehavior.Contains(TEXT("frown")) || LowerBehavior.Contains(TEXT("angry")))
        {
            State.EstimatedValence -= 0.3f;
        }
    }

    State.EstimatedValence = FMath::Clamp(State.EstimatedValence, -1.0f, 1.0f);
    State.Confidence = FMath::Min(0.9f, 0.3f + ObservedBehaviors.Num() * 0.1f);

    // Update agent's mental state
    KnownAgents[Index].MentalState = State;

    OnMentalStateInferred.Broadcast(AgentID, State);

    return State;
}

// ========================================
// RELATIONSHIPS
// ========================================

FSocialRelationship USocialCognitionSystem::GetRelationship(const FString& AgentID) const
{
    int32 Index = FindRelationshipIndex(AgentID);
    return Index >= 0 ? Relationships[Index] : FSocialRelationship();
}

void USocialCognitionSystem::UpdateAffinity(const FString& AgentID, float Delta)
{
    int32 Index = FindRelationshipIndex(AgentID);
    if (Index >= 0)
    {
        Relationships[Index].Affinity = FMath::Clamp(Relationships[Index].Affinity + Delta, -1.0f, 1.0f);

        ERelationshipType NewType = ComputeRelationshipType(Relationships[Index]);
        if (NewType != Relationships[Index].Type)
        {
            Relationships[Index].Type = NewType;
            OnRelationshipChanged.Broadcast(AgentID, NewType);
        }
    }
}

void USocialCognitionSystem::UpdateTrust(const FString& AgentID, float Delta)
{
    int32 Index = FindRelationshipIndex(AgentID);
    if (Index >= 0)
    {
        Relationships[Index].Trust = FMath::Clamp(Relationships[Index].Trust + Delta, 0.0f, 1.0f);
    }

    int32 AgentIndex = FindAgentIndex(AgentID);
    if (AgentIndex >= 0)
    {
        KnownAgents[AgentIndex].Trustworthiness = FMath::Clamp(
            KnownAgents[AgentIndex].Trustworthiness + Delta * 0.5f, 0.0f, 1.0f);
    }
}

TArray<FSocialAgent> USocialCognitionSystem::GetAgentsByRelationship(ERelationshipType Type) const
{
    TArray<FSocialAgent> Result;
    for (const FSocialRelationship& Rel : Relationships)
    {
        if (Rel.Type == Type)
        {
            int32 Index = FindAgentIndex(Rel.AgentID);
            if (Index >= 0)
            {
                Result.Add(KnownAgents[Index]);
            }
        }
    }
    return Result;
}

// ========================================
// SOCIAL INTERACTIONS
// ========================================

FSocialInteraction USocialCognitionSystem::RecordInteraction(const FString& AgentID, const FString& Type, 
                                                              float Valence, const FString& Context)
{
    FSocialInteraction Interaction;
    Interaction.InteractionID = GenerateInteractionID();
    Interaction.AgentID = AgentID;
    Interaction.InteractionType = Type;
    Interaction.Valence = Valence;
    Interaction.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Interaction.Context = Context;

    Interactions.Add(Interaction);

    // Limit interactions history
    while (Interactions.Num() > 500)
    {
        Interactions.RemoveAt(0);
    }

    // Update agent
    int32 AgentIndex = FindAgentIndex(AgentID);
    if (AgentIndex >= 0)
    {
        KnownAgents[AgentIndex].InteractionCount++;
        KnownAgents[AgentIndex].LastInteractionTime = Interaction.Timestamp;
    }

    // Update relationship
    int32 RelIndex = FindRelationshipIndex(AgentID);
    if (RelIndex >= 0)
    {
        Relationships[RelIndex].Familiarity = FMath::Min(1.0f, Relationships[RelIndex].Familiarity + 0.05f);
        Relationships[RelIndex].SharedExperiences.Add(Context);

        // Affinity changes based on valence
        UpdateAffinity(AgentID, Valence * 0.1f);
    }

    OnSocialInteraction.Broadcast(Interaction);

    return Interaction;
}

TArray<FSocialInteraction> USocialCognitionSystem::GetRecentInteractions(int32 Count) const
{
    TArray<FSocialInteraction> Recent;
    int32 Start = FMath::Max(0, Interactions.Num() - Count);
    for (int32 i = Start; i < Interactions.Num(); ++i)
    {
        Recent.Add(Interactions[i]);
    }
    return Recent;
}

TArray<FSocialInteraction> USocialCognitionSystem::GetInteractionsWithAgent(const FString& AgentID) const
{
    TArray<FSocialInteraction> Result;
    for (const FSocialInteraction& Int : Interactions)
    {
        if (Int.AgentID == AgentID)
        {
            Result.Add(Int);
        }
    }
    return Result;
}

// ========================================
// THEORY OF MIND
// ========================================

FString USocialCognitionSystem::PredictAgentAction(const FString& AgentID, const FString& Situation) const
{
    int32 Index = FindAgentIndex(AgentID);
    if (Index < 0)
    {
        return TEXT("Unknown");
    }

    const FSocialAgent& Agent = KnownAgents[Index];

    // Simple prediction based on traits and mental state
    if (Agent.MentalState.Intentions.Num() > 0)
    {
        return Agent.MentalState.Intentions[0];
    }

    if (Agent.Warmth > 0.7f)
    {
        return TEXT("Cooperative action");
    }
    else if (Agent.Warmth < 0.3f)
    {
        return TEXT("Self-interested action");
    }

    return TEXT("Neutral action");
}

float USocialCognitionSystem::EstimateAgentEmotion(const FString& AgentID, const FString& Event) const
{
    int32 Index = FindAgentIndex(AgentID);
    if (Index < 0)
    {
        return 0.0f;
    }

    const FSocialAgent& Agent = KnownAgents[Index];
    float BaseValence = Agent.MentalState.EstimatedValence;

    FString LowerEvent = Event.ToLower();

    // Adjust based on event type
    if (LowerEvent.Contains(TEXT("success")) || LowerEvent.Contains(TEXT("win")) || 
        LowerEvent.Contains(TEXT("reward")))
    {
        return FMath::Clamp(BaseValence + 0.4f, -1.0f, 1.0f);
    }
    if (LowerEvent.Contains(TEXT("failure")) || LowerEvent.Contains(TEXT("loss")) || 
        LowerEvent.Contains(TEXT("punishment")))
    {
        return FMath::Clamp(BaseValence - 0.4f, -1.0f, 1.0f);
    }

    return BaseValence;
}

bool USocialCognitionSystem::DoesAgentBelieve(const FString& AgentID, const FString& Belief) const
{
    int32 Index = FindAgentIndex(AgentID);
    if (Index < 0)
    {
        return false;
    }

    return KnownAgents[Index].MentalState.Beliefs.Contains(Belief);
}

// ========================================
// INTERNAL METHODS
// ========================================

void USocialCognitionSystem::UpdateRelationshipTypes()
{
    for (FSocialRelationship& Rel : Relationships)
    {
        ERelationshipType NewType = ComputeRelationshipType(Rel);
        if (NewType != Rel.Type)
        {
            Rel.Type = NewType;
            OnRelationshipChanged.Broadcast(Rel.AgentID, NewType);
        }
    }
}

ERelationshipType USocialCognitionSystem::ComputeRelationshipType(const FSocialRelationship& Rel) const
{
    if (Rel.Affinity < -0.6f)
    {
        return ERelationshipType::Adversary;
    }
    if (Rel.Affinity < -0.3f)
    {
        return ERelationshipType::Rival;
    }
    if (Rel.Familiarity < 0.1f)
    {
        return ERelationshipType::Stranger;
    }
    if (Rel.Familiarity < 0.3f)
    {
        return ERelationshipType::Acquaintance;
    }
    if (Rel.Affinity > 0.6f && Rel.Trust > 0.7f)
    {
        return ERelationshipType::CloseFriend;
    }
    if (Rel.Affinity > 0.3f)
    {
        return ERelationshipType::Friend;
    }
    if (Rel.Trust > 0.7f)
    {
        return ERelationshipType::Ally;
    }

    return ERelationshipType::Acquaintance;
}

FString USocialCognitionSystem::GenerateAgentID()
{
    return FString::Printf(TEXT("AGENT_%d_%d"), ++AgentIDCounter, FMath::RandRange(1000, 9999));
}

FString USocialCognitionSystem::GenerateRelationshipID()
{
    return FString::Printf(TEXT("REL_%d_%d"), ++RelationshipIDCounter, FMath::RandRange(1000, 9999));
}

FString USocialCognitionSystem::GenerateInteractionID()
{
    return FString::Printf(TEXT("INT_%d_%d"), ++InteractionIDCounter, FMath::RandRange(1000, 9999));
}

int32 USocialCognitionSystem::FindAgentIndex(const FString& AgentID) const
{
    for (int32 i = 0; i < KnownAgents.Num(); ++i)
    {
        if (KnownAgents[i].AgentID == AgentID)
        {
            return i;
        }
    }
    return -1;
}

int32 USocialCognitionSystem::FindRelationshipIndex(const FString& AgentID) const
{
    for (int32 i = 0; i < Relationships.Num(); ++i)
    {
        if (Relationships[i].AgentID == AgentID)
        {
            return i;
        }
    }
    return -1;
}

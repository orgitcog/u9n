// Enhanced4ECognition.cpp
// Deep Tree Echo - Enhanced 4E Embodied Cognition Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "Enhanced4ECognition.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(Log4ECognition, Log, All);

// ========================================
// Constructor / Destructor
// ========================================

UEnhanced4ECognitionComponent::UEnhanced4ECognitionComponent()
    : ExplorationDrive(0.5f)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UEnhanced4ECognitionComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize body schema from skeleton
    if (AActor* Owner = GetOwner())
    {
        if (USkeletalMeshComponent* SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
        {
            // Initialize body schema with default values
            CurrentBodySchema.BodyExtent = FVector(100.0f, 50.0f, 180.0f);
            CurrentBodySchema.CenterOfMass = FVector(0.0f, 0.0f, 90.0f);
            CurrentBodySchema.MaxReachDistance = 100.0f;
            CurrentBodySchema.SchemaConfidence = 1.0f;
        }
    }

    // Initialize interoceptive state
    CurrentInteroceptiveState = FInteroceptiveState();

    UE_LOG(Log4ECognition, Log, TEXT("Enhanced 4E Cognition Component initialized"));
}

void UEnhanced4ECognitionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update all 4E subsystems
    UpdateBodySchemaConfidence(DeltaTime);
    UpdateInteroceptiveState(DeltaTime);
    UpdateEnvironmentalNiche(DeltaTime);
    UpdateExplorationDrive(DeltaTime);
}

// ========================================
// EMBODIED: Body Schema & Sensorimotor
// ========================================

void UEnhanced4ECognitionComponent::UpdateBodySchema(const TMap<FName, FTransform>& BoneTransforms)
{
    CurrentBodySchema.JointTransforms = BoneTransforms;

    // Compute body extent from joint positions
    FVector MinBound = FVector(FLT_MAX);
    FVector MaxBound = FVector(-FLT_MAX);

    for (const auto& Pair : BoneTransforms)
    {
        FVector Location = Pair.Value.GetLocation();
        MinBound = MinBound.ComponentMin(Location);
        MaxBound = MaxBound.ComponentMax(Location);
    }

    if (BoneTransforms.Num() > 0)
    {
        CurrentBodySchema.BodyExtent = MaxBound - MinBound;

        // Compute center of mass (simplified as geometric center)
        CurrentBodySchema.CenterOfMass = (MaxBound + MinBound) * 0.5f;

        // Update max reach distance based on arm length
        // This is a simplified calculation
        CurrentBodySchema.MaxReachDistance = CurrentBodySchema.BodyExtent.X * 0.8f;
    }

    // Boost schema confidence when updated
    CurrentBodySchema.SchemaConfidence = FMath::Min(1.0f, CurrentBodySchema.SchemaConfidence + 0.1f);
}

void UEnhanced4ECognitionComponent::LearnSensorimotorContingency(const FString& MotorCommand,
                                                                  const FString& SensoryFeedback)
{
    // Check if contingency already exists
    for (FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        if (Contingency.MotorCommand == MotorCommand)
        {
            // Update existing contingency
            if (Contingency.ExpectedSensoryOutcome == SensoryFeedback)
            {
                // Confirmed expectation - strengthen confidence
                Contingency.Confidence = FMath::Min(1.0f, Contingency.Confidence + 0.1f);
            }
            else
            {
                // Prediction error - adjust
                Contingency.ExpectedSensoryOutcome = SensoryFeedback;
                Contingency.Confidence *= 0.8f;  // Reduce confidence
            }
            Contingency.ExperienceCount++;
            return;
        }
    }

    // Create new contingency
    FSensorimotorContingency NewContingency;
    NewContingency.MotorCommand = MotorCommand;
    NewContingency.ExpectedSensoryOutcome = SensoryFeedback;
    NewContingency.Confidence = 0.5f;  // Initial confidence
    NewContingency.ExperienceCount = 1;

    LearnedContingencies.Add(NewContingency);

    UE_LOG(Log4ECognition, Verbose, TEXT("Learned new sensorimotor contingency: %s -> %s"),
           *MotorCommand, *SensoryFeedback);
}

FString UEnhanced4ECognitionComponent::PredictSensoryOutcome(const FString& MotorCommand)
{
    // Find matching contingency
    for (const FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        if (Contingency.MotorCommand == MotorCommand)
        {
            // Add to pending predictions for validation
            PendingPredictions.Add(Contingency.ExpectedSensoryOutcome);
            return Contingency.ExpectedSensoryOutcome;
        }
    }

    // No learned contingency - return empty (uncertainty)
    return FString();
}

float UEnhanced4ECognitionComponent::CalculatePredictionError(const FString& Predicted, const FString& Actual)
{
    if (Predicted.IsEmpty() || Actual.IsEmpty())
    {
        return 1.0f;  // Maximum error if no prediction
    }

    if (Predicted == Actual)
    {
        return 0.0f;  // No error
    }

    // Calculate similarity (simple approach)
    int32 CommonChars = 0;
    int32 MaxLen = FMath::Max(Predicted.Len(), Actual.Len());

    for (int32 i = 0; i < FMath::Min(Predicted.Len(), Actual.Len()); i++)
    {
        if (Predicted[i] == Actual[i])
        {
            CommonChars++;
        }
    }

    float Similarity = MaxLen > 0 ? static_cast<float>(CommonChars) / MaxLen : 0.0f;
    return 1.0f - Similarity;
}

void UEnhanced4ECognitionComponent::AssociateSomaticMarker(const FString& BodyState,
                                                           const FEmotionalState& Emotion)
{
    // Check for existing marker
    for (FSomaticMarker& Marker : SomaticMarkerMemory)
    {
        if (Marker.BodyStateSignature == BodyState)
        {
            // Update existing marker
            Marker.AssociatedEmotion = Emotion;
            Marker.Strength = FMath::Min(1.0f, Marker.Strength + 0.1f);
            Marker.ActivationCount++;
            return;
        }
    }

    // Create new somatic marker
    FSomaticMarker NewMarker;
    NewMarker.BodyStateSignature = BodyState;
    NewMarker.AssociatedEmotion = Emotion;
    NewMarker.Strength = 0.5f;
    NewMarker.ActivationCount = 1;

    SomaticMarkerMemory.Add(NewMarker);

    UE_LOG(Log4ECognition, Verbose, TEXT("Created new somatic marker for body state: %s"), *BodyState);
}

bool UEnhanced4ECognitionComponent::RecallEmotionFromBodyState(const FString& BodyState,
                                                                FEmotionalState& OutEmotion)
{
    // Find matching somatic marker
    for (FSomaticMarker& Marker : SomaticMarkerMemory)
    {
        if (Marker.BodyStateSignature == BodyState)
        {
            OutEmotion = Marker.AssociatedEmotion;
            Marker.ActivationCount++;
            return true;
        }
    }

    return false;
}

void UEnhanced4ECognitionComponent::UpdateInteroceptiveState(float DeltaTime)
{
    // Gradual changes to interoceptive state based on activity

    // Fatigue increases slowly over time
    CurrentInteroceptiveState.Fatigue = FMath::Min(1.0f,
        CurrentInteroceptiveState.Fatigue + DeltaTime * 0.001f);

    // Arousal tends toward baseline
    float ArousalBaseline = 0.5f;
    CurrentInteroceptiveState.Arousal = FMath::FInterpTo(
        CurrentInteroceptiveState.Arousal, ArousalBaseline, DeltaTime, 0.1f);

    // Heart rate adjusts based on arousal
    float TargetHeartRate = 60.0f + (CurrentInteroceptiveState.Arousal * 60.0f);
    CurrentInteroceptiveState.HeartRate = FMath::FInterpTo(
        CurrentInteroceptiveState.HeartRate, TargetHeartRate, DeltaTime, 0.5f);

    // Breathing rate adjusts based on arousal
    float TargetBreathingRate = 12.0f + (CurrentInteroceptiveState.Arousal * 8.0f);
    CurrentInteroceptiveState.BreathingRate = FMath::FInterpTo(
        CurrentInteroceptiveState.BreathingRate, TargetBreathingRate, DeltaTime, 0.5f);
}

void UEnhanced4ECognitionComponent::UpdateBodySchemaConfidence(float DeltaTime)
{
    // Body schema confidence decays slowly if not updated
    CurrentBodySchema.SchemaConfidence = FMath::Max(0.5f,
        CurrentBodySchema.SchemaConfidence - DeltaTime * 0.01f);
}

FString UEnhanced4ECognitionComponent::GenerateBodyStateSignature()
{
    // Generate a signature string representing current body state
    // This is used for somatic marker association

    FString Signature = TEXT("BodyState_");

    // Add interoceptive components
    Signature += FString::Printf(TEXT("A%.1f_"), CurrentInteroceptiveState.Arousal);
    Signature += FString::Printf(TEXT("F%.1f_"), CurrentInteroceptiveState.Fatigue);
    Signature += FString::Printf(TEXT("HR%.0f"), CurrentInteroceptiveState.HeartRate);

    return Signature;
}

// ========================================
// EMBEDDED: Affordances & Environment
// ========================================

TArray<FAffordance> UEnhanced4ECognitionComponent::DetectAffordances(const TArray<AActor*>& NearbyObjects)
{
    TArray<FAffordance> Affordances;

    for (AActor* Object : NearbyObjects)
    {
        if (!Object)
        {
            continue;
        }

        FAffordance Affordance;
        Affordance.TargetObject = Object;
        Affordance.InteractionLocation = Object->GetActorLocation();
        Affordance.bCurrentlyAvailable = true;

        // Determine affordance type based on object properties
        // This is a simplified detection - real implementation would use object tags/interfaces

        FVector ObjectExtent = Object->GetComponentsBoundingBox().GetExtent();
        float ObjectSize = ObjectExtent.Size();

        // Small objects are graspable
        if (ObjectSize < 50.0f)
        {
            Affordance.AffordanceType = TEXT("Graspable");
            Affordance.Salience = 0.7f;
        }
        // Medium objects might be sittable
        else if (ObjectSize < 150.0f && ObjectExtent.Z < 100.0f)
        {
            Affordance.AffordanceType = TEXT("Sittable");
            Affordance.Salience = 0.5f;
        }
        // Large vertical objects might be climbable
        else if (ObjectExtent.Z > ObjectExtent.X * 2.0f)
        {
            Affordance.AffordanceType = TEXT("Climbable");
            Affordance.Salience = 0.4f;
        }
        else
        {
            Affordance.AffordanceType = TEXT("Observable");
            Affordance.Salience = 0.3f;
        }

        // Check feasibility based on reach
        float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Object->GetActorLocation());
        Affordance.Feasibility = FMath::Clamp(1.0f - (Distance / CurrentBodySchema.MaxReachDistance), 0.0f, 1.0f);

        Affordances.Add(Affordance);
    }

    DetectedAffordances = Affordances;
    return Affordances;
}

TArray<FAffordance> UEnhanced4ECognitionComponent::FilterAffordancesByCognitiveState(
    const TArray<FAffordance>& Affordances, const FCognitiveState& State)
{
    TArray<FAffordance> FilteredAffordances;

    for (const FAffordance& Affordance : Affordances)
    {
        // Calculate adjusted salience based on cognitive state
        float AdjustedSalience = CalculateAffordanceSalience(Affordance, State);

        // Only include affordances above salience threshold
        if (AdjustedSalience > 0.3f && CheckAffordanceFeasibility(Affordance))
        {
            FAffordance FilteredAffordance = Affordance;
            FilteredAffordance.Salience = AdjustedSalience;
            FilteredAffordances.Add(FilteredAffordance);
        }
    }

    // Sort by salience
    FilteredAffordances.Sort([](const FAffordance& A, const FAffordance& B)
    {
        return A.Salience > B.Salience;
    });

    return FilteredAffordances;
}

float UEnhanced4ECognitionComponent::CalculateAffordanceSalience(const FAffordance& Affordance,
                                                                  const FCognitiveState& State)
{
    float BaseSalience = Affordance.Salience;

    // Modify salience based on cognitive goals
    // This would check if the affordance matches current goals

    // Modify by feasibility
    BaseSalience *= Affordance.Feasibility;

    // Modify by interoceptive state
    // e.g., "Sittable" becomes more salient when fatigued
    if (Affordance.AffordanceType == TEXT("Sittable"))
    {
        BaseSalience *= (1.0f + CurrentInteroceptiveState.Fatigue * 0.5f);
    }

    return FMath::Clamp(BaseSalience, 0.0f, 1.0f);
}

bool UEnhanced4ECognitionComponent::CheckAffordanceFeasibility(const FAffordance& Affordance)
{
    if (Affordance.Feasibility < 0.1f)
    {
        return false;
    }

    // Check if body schema allows this action
    // e.g., can we reach the object?
    if (Affordance.TargetObject)
    {
        float Distance = FVector::Dist(
            GetOwner()->GetActorLocation(),
            Affordance.InteractionLocation
        );

        if (Distance > CurrentBodySchema.MaxReachDistance * 1.5f)
        {
            return false;  // Too far to interact
        }
    }

    return true;
}

bool UEnhanced4ECognitionComponent::ModifyEnvironmentForCognitiveSupport(const FString& ModificationType,
                                                                          AActor* TargetObject)
{
    if (!TargetObject)
    {
        return false;
    }

    // Niche construction - modifying environment to support cognition
    // This could involve moving objects, creating landmarks, etc.

    UE_LOG(Log4ECognition, Log, TEXT("Niche construction: %s on %s"),
           *ModificationType, *TargetObject->GetName());

    // Update niche with modification
    CurrentNiche.CognitiveSupport = FMath::Min(1.0f, CurrentNiche.CognitiveSupport + 0.1f);

    return true;
}

TArray<AActor*> UEnhanced4ECognitionComponent::DetectEnvironmentalScaffolding()
{
    TArray<AActor*> Scaffolding;

    // Find objects that can support cognition
    // Signs, landmarks, tools, etc.

    if (GetWorld())
    {
        // This would use spatial queries to find relevant objects
        // For now, return currently tracked scaffolding
        Scaffolding = EnvironmentalScaffolding;
    }

    return Scaffolding;
}

bool UEnhanced4ECognitionComponent::UtilizeScaffolding(AActor* Scaffolding)
{
    if (!Scaffolding)
    {
        return false;
    }

    // Add to active scaffolding if not already present
    if (!EnvironmentalScaffolding.Contains(Scaffolding))
    {
        EnvironmentalScaffolding.Add(Scaffolding);
    }

    // Update niche
    CurrentNiche.CognitiveSupport = FMath::Min(1.0f, CurrentNiche.CognitiveSupport + 0.1f);

    return true;
}

void UEnhanced4ECognitionComponent::UpdateEnvironmentalNiche(float DeltaTime)
{
    // Familiarity increases with time in environment
    CurrentNiche.Familiarity = FMath::Min(1.0f, CurrentNiche.Familiarity + DeltaTime * 0.01f);

    // Cognitive support decays without active scaffolding
    if (EnvironmentalScaffolding.Num() == 0)
    {
        CurrentNiche.CognitiveSupport = FMath::Max(0.0f,
            CurrentNiche.CognitiveSupport - DeltaTime * 0.005f);
    }

    // Update available affordances in niche
    CurrentNiche.AvailableAffordances = DetectedAffordances;
}

// ========================================
// ENACTED: Active Sampling & World-Making
// ========================================

void UEnhanced4ECognitionComponent::InitiateExploratorySampling(float UncertaintyLevel)
{
    ExplorationDrive = UncertaintyLevel;

    UE_LOG(Log4ECognition, Verbose, TEXT("Initiated exploratory sampling with uncertainty: %.2f"),
           UncertaintyLevel);
}

FString UEnhanced4ECognitionComponent::GenerateSamplingAction()
{
    // Generate action to reduce uncertainty
    // Higher exploration drive -> more exploratory actions

    TArray<FString> PossibleActions = {
        TEXT("LookAround"),
        TEXT("MoveForward"),
        TEXT("Reach"),
        TEXT("Touch"),
        TEXT("Turn")
    };

    if (ExplorationDrive > 0.7f)
    {
        // High uncertainty - explore actively
        return PossibleActions[FMath::RandRange(0, PossibleActions.Num() - 1)];
    }
    else if (ExplorationDrive > 0.4f)
    {
        // Medium uncertainty - look around
        return TEXT("LookAround");
    }
    else
    {
        // Low uncertainty - minimal action
        return TEXT("Observe");
    }
}

void UEnhanced4ECognitionComponent::ValidateSensorimotorPrediction(const FString& Prediction,
                                                                    const FString& Actual)
{
    float Error = CalculatePredictionError(Prediction, Actual);

    // Remove from pending predictions
    PendingPredictions.Remove(Prediction);

    // Update world model based on error
    UpdateWorldModelFromValidation(Error);

    // Learn from the experience
    LearnFromPredictionError(Error);
}

void UEnhanced4ECognitionComponent::UpdateWorldModelFromValidation(float PredictionError)
{
    // High prediction error -> increase exploration drive
    if (PredictionError > 0.5f)
    {
        ExplorationDrive = FMath::Min(1.0f, ExplorationDrive + 0.1f);
    }
    else if (PredictionError < 0.2f)
    {
        // Low error -> decrease exploration drive
        ExplorationDrive = FMath::Max(0.0f, ExplorationDrive - 0.05f);
    }
}

void UEnhanced4ECognitionComponent::LearnFromPredictionError(float Error)
{
    // Update sensorimotor contingencies based on prediction error
    // High error indicates model mismatch

    if (Error > 0.5f)
    {
        // Significant error - decrease confidence in related contingencies
        for (FSensorimotorContingency& Contingency : LearnedContingencies)
        {
            Contingency.Confidence *= 0.95f;
        }
    }
}

FString UEnhanced4ECognitionComponent::EnactWorldModel()
{
    // Active enactment of world model through sensorimotor loop
    // Generate description of enacted world

    FString EnactedModel = TEXT("Enacted world model: ");

    // Add body schema state
    EnactedModel += FString::Printf(TEXT("Body extent (%.0f, %.0f, %.0f), "),
        CurrentBodySchema.BodyExtent.X,
        CurrentBodySchema.BodyExtent.Y,
        CurrentBodySchema.BodyExtent.Z);

    // Add niche description
    EnactedModel += FString::Printf(TEXT("Niche familiarity %.2f, "),
        CurrentNiche.Familiarity);

    // Add affordance count
    EnactedModel += FString::Printf(TEXT("%d affordances detected"),
        DetectedAffordances.Num());

    EnactedWorldModelDescription = EnactedModel;
    return EnactedModel;
}

void UEnhanced4ECognitionComponent::CoCreateMeaningWithEnvironment(const FString& InteractionContext)
{
    // Participatory sense-making with environment
    // Meaning emerges from interaction, not just representation

    UE_LOG(Log4ECognition, Verbose, TEXT("Co-creating meaning in context: %s"), *InteractionContext);

    // Update niche based on interaction
    CurrentNiche.Familiarity = FMath::Min(1.0f, CurrentNiche.Familiarity + 0.05f);
}

void UEnhanced4ECognitionComponent::EngageParticipatoryKnowing(const FString& Phenomenon)
{
    // Engage with phenomenon through active participation
    // Knowledge through doing, not just observing

    UE_LOG(Log4ECognition, Verbose, TEXT("Engaging participatory knowing with: %s"), *Phenomenon);

    // Generate exploratory action
    FString Action = GenerateSamplingAction();

    // This would trigger actual motor action in a full implementation
}

void UEnhanced4ECognitionComponent::UpdateExplorationDrive(float DeltaTime)
{
    // Exploration drive naturally decays toward baseline
    float Baseline = 0.3f;
    ExplorationDrive = FMath::FInterpTo(ExplorationDrive, Baseline, DeltaTime, 0.1f);

    // Pending predictions increase exploration drive
    ExplorationDrive += PendingPredictions.Num() * 0.05f;
    ExplorationDrive = FMath::Clamp(ExplorationDrive, 0.0f, 1.0f);
}

// ========================================
// EXTENDED: Tools & Social Cognition
// ========================================

void UEnhanced4ECognitionComponent::RegisterCognitiveTool(const FCognitiveTool& Tool)
{
    // Check if tool already registered
    for (FCognitiveTool& ExistingTool : RegisteredTools)
    {
        if (ExistingTool.ToolObject == Tool.ToolObject)
        {
            // Update existing registration
            ExistingTool = Tool;
            return;
        }
    }

    RegisteredTools.Add(Tool);

    UE_LOG(Log4ECognition, Log, TEXT("Registered cognitive tool: %s (%s)"),
           *Tool.ToolType, *Tool.CognitiveFunction);
}

bool UEnhanced4ECognitionComponent::ExtendCognitionThroughTool(const FCognitiveTool& Tool)
{
    // Find the tool in registered tools
    for (FCognitiveTool& RegisteredTool : RegisteredTools)
    {
        if (RegisteredTool.ToolObject == Tool.ToolObject)
        {
            RegisteredTool.bCurrentlyUsing = true;

            // Increase proficiency with use
            RegisteredTool.Proficiency = FMath::Min(1.0f, RegisteredTool.Proficiency + 0.01f);

            UE_LOG(Log4ECognition, Verbose, TEXT("Extended cognition through tool: %s"),
                   *RegisteredTool.ToolType);

            return true;
        }
    }

    return false;
}

void UEnhanced4ECognitionComponent::RegisterSocialAgent(const FSocialAgent& Agent)
{
    // Check if agent already in network
    for (FSocialAgent& ExistingAgent : SocialNetwork)
    {
        if (ExistingAgent.AgentActor == Agent.AgentActor)
        {
            // Update existing entry
            ExistingAgent = Agent;
            return;
        }
    }

    SocialNetwork.Add(Agent);

    UE_LOG(Log4ECognition, Log, TEXT("Registered social agent: %s"), *Agent.AgentName);
}

void UEnhanced4ECognitionComponent::EngageCollectiveIntelligence(const TArray<FSocialAgent>& Agents)
{
    // Engage with multiple agents for collective problem-solving

    for (const FSocialAgent& Agent : Agents)
    {
        // Access shared knowledge from each agent
        for (const FString& Knowledge : Agent.SharedKnowledge)
        {
            if (!SharedKnowledgePool.Contains(Knowledge))
            {
                SharedKnowledgePool.Add(Knowledge);
            }
        }
    }

    UE_LOG(Log4ECognition, Log, TEXT("Engaged collective intelligence with %d agents"),
           Agents.Num());
}

TArray<FString> UEnhanced4ECognitionComponent::AccessSharedKnowledge()
{
    // Compile knowledge from social network
    TArray<FString> AllKnowledge;

    for (const FSocialAgent& Agent : SocialNetwork)
    {
        for (const FString& Knowledge : Agent.SharedKnowledge)
        {
            if (!AllKnowledge.Contains(Knowledge))
            {
                AllKnowledge.Add(Knowledge);
            }
        }
    }

    // Add from shared pool
    for (const FString& Knowledge : SharedKnowledgePool)
    {
        if (!AllKnowledge.Contains(Knowledge))
        {
            AllKnowledge.Add(Knowledge);
        }
    }

    return AllKnowledge;
}

void UEnhanced4ECognitionComponent::InheritCulturalKnowledge(const FString& CulturalContext)
{
    // Inherit knowledge from cultural context
    // This represents learning from cultural transmission

    if (!CulturalKnowledgeBase.Contains(CulturalContext))
    {
        CulturalKnowledgeBase.Add(CulturalContext);
    }

    UE_LOG(Log4ECognition, Log, TEXT("Inherited cultural knowledge: %s"), *CulturalContext);
}

void UEnhanced4ECognitionComponent::ContributeToCulturalKnowledge(const FString& Contribution)
{
    // Contribute new knowledge to cultural pool
    // This extends cognition through cultural participation

    if (!CulturalKnowledgeBase.Contains(Contribution))
    {
        CulturalKnowledgeBase.Add(Contribution);
    }

    // Also add to shared pool for immediate access by social network
    if (!SharedKnowledgePool.Contains(Contribution))
    {
        SharedKnowledgePool.Add(Contribution);
    }

    UE_LOG(Log4ECognition, Log, TEXT("Contributed to cultural knowledge: %s"), *Contribution);
}

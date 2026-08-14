/**
 * 4E Embodied Cognition Component Implementation
 * 
 * Implements the four dimensions of embodied cognition for avatar-based
 * cognitive embodiment in the Deep Tree Echo system.
 */

#include "EmbodiedCognitionComponent.h"

UEmbodiedCognitionComponent::UEmbodiedCognitionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f; // 20 Hz update
}

void UEmbodiedCognitionComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize body schema with default values
    BodySchema.PeripersonalRadius = 150.0f;
    BodySchema.SchemaCoherence = 1.0f;

    // Initialize default body parts
    BodySchema.BodyPartPositions.Add(TEXT("Head"), FVector(0, 0, 170));
    BodySchema.BodyPartPositions.Add(TEXT("Torso"), FVector(0, 0, 100));
    BodySchema.BodyPartPositions.Add(TEXT("LeftHand"), FVector(-50, 0, 100));
    BodySchema.BodyPartPositions.Add(TEXT("RightHand"), FVector(50, 0, 100));
    BodySchema.BodyPartPositions.Add(TEXT("LeftFoot"), FVector(-20, 0, 0));
    BodySchema.BodyPartPositions.Add(TEXT("RightFoot"), FVector(20, 0, 0));

    // Set initial niche
    CurrentNiche = TEXT("Default");
    CurrentActionPerceptionState = TEXT("Idle");
}

void UEmbodiedCognitionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update embodied components
    if (bEnableEmbodied)
    {
        UpdateSomaticMarkerDecay(DeltaTime);
    }

    // Update embedded components
    if (bEnableEmbedded)
    {
        UpdateAffordanceSalience(DeltaTime);
    }

    // Update enacted components
    if (bEnableEnacted)
    {
        UpdateContingencyStrengths(DeltaTime);
    }

    // Update extended components
    if (bEnableExtended)
    {
        UpdateToolIntegration(DeltaTime);
    }
}

// ========================================
// EMBODIED IMPLEMENTATION
// ========================================

void UEmbodiedCognitionComponent::UpdateBodySchema(const TMap<FString, FVector>& BodyPartPositions, const TMap<FString, FRotator>& BodyPartOrientations)
{
    BodySchema.BodyPartPositions = BodyPartPositions;
    BodySchema.BodyPartOrientations = BodyPartOrientations;

    // Compute schema coherence based on expected vs actual positions
    // Higher coherence when body parts are in expected relative positions
    float TotalDeviation = 0.0f;
    int32 Count = 0;

    // Check head-torso relationship
    if (const FVector* HeadPos = BodyPartPositions.Find(TEXT("Head")))
    {
        if (const FVector* TorsoPos = BodyPartPositions.Find(TEXT("Torso")))
        {
            float ExpectedDist = 70.0f;
            float ActualDist = FVector::Dist(*HeadPos, *TorsoPos);
            TotalDeviation += FMath::Abs(ActualDist - ExpectedDist) / ExpectedDist;
            ++Count;
        }
    }

    if (Count > 0)
    {
        BodySchema.SchemaCoherence = FMath::Clamp(1.0f - (TotalDeviation / Count), 0.0f, 1.0f);
    }
}

void UEmbodiedCognitionComponent::AddSomaticMarker(const FString& Stimulus, const FString& BodyRegion, float Valence, float Arousal)
{
    FSomaticMarker Marker;
    Marker.MarkerID = GenerateMarkerID();
    Marker.AssociatedStimulus = Stimulus;
    Marker.BodyRegion = BodyRegion;
    Marker.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    Marker.Arousal = FMath::Clamp(Arousal, 0.0f, 1.0f);
    Marker.Strength = 1.0f;

    // Check if marker for this stimulus already exists
    for (FSomaticMarker& ExistingMarker : ActiveSomaticMarkers)
    {
        if (ExistingMarker.AssociatedStimulus == Stimulus)
        {
            // Update existing marker
            ExistingMarker.Valence = FMath::Lerp(ExistingMarker.Valence, Valence, 0.5f);
            ExistingMarker.Arousal = FMath::Lerp(ExistingMarker.Arousal, Arousal, 0.5f);
            ExistingMarker.Strength = FMath::Min(1.0f, ExistingMarker.Strength + 0.2f);
            return;
        }
    }

    ActiveSomaticMarkers.Add(Marker);
}

FSomaticMarker UEmbodiedCognitionComponent::GetSomaticMarkerForStimulus(const FString& Stimulus) const
{
    for (const FSomaticMarker& Marker : ActiveSomaticMarkers)
    {
        if (Marker.AssociatedStimulus == Stimulus)
        {
            return Marker;
        }
    }
    return FSomaticMarker();
}

bool UEmbodiedCognitionComponent::IsInPeripersonalSpace(const FVector& Position) const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    FVector OwnerLocation = Owner->GetActorLocation();
    float Distance = FVector::Dist(OwnerLocation, Position);
    return Distance <= BodySchema.PeripersonalRadius;
}

// ========================================
// EMBEDDED IMPLEMENTATION
// ========================================

void UEmbodiedCognitionComponent::DetectAffordances(const TArray<AActor*>& NearbyActors)
{
    DetectedAffordances.Empty();

    for (AActor* Actor : NearbyActors)
    {
        if (!Actor)
        {
            continue;
        }

        FVector ActorLocation = Actor->GetActorLocation();
        
        // Check if in peripersonal space
        if (!IsInPeripersonalSpace(ActorLocation))
        {
            continue;
        }

        // Detect affordances based on actor properties
        FString ActorName = Actor->GetName();
        
        // Simple heuristic affordance detection
        FEnvironmentalAffordance Affordance;
        Affordance.AffordanceID = GenerateAffordanceID();
        Affordance.Provider = ActorName;
        Affordance.Location = ActorLocation;
        
        // Determine affordance type from actor tags or name
        if (Actor->ActorHasTag(TEXT("Graspable")) || ActorName.Contains(TEXT("Item")))
        {
            Affordance.AffordanceType = TEXT("Graspable");
            Affordance.RequiredCapability = TEXT("Hand");
        }
        else if (Actor->ActorHasTag(TEXT("Sittable")) || ActorName.Contains(TEXT("Chair")))
        {
            Affordance.AffordanceType = TEXT("Sittable");
            Affordance.RequiredCapability = TEXT("Legs");
        }
        else if (Actor->ActorHasTag(TEXT("Walkable")) || ActorName.Contains(TEXT("Floor")))
        {
            Affordance.AffordanceType = TEXT("Walkable");
            Affordance.RequiredCapability = TEXT("Legs");
        }
        else
        {
            Affordance.AffordanceType = TEXT("Observable");
            Affordance.RequiredCapability = TEXT("Eyes");
        }

        // Compute salience based on distance
        AActor* Owner = GetOwner();
        if (Owner)
        {
            float Distance = FVector::Dist(Owner->GetActorLocation(), ActorLocation);
            Affordance.Salience = 1.0f - (Distance / BodySchema.PeripersonalRadius);
            Affordance.Salience = FMath::Clamp(Affordance.Salience, 0.0f, 1.0f);
        }

        Affordance.Accessibility = 1.0f;

        DetectedAffordances.Add(Affordance);
    }
}

void UEmbodiedCognitionComponent::AddAffordance(const FString& Provider, const FString& Type, const FVector& Location, float Salience)
{
    FEnvironmentalAffordance Affordance;
    Affordance.AffordanceID = GenerateAffordanceID();
    Affordance.Provider = Provider;
    Affordance.AffordanceType = Type;
    Affordance.Location = Location;
    Affordance.Salience = FMath::Clamp(Salience, 0.0f, 1.0f);
    Affordance.Accessibility = 1.0f;

    DetectedAffordances.Add(Affordance);
}

FEnvironmentalAffordance UEmbodiedCognitionComponent::GetMostSalientAffordance() const
{
    FEnvironmentalAffordance MostSalient;
    float HighestSalience = -1.0f;

    for (const FEnvironmentalAffordance& Affordance : DetectedAffordances)
    {
        if (Affordance.Salience > HighestSalience)
        {
            HighestSalience = Affordance.Salience;
            MostSalient = Affordance;
        }
    }

    return MostSalient;
}

void UEmbodiedCognitionComponent::SetCurrentNiche(const FString& Niche)
{
    CurrentNiche = Niche;
}

// ========================================
// ENACTED IMPLEMENTATION
// ========================================

void UEmbodiedCognitionComponent::LearnContingency(const FString& Action, const FString& ExpectedOutcome, const FString& ActualOutcome)
{
    // Check if contingency already exists
    for (FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        if (Contingency.TriggeringAction == Action)
        {
            // Update existing contingency
            float Error = (ExpectedOutcome == ActualOutcome) ? 0.0f : 1.0f;
            Contingency.PredictionError = Error;
            LastPredictionError = Error;

            // Update expected outcome based on learning
            if (Error > 0.0f)
            {
                Contingency.ExpectedOutcome = ActualOutcome;
                Contingency.Strength = FMath::Max(0.1f, Contingency.Strength - SensorimotorLearningRate);
            }
            else
            {
                Contingency.Strength = FMath::Min(1.0f, Contingency.Strength + SensorimotorLearningRate);
            }

            Contingency.ActualOutcome = ActualOutcome;
            return;
        }
    }

    // Create new contingency
    FSensorimotorContingency NewContingency;
    NewContingency.ContingencyID = GenerateContingencyID();
    NewContingency.TriggeringAction = Action;
    NewContingency.ExpectedOutcome = ExpectedOutcome;
    NewContingency.ActualOutcome = ActualOutcome;
    NewContingency.PredictionError = (ExpectedOutcome == ActualOutcome) ? 0.0f : 1.0f;
    NewContingency.Strength = 0.5f;

    LastPredictionError = NewContingency.PredictionError;

    LearnedContingencies.Add(NewContingency);
}

FString UEmbodiedCognitionComponent::PredictOutcome(const FString& Action) const
{
    for (const FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        if (Contingency.TriggeringAction == Action)
        {
            return Contingency.ExpectedOutcome;
        }
    }
    return TEXT("Unknown");
}

float UEmbodiedCognitionComponent::GetLastPredictionError() const
{
    return LastPredictionError;
}

void UEmbodiedCognitionComponent::UpdateActionPerceptionState(const FString& State)
{
    CurrentActionPerceptionState = State;
}

// ========================================
// EXTENDED IMPLEMENTATION
// ========================================

void UEmbodiedCognitionComponent::RegisterCognitiveTool(const FString& ToolName, const FString& ToolType, const FString& ExtendedFunction)
{
    FCognitiveTool Tool;
    Tool.ToolID = GenerateToolID();
    Tool.ToolName = ToolName;
    Tool.ToolType = ToolType;
    Tool.ExtendedFunction = ExtendedFunction;
    Tool.IntegrationLevel = 0.0f;
    Tool.bIsActive = false;

    AvailableTools.Add(Tool);
}

void UEmbodiedCognitionComponent::ActivateTool(const FString& ToolID)
{
    for (FCognitiveTool& Tool : AvailableTools)
    {
        if (Tool.ToolID == ToolID)
        {
            Tool.bIsActive = true;
            break;
        }
    }
}

void UEmbodiedCognitionComponent::DeactivateTool(const FString& ToolID)
{
    for (FCognitiveTool& Tool : AvailableTools)
    {
        if (Tool.ToolID == ToolID)
        {
            Tool.bIsActive = false;
            break;
        }
    }
}

void UEmbodiedCognitionComponent::AddExternalMemoryRef(const FString& Reference)
{
    ExternalMemoryRefs.AddUnique(Reference);
}

float UEmbodiedCognitionComponent::GetToolIntegrationLevel(const FString& ToolID) const
{
    for (const FCognitiveTool& Tool : AvailableTools)
    {
        if (Tool.ToolID == ToolID)
        {
            return Tool.IntegrationLevel;
        }
    }
    return 0.0f;
}

// ========================================
// INTEGRATION IMPLEMENTATION
// ========================================

float UEmbodiedCognitionComponent::Get4EIntegrationScore() const
{
    return (GetEmbodiedScore() + GetEmbeddedScore() + GetEnactedScore() + GetExtendedScore()) / 4.0f;
}

float UEmbodiedCognitionComponent::GetEmbodiedScore() const
{
    if (!bEnableEmbodied)
    {
        return 0.0f;
    }

    // Based on body schema coherence and somatic marker activity
    float MarkerActivity = ActiveSomaticMarkers.Num() > 0 ? 
        FMath::Min(1.0f, ActiveSomaticMarkers.Num() / 5.0f) : 0.0f;
    
    return (BodySchema.SchemaCoherence + MarkerActivity) / 2.0f;
}

float UEmbodiedCognitionComponent::GetEmbeddedScore() const
{
    if (!bEnableEmbedded)
    {
        return 0.0f;
    }

    // Based on affordance detection and niche engagement
    float AffordanceScore = DetectedAffordances.Num() > 0 ?
        FMath::Min(1.0f, DetectedAffordances.Num() / 10.0f) : 0.0f;
    
    float NicheScore = !CurrentNiche.IsEmpty() && CurrentNiche != TEXT("Default") ? 1.0f : 0.5f;
    
    return (AffordanceScore + NicheScore) / 2.0f;
}

float UEmbodiedCognitionComponent::GetEnactedScore() const
{
    if (!bEnableEnacted)
    {
        return 0.0f;
    }

    // Based on learned contingencies and prediction accuracy
    if (LearnedContingencies.Num() == 0)
    {
        return 0.0f;
    }

    float TotalStrength = 0.0f;
    for (const FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        TotalStrength += Contingency.Strength;
    }
    
    return TotalStrength / LearnedContingencies.Num();
}

float UEmbodiedCognitionComponent::GetExtendedScore() const
{
    if (!bEnableExtended)
    {
        return 0.0f;
    }

    // Based on tool integration and external memory
    float ToolScore = 0.0f;
    int32 ActiveToolCount = 0;
    
    for (const FCognitiveTool& Tool : AvailableTools)
    {
        if (Tool.bIsActive)
        {
            ToolScore += Tool.IntegrationLevel;
            ++ActiveToolCount;
        }
    }
    
    if (ActiveToolCount > 0)
    {
        ToolScore /= ActiveToolCount;
    }
    
    float MemoryScore = ExternalMemoryRefs.Num() > 0 ?
        FMath::Min(1.0f, ExternalMemoryRefs.Num() / 5.0f) : 0.0f;
    
    return (ToolScore + MemoryScore) / 2.0f;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UEmbodiedCognitionComponent::UpdateSomaticMarkerDecay(float DeltaTime)
{
    const float DecayRate = 0.1f;
    
    for (int32 i = ActiveSomaticMarkers.Num() - 1; i >= 0; --i)
    {
        ActiveSomaticMarkers[i].Strength -= DecayRate * DeltaTime;
        
        if (ActiveSomaticMarkers[i].Strength <= 0.0f)
        {
            ActiveSomaticMarkers.RemoveAt(i);
        }
    }
}

void UEmbodiedCognitionComponent::UpdateAffordanceSalience(float DeltaTime)
{
    const float DecayRate = 0.05f;
    
    for (int32 i = DetectedAffordances.Num() - 1; i >= 0; --i)
    {
        DetectedAffordances[i].Salience -= DecayRate * DeltaTime;
        
        if (DetectedAffordances[i].Salience <= 0.0f)
        {
            DetectedAffordances.RemoveAt(i);
        }
    }
}

void UEmbodiedCognitionComponent::UpdateContingencyStrengths(float DeltaTime)
{
    const float DecayRate = 0.01f;
    
    for (FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        // Slow decay of unused contingencies
        Contingency.Strength = FMath::Max(0.1f, Contingency.Strength - DecayRate * DeltaTime);
    }
}

void UEmbodiedCognitionComponent::UpdateToolIntegration(float DeltaTime)
{
    const float IntegrationRate = 0.1f;
    const float DecayRate = 0.05f;
    
    for (FCognitiveTool& Tool : AvailableTools)
    {
        if (Tool.bIsActive)
        {
            // Active tools become more integrated
            Tool.IntegrationLevel = FMath::Min(1.0f, Tool.IntegrationLevel + IntegrationRate * DeltaTime);
        }
        else
        {
            // Inactive tools slowly lose integration
            Tool.IntegrationLevel = FMath::Max(0.0f, Tool.IntegrationLevel - DecayRate * DeltaTime);
        }
    }
}

FString UEmbodiedCognitionComponent::GenerateContingencyID()
{
    return FString::Printf(TEXT("Contingency_%d"), ++ContingencyIDCounter);
}

FString UEmbodiedCognitionComponent::GenerateToolID()
{
    return FString::Printf(TEXT("Tool_%d"), ++ToolIDCounter);
}

FString UEmbodiedCognitionComponent::GenerateMarkerID()
{
    return FString::Printf(TEXT("Marker_%d"), ++MarkerIDCounter);
}

FString UEmbodiedCognitionComponent::GenerateAffordanceID()
{
    return FString::Printf(TEXT("Affordance_%d"), ++AffordanceIDCounter);
}

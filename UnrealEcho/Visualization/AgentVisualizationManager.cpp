// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Agent Visualization Manager Implementation

#include "AgentVisualizationManager.h"
#include "AgentAvatarActor.h"
#include "SharedVisualizationSpace.h"
#include "../9P/Avatar9PServer.h"
#include "../Rendering/DeepTreeEchoRenderingSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UAgentVisualizationManager::UAgentVisualizationManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Every frame for smooth visuals

    // Default configuration
    StateUpdateInterval = 0.1f; // 100ms for state sync
    TargetFPS = 60;
    MaxAgentCount = 10;
    bEnablePerformanceOptimization = true;
    GlobalAuraIntensityMultiplier = 1.0f;
    bVerboseLogging = false;

    // Initialize timers
    StateUpdateTimer = 0.0f;
    PerformanceCheckTimer = 0.0f;
    LODUpdateTimer = 0.0f;
}

void UAgentVisualizationManager::BeginPlay()
{
    Super::BeginPlay();

    // Get component references
    Server9P = GetOwner()->FindComponentByClass<UAvatar9PServer>();
    RenderingSubsystem = UDeepTreeEchoRenderingSubsystem::Get(GetWorld());

    // Create shared visualization space
    SharedSpace = NewObject<USharedVisualizationSpace>(this);
    if (SharedSpace)
    {
        SharedSpace->RegisterComponent();
    }

    // Initialize default visual styles
    InitializeDefaultStyles();

    UE_LOG(LogTemp, Log, TEXT("AgentVisualizationManager initialized with target %d FPS, max %d agents"),
           TargetFPS, MaxAgentCount);
}

void UAgentVisualizationManager::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update state from 9P at configured interval
    StateUpdateTimer += DeltaTime;
    if (StateUpdateTimer >= StateUpdateInterval)
    {
        StateUpdateTimer = 0.0f;
        SyncAllAgentStates();
    }

    // Update communication links (visual effects)
    UpdateCommunicationLinks(DeltaTime);

    // Update performance metrics
    PerformanceCheckTimer += DeltaTime;
    if (PerformanceCheckTimer >= 0.5f) // Every 500ms
    {
        PerformanceCheckTimer = 0.0f;
        UpdatePerformanceMetrics(DeltaTime);

        // Optimize if needed
        if (bEnablePerformanceOptimization)
        {
            OptimizeForCurrentLoad();
        }
    }

    // Update LODs
    LODUpdateTimer += DeltaTime;
    if (LODUpdateTimer >= 0.25f) // Every 250ms
    {
        LODUpdateTimer = 0.0f;
        UpdateAgentLODs();
    }
}

// ===== Agent Visualization Creation =====

AAgentAvatarActor* UAgentVisualizationManager::CreateAgentVisualization(const FString& AgentPath,
                                                                         const FAgentVisualStyle& Style)
{
    // Check if already visualized
    if (VisualizedAgents.Contains(AgentPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Agent %s is already visualized"), *AgentPath);
        return VisualizedAgents[AgentPath];
    }

    // Check max agent limit
    if (VisualizedAgents.Num() >= MaxAgentCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot create visualization: max agent count (%d) reached"), MaxAgentCount);
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Spawn avatar actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // Calculate spawn position (arrange in circle if multiple agents)
    FVector SpawnLocation = FVector::ZeroVector;
    int32 AgentIndex = VisualizedAgents.Num();
    float Angle = (2.0f * PI * AgentIndex) / FMath::Max(1, MaxAgentCount);
    SpawnLocation.X = FMath::Cos(Angle) * 300.0f;
    SpawnLocation.Y = FMath::Sin(Angle) * 300.0f;

    AAgentAvatarActor* Avatar = World->SpawnActor<AAgentAvatarActor>(
        AAgentAvatarActor::StaticClass(),
        SpawnLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (Avatar)
    {
        // Configure avatar with style
        Avatar->SetAgentPath(AgentPath);
        Avatar->SetVisualStyle(Style);

        // Store reference
        VisualizedAgents.Add(AgentPath, Avatar);

        // Initialize state
        FAgentVisualizationState InitialState;
        InitialState.AgentPath = AgentPath;
        InitialState.AgentName = FPaths::GetBaseFilename(AgentPath);
        InitialState.AgentType = DetermineAgentType(AgentPath);
        InitialState.WorldPosition = SpawnLocation;
        InitialState.AuraColor = Style.DefaultAuraColor;
        InitialState.bIsOnline = true;
        AgentStates.Add(AgentPath, InitialState);

        // Broadcast event
        OnAgentVisualized.Broadcast(AgentPath, Avatar);

        if (bVerboseLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("Created visualization for agent: %s"), *AgentPath);
        }
    }

    return Avatar;
}

AAgentAvatarActor* UAgentVisualizationManager::CreateAgentVisualizationAuto(const FString& AgentPath)
{
    FString AgentType = DetermineAgentType(AgentPath);
    FAgentVisualStyle Style = GetVisualStyleForType(AgentType);
    return CreateAgentVisualization(AgentPath, Style);
}

void UAgentVisualizationManager::RemoveAgentVisualization(const FString& AgentPath)
{
    if (AAgentAvatarActor** AvatarPtr = VisualizedAgents.Find(AgentPath))
    {
        if (*AvatarPtr)
        {
            (*AvatarPtr)->Destroy();
        }
        VisualizedAgents.Remove(AgentPath);
        AgentStates.Remove(AgentPath);

        // Remove associated communication links
        CommunicationLinks.RemoveAll([&AgentPath](const FCommunicationLink& Link) {
            return Link.SourceAgentPath == AgentPath || Link.TargetAgentPath == AgentPath;
        });

        OnAgentRemoved.Broadcast(AgentPath);

        if (bVerboseLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("Removed visualization for agent: %s"), *AgentPath);
        }
    }
}

void UAgentVisualizationManager::RemoveAllAgentVisualizations()
{
    TArray<FString> PathsToRemove;
    VisualizedAgents.GetKeys(PathsToRemove);

    for (const FString& Path : PathsToRemove)
    {
        RemoveAgentVisualization(Path);
    }
}

// ===== State Synchronization =====

void UAgentVisualizationManager::UpdateAgentFromState(AAgentAvatarActor* Avatar, const FString& AgentPath)
{
    if (!Avatar)
    {
        return;
    }

    // Fetch state from 9P
    FAgentVisualizationState State = FetchStateFrom9P(AgentPath);

    // Apply state to avatar
    ApplyStateToAvatar(Avatar, State);

    // Update cached state
    AgentStates.Add(AgentPath, State);

    // Broadcast update
    OnAgentStateUpdated.Broadcast(AgentPath, State);
}

void UAgentVisualizationManager::SyncAllAgentStates()
{
    for (auto& Pair : VisualizedAgents)
    {
        if (Pair.Value)
        {
            UpdateAgentFromState(Pair.Value, Pair.Key);
        }
    }
}

FAgentVisualizationState UAgentVisualizationManager::GetAgentVisualizationState(const FString& AgentPath)
{
    if (FAgentVisualizationState* State = AgentStates.Find(AgentPath))
    {
        return *State;
    }
    return FAgentVisualizationState();
}

void UAgentVisualizationManager::SetStateUpdateInterval(float IntervalSeconds)
{
    StateUpdateInterval = FMath::Clamp(IntervalSeconds, 0.016f, 1.0f);
}

FAgentVisualizationState UAgentVisualizationManager::FetchStateFrom9P(const FString& AgentPath)
{
    FAgentVisualizationState State;
    State.AgentPath = AgentPath;
    State.LastUpdate = FDateTime::Now();

    if (!Server9P)
    {
        return State;
    }

    // Read state files from 9P namespace
    FString StatePath = AgentPath + TEXT("/state");

    // Read attention level
    FString AttentionData = Server9P->ReadFile(StatePath + TEXT("/attention"));
    if (!AttentionData.IsEmpty())
    {
        State.AttentionLevel = FCString::Atof(*AttentionData);
    }

    // Read activity level
    FString ActivityData = Server9P->ReadFile(StatePath + TEXT("/activity"));
    if (!ActivityData.IsEmpty())
    {
        State.ActivityLevel = FCString::Atof(*ActivityData);
    }

    // Read emotional state
    FString EmotionData = Server9P->ReadFile(StatePath + TEXT("/emotions"));
    if (!EmotionData.IsEmpty())
    {
        // Parse emotion JSON
        TArray<FString> Emotions;
        EmotionData.ParseIntoArray(Emotions, TEXT(","));
        for (const FString& EmotionPair : Emotions)
        {
            FString Name, Value;
            if (EmotionPair.Split(TEXT(":"), &Name, &Value))
            {
                State.EmotionalState.Add(Name.TrimStartAndEnd(), FCString::Atof(*Value));
            }
        }
    }

    // Read online status
    FString OnlineData = Server9P->ReadFile(StatePath + TEXT("/online"));
    State.bIsOnline = OnlineData.ToBool();

    // Read capabilities
    FString CapabilitiesData = Server9P->ReadFile(AgentPath + TEXT("/capabilities"));
    if (!CapabilitiesData.IsEmpty())
    {
        CapabilitiesData.ParseIntoArray(State.ActiveCapabilities, TEXT(","));
    }

    // Determine aura color based on emotional state
    float Happiness = State.EmotionalState.FindRef(TEXT("happiness"));
    float Sadness = State.EmotionalState.FindRef(TEXT("sadness"));
    float Excitement = State.EmotionalState.FindRef(TEXT("excitement"));

    State.AuraColor = FLinearColor(
        0.2f + Excitement * 0.5f,
        0.3f + Happiness * 0.5f,
        0.8f - Sadness * 0.3f,
        1.0f
    );
    State.AuraIntensity = 0.5f + State.ActivityLevel * 0.5f;

    return State;
}

void UAgentVisualizationManager::ApplyStateToAvatar(AAgentAvatarActor* Avatar,
                                                     const FAgentVisualizationState& State)
{
    if (!Avatar)
    {
        return;
    }

    // Update visual properties
    Avatar->SetAuraColor(State.AuraColor);
    Avatar->SetAuraIntensity(State.AuraIntensity * GlobalAuraIntensityMultiplier);
    Avatar->SetAttentionLevel(State.AttentionLevel);
    Avatar->SetActivityLevel(State.ActivityLevel);
    Avatar->SetOnlineState(State.bIsOnline);

    // Update emotional expression
    Avatar->UpdateEmotionalExpression(State.EmotionalState);

    // Update position if changed
    if (!State.WorldPosition.IsNearlyZero())
    {
        Avatar->SetActorLocation(State.WorldPosition, true);
    }
}

// ===== Shared Visualization Space =====

void UAgentVisualizationManager::CreateSharedSpace(const TArray<FString>& AgentPaths)
{
    if (SharedSpace)
    {
        SharedSpace->Initialize(AgentPaths);

        // Create visualizations for all agents
        for (const FString& Path : AgentPaths)
        {
            if (!IsAgentVisualized(Path))
            {
                CreateAgentVisualizationAuto(Path);
            }
            SharedSpace->AddAgent(Path);
        }

        // Arrange agents in circle
        ArrangeAgentsInCircle(FVector::ZeroVector, 400.0f);
    }
}

void UAgentVisualizationManager::AddAgentToSharedSpace(const FString& AgentPath)
{
    if (SharedSpace)
    {
        SharedSpace->AddAgent(AgentPath);
    }
}

void UAgentVisualizationManager::RemoveAgentFromSharedSpace(const FString& AgentPath)
{
    if (SharedSpace)
    {
        SharedSpace->RemoveAgent(AgentPath);
    }
}

USharedVisualizationSpace* UAgentVisualizationManager::GetSharedSpace()
{
    return SharedSpace;
}

// ===== Communication Link Visualization =====

void UAgentVisualizationManager::CreateCommunicationLink(const FString& SourcePath,
                                                          const FString& TargetPath,
                                                          const FString& MessageType)
{
    // Check if link already exists
    for (FCommunicationLink& Link : CommunicationLinks)
    {
        if (Link.SourceAgentPath == SourcePath && Link.TargetAgentPath == TargetPath)
        {
            // Update existing link
            Link.MessageType = MessageType;
            Link.MessageCount++;
            Link.LastActivity = FDateTime::Now();
            return;
        }
    }

    // Create new link
    FCommunicationLink NewLink;
    NewLink.LinkId = FGuid::NewGuid().ToString();
    NewLink.SourceAgentPath = SourcePath;
    NewLink.TargetAgentPath = TargetPath;
    NewLink.MessageType = MessageType;
    NewLink.MessageCount = 1;
    NewLink.LastActivity = FDateTime::Now();

    // Set color based on message type
    if (MessageType == TEXT("query"))
    {
        NewLink.LinkColor = FLinearColor(0.3f, 0.7f, 1.0f, 0.8f); // Blue
    }
    else if (MessageType == TEXT("response"))
    {
        NewLink.LinkColor = FLinearColor(0.3f, 1.0f, 0.5f, 0.8f); // Green
    }
    else if (MessageType == TEXT("broadcast"))
    {
        NewLink.LinkColor = FLinearColor(1.0f, 0.8f, 0.3f, 0.8f); // Yellow
    }
    else
    {
        NewLink.LinkColor = FLinearColor(0.8f, 0.4f, 1.0f, 0.8f); // Purple
    }

    CommunicationLinks.Add(NewLink);
    OnCommunicationLinkCreated.Broadcast(NewLink);

    // Visualize in shared space
    if (SharedSpace)
    {
        SharedSpace->AddCommunicationLink(NewLink);
    }
}

void UAgentVisualizationManager::PulseCommunicationLink(const FString& SourcePath,
                                                         const FString& TargetPath,
                                                         float Intensity)
{
    for (FCommunicationLink& Link : CommunicationLinks)
    {
        if (Link.SourceAgentPath == SourcePath && Link.TargetAgentPath == TargetPath)
        {
            Link.ActivityPulse = FMath::Clamp(Intensity, 0.0f, 1.0f);
            Link.LastActivity = FDateTime::Now();
            break;
        }
    }
}

void UAgentVisualizationManager::RemoveCommunicationLink(const FString& SourcePath, const FString& TargetPath)
{
    CommunicationLinks.RemoveAll([&SourcePath, &TargetPath](const FCommunicationLink& Link) {
        return Link.SourceAgentPath == SourcePath && Link.TargetAgentPath == TargetPath;
    });

    if (SharedSpace)
    {
        SharedSpace->RemoveCommunicationLink(SourcePath, TargetPath);
    }
}

TArray<FCommunicationLink> UAgentVisualizationManager::GetActiveCommunicationLinks()
{
    return CommunicationLinks;
}

void UAgentVisualizationManager::UpdateCommunicationLinks(float DeltaTime)
{
    // Decay pulse intensity over time
    for (FCommunicationLink& Link : CommunicationLinks)
    {
        if (Link.ActivityPulse > 0.0f)
        {
            Link.ActivityPulse = FMath::FInterpTo(Link.ActivityPulse, 0.0f, DeltaTime, 3.0f);
        }
    }

    // Update visual representation in shared space
    if (SharedSpace)
    {
        SharedSpace->UpdateCommunicationLinks(CommunicationLinks);
    }
}

// ===== Agent Positioning =====

void UAgentVisualizationManager::ArrangeAgentsInCircle(FVector Center, float Radius)
{
    TArray<FString> Paths;
    VisualizedAgents.GetKeys(Paths);

    int32 Count = Paths.Num();
    if (Count == 0) return;

    for (int32 i = 0; i < Count; ++i)
    {
        float Angle = (2.0f * PI * i) / Count;
        FVector Position = Center;
        Position.X += FMath::Cos(Angle) * Radius;
        Position.Y += FMath::Sin(Angle) * Radius;

        SetAgentPosition(Paths[i], Position);
    }
}

void UAgentVisualizationManager::ArrangeAgentsInGrid(FVector Origin, float Spacing, int32 Columns)
{
    TArray<FString> Paths;
    VisualizedAgents.GetKeys(Paths);

    int32 Count = Paths.Num();
    if (Count == 0 || Columns <= 0) return;

    for (int32 i = 0; i < Count; ++i)
    {
        int32 Row = i / Columns;
        int32 Col = i % Columns;

        FVector Position = Origin;
        Position.X += Col * Spacing;
        Position.Y += Row * Spacing;

        SetAgentPosition(Paths[i], Position);
    }
}

void UAgentVisualizationManager::SetAgentPosition(const FString& AgentPath, FVector Position)
{
    if (AAgentAvatarActor** AvatarPtr = VisualizedAgents.Find(AgentPath))
    {
        if (*AvatarPtr)
        {
            (*AvatarPtr)->SetActorLocation(Position, true);

            // Update cached state
            if (FAgentVisualizationState* State = AgentStates.Find(AgentPath))
            {
                State->WorldPosition = Position;
            }
        }
    }
}

void UAgentVisualizationManager::FocusOnAgent(const FString& AgentPath, float TransitionTime)
{
    if (AAgentAvatarActor** AvatarPtr = VisualizedAgents.Find(AgentPath))
    {
        if (*AvatarPtr)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                FVector TargetLocation = (*AvatarPtr)->GetActorLocation();
                TargetLocation.Z += 100.0f; // Slight offset
                TargetLocation -= FVector(300.0f, 0.0f, 0.0f); // Camera distance

                // Set view target with blend
                PC->SetViewTargetWithBlend(*AvatarPtr, TransitionTime);
            }
        }
    }
}

// ===== Performance Optimization =====

FVisualizationPerformanceMetrics UAgentVisualizationManager::GetPerformanceMetrics()
{
    return CurrentMetrics;
}

void UAgentVisualizationManager::SetLODSettings(const FAgentLODSettings& Settings)
{
    LODSettings = Settings;
    UpdateAgentLODs();
}

void UAgentVisualizationManager::SetPerformanceOptimizationEnabled(bool bEnabled)
{
    bEnablePerformanceOptimization = bEnabled;
}

void UAgentVisualizationManager::SetTargetFPS(int32 NewTargetFPS)
{
    TargetFPS = FMath::Clamp(NewTargetFPS, 30, 144);
}

int32 UAgentVisualizationManager::GetMaxRecommendedAgentCount()
{
    // Estimate based on current performance
    if (CurrentMetrics.AverageFrameTime <= 0.0f)
    {
        return MaxAgentCount;
    }

    float TargetFrameTime = 1000.0f / TargetFPS;
    float FrameTimePerAgent = CurrentMetrics.AverageFrameTime / FMath::Max(1, CurrentMetrics.ActiveAgentCount);
    float AvailableTime = TargetFrameTime - 4.0f; // Reserve 4ms for other systems

    int32 Recommended = FMath::FloorToInt(AvailableTime / FrameTimePerAgent);
    return FMath::Clamp(Recommended, 1, MaxAgentCount);
}

void UAgentVisualizationManager::UpdatePerformanceMetrics(float DeltaTime)
{
    CurrentMetrics.ActiveAgentCount = VisualizedAgents.Num();
    CurrentMetrics.ActiveLinkCount = CommunicationLinks.Num();

    // Calculate FPS
    float FrameTime = DeltaTime * 1000.0f; // Convert to ms
    CurrentMetrics.AverageFrameTime = FMath::FInterpTo(
        CurrentMetrics.AverageFrameTime, FrameTime, DeltaTime, 2.0f);
    CurrentMetrics.CurrentFPS = 1000.0f / CurrentMetrics.AverageFrameTime;

    // Check if meeting target
    bool bWasOptimal = CurrentMetrics.bIsPerformanceOptimal;
    CurrentMetrics.bIsPerformanceOptimal = CurrentMetrics.CurrentFPS >= (TargetFPS * 0.95f);

    // Broadcast if threshold crossed
    if (bWasOptimal != CurrentMetrics.bIsPerformanceOptimal)
    {
        OnPerformanceThresholdCrossed.Broadcast(CurrentMetrics);
    }
}

void UAgentVisualizationManager::OptimizeForCurrentLoad()
{
    if (!CurrentMetrics.bIsPerformanceOptimal && VisualizedAgents.Num() > 0)
    {
        // Reduce state update frequency if struggling
        if (CurrentMetrics.CurrentFPS < TargetFPS * 0.8f)
        {
            StateUpdateInterval = FMath::Min(StateUpdateInterval * 1.5f, 0.5f);

            // Force LOD updates
            UpdateAgentLODs();

            if (bVerboseLogging)
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("Performance optimization: Reduced update interval to %.2f"),
                       StateUpdateInterval);
            }
        }
    }
    else if (CurrentMetrics.bIsPerformanceOptimal && StateUpdateInterval > 0.1f)
    {
        // Recover update frequency when performance allows
        StateUpdateInterval = FMath::Max(StateUpdateInterval * 0.9f, 0.1f);
    }
}

void UAgentVisualizationManager::UpdateAgentLODs()
{
    if (!LODSettings.bEnableDynamicLOD)
    {
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC || !PC->GetPawn())
    {
        return;
    }

    FVector ViewLocation = PC->GetPawn()->GetActorLocation();

    for (auto& Pair : VisualizedAgents)
    {
        if (Pair.Value)
        {
            float Distance = FVector::Dist(ViewLocation, Pair.Value->GetActorLocation());

            // Determine LOD level
            int32 LODLevel = 0;
            if (Distance > LODSettings.LOD2Distance)
            {
                LODLevel = 3;
            }
            else if (Distance > LODSettings.LOD1Distance)
            {
                LODLevel = 2;
            }
            else if (Distance > LODSettings.LOD0Distance)
            {
                LODLevel = 1;
            }

            // Apply LOD to avatar
            Pair.Value->SetLODLevel(LODLevel);

            // Handle culling
            if (Distance > LODSettings.CullDistance)
            {
                Pair.Value->SetActorHiddenInGame(true);
            }
            else
            {
                Pair.Value->SetActorHiddenInGame(false);
            }
        }
    }
}

// ===== Visual Style Management =====

void UAgentVisualizationManager::RegisterVisualStyle(const FString& AgentType, const FAgentVisualStyle& Style)
{
    RegisteredStyles.Add(AgentType, Style);
}

FAgentVisualStyle UAgentVisualizationManager::GetVisualStyleForType(const FString& AgentType)
{
    if (FAgentVisualStyle* Style = RegisteredStyles.Find(AgentType))
    {
        return *Style;
    }

    // Return default style
    if (FAgentVisualStyle* DefaultStyle = RegisteredStyles.Find(TEXT("generic")))
    {
        return *DefaultStyle;
    }

    return FAgentVisualStyle();
}

void UAgentVisualizationManager::SetGlobalAuraIntensity(float Intensity)
{
    GlobalAuraIntensityMultiplier = FMath::Clamp(Intensity, 0.0f, 3.0f);

    // Apply to all agents
    for (auto& Pair : VisualizedAgents)
    {
        if (Pair.Value && AgentStates.Contains(Pair.Key))
        {
            FAgentVisualizationState& State = AgentStates[Pair.Key];
            Pair.Value->SetAuraIntensity(State.AuraIntensity * GlobalAuraIntensityMultiplier);
        }
    }
}

void UAgentVisualizationManager::InitializeDefaultStyles()
{
    // Deep Tree Echo style
    FAgentVisualStyle DeepTreeStyle;
    DeepTreeStyle.StyleName = TEXT("DeepTreeEcho");
    DeepTreeStyle.DefaultAuraColor = FLinearColor(0.4f, 0.2f, 0.8f, 1.0f); // Purple
    DeepTreeStyle.DefaultScale = 1.0f;
    DeepTreeStyle.bEnableDetailedFacialExpressions = true;
    DeepTreeStyle.bEnablePhysicsSimulation = true;
    RegisteredStyles.Add(TEXT("deep-tree-echo"), DeepTreeStyle);

    // Eliza style
    FAgentVisualStyle ElizaStyle;
    ElizaStyle.StyleName = TEXT("Eliza");
    ElizaStyle.DefaultAuraColor = FLinearColor(0.3f, 0.8f, 0.5f, 1.0f); // Green
    ElizaStyle.DefaultScale = 0.95f;
    ElizaStyle.bEnableDetailedFacialExpressions = true;
    ElizaStyle.bEnablePhysicsSimulation = false;
    RegisteredStyles.Add(TEXT("eliza"), ElizaStyle);

    // Generic style
    FAgentVisualStyle GenericStyle;
    GenericStyle.StyleName = TEXT("Generic");
    GenericStyle.DefaultAuraColor = FLinearColor(0.5f, 0.5f, 0.8f, 1.0f); // Light blue
    GenericStyle.DefaultScale = 0.9f;
    GenericStyle.bEnableDetailedFacialExpressions = false;
    GenericStyle.bEnablePhysicsSimulation = false;
    RegisteredStyles.Add(TEXT("generic"), GenericStyle);
}

// ===== Agent Queries =====

TArray<FString> UAgentVisualizationManager::GetAllVisualizedAgentPaths()
{
    TArray<FString> Paths;
    VisualizedAgents.GetKeys(Paths);
    return Paths;
}

AAgentAvatarActor* UAgentVisualizationManager::GetAvatarForAgent(const FString& AgentPath)
{
    if (AAgentAvatarActor** AvatarPtr = VisualizedAgents.Find(AgentPath))
    {
        return *AvatarPtr;
    }
    return nullptr;
}

bool UAgentVisualizationManager::IsAgentVisualized(const FString& AgentPath)
{
    return VisualizedAgents.Contains(AgentPath);
}

// ===== Internal Methods =====

FString UAgentVisualizationManager::DetermineAgentType(const FString& AgentPath)
{
    // Parse agent type from path
    // Format: /mnt/agents/{type}-{id} or /mnt/agents/{name}

    FString AgentName = FPaths::GetBaseFilename(AgentPath);

    if (AgentName.Contains(TEXT("deep-tree-echo")) || AgentName.Contains(TEXT("dte")))
    {
        return TEXT("deep-tree-echo");
    }
    else if (AgentName.Contains(TEXT("eliza")))
    {
        return TEXT("eliza");
    }
    else if (AgentName.Contains(TEXT("opencog")) || AgentName.Contains(TEXT("atomspace")))
    {
        return TEXT("opencog");
    }

    return TEXT("generic");
}

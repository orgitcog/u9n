// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Visualization Performance Optimizer Implementation

#include "VisualizationPerformanceOptimizer.h"
#include "AgentVisualizationManager.h"
#include "AgentAvatarActor.h"
#include "../Rendering/DeepTreeEchoRenderingSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UVisualizationPerformanceOptimizer::UVisualizationPerformanceOptimizer()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // ~30 Hz for optimization checks

    // Default settings
    CurrentPreset = EVisualizationQualityPreset::Balanced;
    OptimizationStrategy = EOptimizationStrategy::Adaptive;

    GlobalLODBias = 0;
    bDynamicLODEnabled = true;
    LODDistanceScale = 1.0f;
    ForcedLODLevel = 0;
    bLODForced = false;

    bFrustumCullingEnabled = true;
    bDistanceCullingEnabled = true;
    bOcclusionCullingEnabled = false; // More expensive, off by default
    MaxRenderDistance = 5000.0f;

    bParticleSystemsEnabled = true;
    bAuraEffectsEnabled = true;
    bShadowsEnabled = true;
    bReflectionsEnabled = true;

    FrameTimeAccumulator = 0.0f;
    FrameCount = 0;
    AverageFrameTime = 16.67f;
    bOptimizationPaused = false;

    OptimizationTimer = 0.0f;
    CullingTimer = 0.0f;
    LODTimer = 0.0f;
    BudgetCheckTimer = 0.0f;
}

void UVisualizationPerformanceOptimizer::BeginPlay()
{
    Super::BeginPlay();

    // Get component references
    VisualizationManager = GetOwner()->FindComponentByClass<UAgentVisualizationManager>();
    RenderingSubsystem = UDeepTreeEchoRenderingSubsystem::Get(GetWorld());

    // Apply default preset
    ApplyQualityPreset(CurrentPreset);

    UE_LOG(LogTemp, Log, TEXT("VisualizationPerformanceOptimizer initialized with %s preset"),
           CurrentPreset == EVisualizationQualityPreset::Performance ? TEXT("Performance") :
           CurrentPreset == EVisualizationQualityPreset::Balanced ? TEXT("Balanced") :
           CurrentPreset == EVisualizationQualityPreset::Quality ? TEXT("Quality") : TEXT("Custom"));
}

void UVisualizationPerformanceOptimizer::TickComponent(float DeltaTime, ELevelTick TickType,
                                                        FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bOptimizationPaused)
    {
        return;
    }

    // Update performance metrics
    UpdatePerformanceMetrics(DeltaTime);

    // Perform culling at regular interval
    CullingTimer += DeltaTime;
    if (CullingTimer >= 0.1f) // 10 Hz
    {
        CullingTimer = 0.0f;
        PerformCulling();
    }

    // Update LODs at regular interval
    LODTimer += DeltaTime;
    if (LODTimer >= 0.25f) // 4 Hz
    {
        LODTimer = 0.0f;
        UpdateLODs();
    }

    // Check budgets at regular interval
    BudgetCheckTimer += DeltaTime;
    if (BudgetCheckTimer >= 0.5f) // 2 Hz
    {
        BudgetCheckTimer = 0.0f;
        CheckBudgets();
    }

    // Apply adaptive optimization
    OptimizationTimer += DeltaTime;
    if (OptimizationTimer >= 1.0f) // 1 Hz
    {
        OptimizationTimer = 0.0f;
        if (OptimizationStrategy == EOptimizationStrategy::Adaptive)
        {
            ApplyAdaptiveOptimization();
        }
    }
}

// ===== Configuration =====

void UVisualizationPerformanceOptimizer::SetVisualizationBudget(const FVisualizationBudget& Budget)
{
    CurrentBudget = Budget;
    CurrentPreset = EVisualizationQualityPreset::Custom;
}

void UVisualizationPerformanceOptimizer::SetQualityPreset(EVisualizationQualityPreset Preset)
{
    CurrentPreset = Preset;
    ApplyQualityPreset(Preset);
}

void UVisualizationPerformanceOptimizer::SetOptimizationStrategy(EOptimizationStrategy Strategy)
{
    OptimizationStrategy = Strategy;
}

void UVisualizationPerformanceOptimizer::SetInstancingConfig(const FAgentInstancingConfig& Config)
{
    InstancingConfig = Config;
    ApplyInstancing();
}

void UVisualizationPerformanceOptimizer::ApplyQualityPreset(EVisualizationQualityPreset Preset)
{
    switch (Preset)
    {
    case EVisualizationQualityPreset::Performance:
        CurrentBudget.TargetFrameTimeMs = 16.67f; // 60 FPS
        CurrentBudget.VisualizationBudgetMs = 4.0f;
        CurrentBudget.MaxActiveParticleSystems = 3;
        CurrentBudget.MaxVisibleAgents = 5;
        CurrentBudget.MaxCommunicationLinks = 10;
        bParticleSystemsEnabled = false;
        bAuraEffectsEnabled = true;
        bShadowsEnabled = false;
        bReflectionsEnabled = false;
        GlobalLODBias = 1;
        break;

    case EVisualizationQualityPreset::Balanced:
        CurrentBudget.TargetFrameTimeMs = 16.67f; // 60 FPS
        CurrentBudget.VisualizationBudgetMs = 8.0f;
        CurrentBudget.MaxActiveParticleSystems = 6;
        CurrentBudget.MaxVisibleAgents = 8;
        CurrentBudget.MaxCommunicationLinks = 15;
        bParticleSystemsEnabled = true;
        bAuraEffectsEnabled = true;
        bShadowsEnabled = true;
        bReflectionsEnabled = false;
        GlobalLODBias = 0;
        break;

    case EVisualizationQualityPreset::Quality:
        CurrentBudget.TargetFrameTimeMs = 16.67f; // 60 FPS
        CurrentBudget.VisualizationBudgetMs = 12.0f;
        CurrentBudget.MaxActiveParticleSystems = 10;
        CurrentBudget.MaxVisibleAgents = 10;
        CurrentBudget.MaxCommunicationLinks = 20;
        bParticleSystemsEnabled = true;
        bAuraEffectsEnabled = true;
        bShadowsEnabled = true;
        bReflectionsEnabled = true;
        GlobalLODBias = 0;
        break;

    case EVisualizationQualityPreset::Cinematic:
        CurrentBudget.TargetFrameTimeMs = 33.33f; // 30 FPS (cinematic)
        CurrentBudget.VisualizationBudgetMs = 20.0f;
        CurrentBudget.MaxActiveParticleSystems = 20;
        CurrentBudget.MaxVisibleAgents = 15;
        CurrentBudget.MaxCommunicationLinks = 50;
        bParticleSystemsEnabled = true;
        bAuraEffectsEnabled = true;
        bShadowsEnabled = true;
        bReflectionsEnabled = true;
        GlobalLODBias = -1; // Force higher LOD
        break;

    default:
        break;
    }
}

// ===== Agent Priority =====

void UVisualizationPerformanceOptimizer::SetAgentPriority(const FString& AgentPath, float Priority)
{
    FAgentRenderPriority& RenderPriority = AgentPriorities.FindOrAdd(AgentPath);
    RenderPriority.AgentPath = AgentPath;
    RenderPriority.Priority = FMath::Clamp(Priority, 0.0f, 1.0f);
}

void UVisualizationPerformanceOptimizer::SetAgentAlwaysRender(const FString& AgentPath, bool bAlwaysRender)
{
    FAgentRenderPriority& RenderPriority = AgentPriorities.FindOrAdd(AgentPath);
    RenderPriority.AgentPath = AgentPath;
    RenderPriority.bAlwaysRender = bAlwaysRender;
}

void UVisualizationPerformanceOptimizer::SetAgentMinLOD(const FString& AgentPath, int32 MinLOD)
{
    FAgentRenderPriority& RenderPriority = AgentPriorities.FindOrAdd(AgentPath);
    RenderPriority.AgentPath = AgentPath;
    RenderPriority.MinLODLevel = FMath::Clamp(MinLOD, 0, 3);
}

float UVisualizationPerformanceOptimizer::GetAgentPriority(const FString& AgentPath)
{
    if (FAgentRenderPriority* Priority = AgentPriorities.Find(AgentPath))
    {
        return Priority->Priority;
    }
    return 0.5f; // Default priority
}

void UVisualizationPerformanceOptimizer::ClearAgentPriorities()
{
    AgentPriorities.Empty();
    AgentImportanceScores.Empty();
}

// ===== LOD Control =====

void UVisualizationPerformanceOptimizer::SetGlobalLODBias(int32 Bias)
{
    GlobalLODBias = FMath::Clamp(Bias, -2, 3);
}

void UVisualizationPerformanceOptimizer::SetDynamicLODEnabled(bool bEnabled)
{
    bDynamicLODEnabled = bEnabled;
}

void UVisualizationPerformanceOptimizer::SetLODDistanceScale(float Scale)
{
    LODDistanceScale = FMath::Clamp(Scale, 0.1f, 3.0f);
}

void UVisualizationPerformanceOptimizer::ForceLODLevel(int32 Level)
{
    ForcedLODLevel = FMath::Clamp(Level, 0, 3);
    bLODForced = true;
}

void UVisualizationPerformanceOptimizer::ClearForcedLOD()
{
    bLODForced = false;
}

// ===== Culling =====

void UVisualizationPerformanceOptimizer::SetFrustumCullingEnabled(bool bEnabled)
{
    bFrustumCullingEnabled = bEnabled;
}

void UVisualizationPerformanceOptimizer::SetDistanceCullingEnabled(bool bEnabled)
{
    bDistanceCullingEnabled = bEnabled;
}

void UVisualizationPerformanceOptimizer::SetMaxRenderDistance(float Distance)
{
    MaxRenderDistance = FMath::Max(100.0f, Distance);
}

void UVisualizationPerformanceOptimizer::SetOcclusionCullingEnabled(bool bEnabled)
{
    bOcclusionCullingEnabled = bEnabled;
}

// ===== Performance Queries =====

FOptimizationReport UVisualizationPerformanceOptimizer::GetOptimizationReport()
{
    return LastReport;
}

bool UVisualizationPerformanceOptimizer::IsMeetingTargetFPS()
{
    return AverageFrameTime <= CurrentBudget.TargetFrameTimeMs;
}

float UVisualizationPerformanceOptimizer::GetCurrentVisualizationLoad()
{
    if (CurrentBudget.VisualizationBudgetMs <= 0.0f)
    {
        return 0.0f;
    }
    return LastReport.VisualizationTimeMs / CurrentBudget.VisualizationBudgetMs;
}

int32 UVisualizationPerformanceOptimizer::GetRecommendedMaxAgents()
{
    if (!VisualizationManager)
    {
        return CurrentBudget.MaxVisibleAgents;
    }

    int32 CurrentAgentCount = VisualizationManager->GetActiveAgentCount();
    if (CurrentAgentCount <= 0)
    {
        return CurrentBudget.MaxVisibleAgents;
    }

    // Calculate based on current performance
    float TimePerAgent = LastReport.VisualizationTimeMs / CurrentAgentCount;
    if (TimePerAgent <= 0.0f)
    {
        return CurrentBudget.MaxVisibleAgents;
    }

    float AvailableTime = CurrentBudget.VisualizationBudgetMs;
    int32 Recommended = FMath::FloorToInt(AvailableTime / TimePerAgent);

    return FMath::Clamp(Recommended, 1, CurrentBudget.MaxVisibleAgents);
}

TArray<FString> UVisualizationPerformanceOptimizer::GetCulledAgents()
{
    return CulledAgents;
}

// ===== Manual Optimization =====

void UVisualizationPerformanceOptimizer::OptimizeNow()
{
    PerformCulling();
    UpdateLODs();
    CheckBudgets();
    ApplyAdaptiveOptimization();

    // Generate and broadcast report
    LastReport.Timestamp = FDateTime::Now();
    OnOptimizationApplied.Broadcast(LastReport);
}

void UVisualizationPerformanceOptimizer::ResetOptimizations()
{
    // Reset all optimization state
    ClearAgentPriorities();
    ClearForcedLOD();
    CulledAgents.Empty();

    // Re-apply preset
    ApplyQualityPreset(CurrentPreset);

    // Unhide all agents
    if (VisualizationManager)
    {
        TArray<FString> AllAgents = VisualizationManager->GetAllVisualizedAgentPaths();
        for (const FString& AgentPath : AllAgents)
        {
            AAgentAvatarActor* Avatar = VisualizationManager->GetAvatarForAgent(AgentPath);
            if (Avatar)
            {
                Avatar->SetActorHiddenInGame(false);
                Avatar->SetLODLevel(0);
            }
        }
    }
}

void UVisualizationPerformanceOptimizer::PauseOptimization(bool bPause)
{
    bOptimizationPaused = bPause;
}

void UVisualizationPerformanceOptimizer::ProfileFrame()
{
    // Record detailed frame timing
    double StartTime = FPlatformTime::Seconds();

    // Profile visualization update
    if (VisualizationManager)
    {
        VisualizationManager->SyncAllAgentStates();
    }

    double EndTime = FPlatformTime::Seconds();
    LastReport.VisualizationTimeMs = (EndTime - StartTime) * 1000.0;

    UE_LOG(LogTemp, Log, TEXT("Frame profile: Visualization=%.2fms, Target=%.2fms"),
           LastReport.VisualizationTimeMs, CurrentBudget.VisualizationBudgetMs);
}

// ===== Effect Control =====

void UVisualizationPerformanceOptimizer::SetParticleSystemsEnabled(bool bEnabled)
{
    bParticleSystemsEnabled = bEnabled;

    // Apply to all agent avatars
    if (VisualizationManager)
    {
        TArray<FString> AllAgents = VisualizationManager->GetAllVisualizedAgentPaths();
        for (const FString& AgentPath : AllAgents)
        {
            AAgentAvatarActor* Avatar = VisualizationManager->GetAvatarForAgent(AgentPath);
            if (Avatar)
            {
                Avatar->TriggerThinkingEffect(false);
            }
        }
    }
}

void UVisualizationPerformanceOptimizer::SetAuraEffectsEnabled(bool bEnabled)
{
    bAuraEffectsEnabled = bEnabled;

    // Apply to all agent avatars
    if (VisualizationManager)
    {
        TArray<FString> AllAgents = VisualizationManager->GetAllVisualizedAgentPaths();
        for (const FString& AgentPath : AllAgents)
        {
            AAgentAvatarActor* Avatar = VisualizationManager->GetAvatarForAgent(AgentPath);
            if (Avatar)
            {
                Avatar->SetAuraVisible(bEnabled);
            }
        }
    }
}

void UVisualizationPerformanceOptimizer::SetShadowsEnabled(bool bEnabled)
{
    bShadowsEnabled = bEnabled;

    // Would toggle shadow casting on avatar meshes
}

void UVisualizationPerformanceOptimizer::SetReflectionsEnabled(bool bEnabled)
{
    bReflectionsEnabled = bEnabled;

    // Would toggle reflection captures
}

// ===== Internal Methods =====

void UVisualizationPerformanceOptimizer::UpdatePerformanceMetrics(float DeltaTime)
{
    // Accumulate frame times
    FrameTimeAccumulator += DeltaTime * 1000.0f; // Convert to ms
    FrameCount++;

    // Calculate average every 30 frames
    if (FrameCount >= 30)
    {
        AverageFrameTime = FrameTimeAccumulator / FrameCount;
        FrameTimeAccumulator = 0.0f;
        FrameCount = 0;

        // Update report
        LastReport.AverageFrameTimeMs = AverageFrameTime;
        LastReport.CurrentFPS = 1000.0f / AverageFrameTime;
        LastReport.bMeetingTargetFPS = AverageFrameTime <= CurrentBudget.TargetFrameTimeMs;

        if (VisualizationManager)
        {
            LastReport.ActiveAgentCount = VisualizationManager->GetActiveAgentCount();
            LastReport.VisibleAgentCount = LastReport.ActiveAgentCount - CulledAgents.Num();
            LastReport.CulledAgentCount = CulledAgents.Num();
        }
    }
}

void UVisualizationPerformanceOptimizer::PerformCulling()
{
    if (!VisualizationManager)
    {
        return;
    }

    CulledAgents.Empty();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

    TArray<FString> AllAgents = VisualizationManager->GetAllVisualizedAgentPaths();

    // Calculate importance for all agents
    CalculateAgentImportance();

    // Sort by importance
    AllAgents.Sort([this](const FString& A, const FString& B) {
        float ScoreA = AgentImportanceScores.FindRef(A);
        float ScoreB = AgentImportanceScores.FindRef(B);
        return ScoreA > ScoreB;
    });

    int32 VisibleCount = 0;

    for (const FString& AgentPath : AllAgents)
    {
        AAgentAvatarActor* Avatar = VisualizationManager->GetAvatarForAgent(AgentPath);
        if (!Avatar)
        {
            continue;
        }

        // Check if always render
        if (FAgentRenderPriority* Priority = AgentPriorities.Find(AgentPath))
        {
            if (Priority->bAlwaysRender)
            {
                Avatar->SetActorHiddenInGame(false);
                VisibleCount++;
                continue;
            }
        }

        // Check max visible limit
        if (VisibleCount >= CurrentBudget.MaxVisibleAgents)
        {
            Avatar->SetActorHiddenInGame(true);
            CulledAgents.Add(AgentPath);
            continue;
        }

        // Distance culling
        if (bDistanceCullingEnabled)
        {
            float Distance = FVector::Dist(ViewLocation, Avatar->GetActorLocation());
            if (Distance > MaxRenderDistance)
            {
                Avatar->SetActorHiddenInGame(true);
                CulledAgents.Add(AgentPath);
                continue;
            }
        }

        // Frustum culling
        if (bFrustumCullingEnabled)
        {
            FVector ToAvatar = Avatar->GetActorLocation() - ViewLocation;
            ToAvatar.Normalize();
            FVector ViewDir = ViewRotation.Vector();

            float DotProduct = FVector::DotProduct(ViewDir, ToAvatar);
            if (DotProduct < 0.0f) // Behind camera
            {
                Avatar->SetActorHiddenInGame(true);
                CulledAgents.Add(AgentPath);
                continue;
            }
        }

        // Not culled
        Avatar->SetActorHiddenInGame(false);
        VisibleCount++;
    }
}

void UVisualizationPerformanceOptimizer::UpdateLODs()
{
    if (!VisualizationManager || !bDynamicLODEnabled)
    {
        return;
    }

    if (bLODForced)
    {
        // Apply forced LOD to all
        TArray<FString> AllAgents = VisualizationManager->GetAllVisualizedAgentPaths();
        for (const FString& AgentPath : AllAgents)
        {
            AAgentAvatarActor* Avatar = VisualizationManager->GetAvatarForAgent(AgentPath);
            if (Avatar)
            {
                Avatar->SetLODLevel(ForcedLODLevel);
            }
        }
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

    TArray<FString> AllAgents = VisualizationManager->GetAllVisualizedAgentPaths();
    int32 LODReductions = 0;

    for (const FString& AgentPath : AllAgents)
    {
        AAgentAvatarActor* Avatar = VisualizationManager->GetAvatarForAgent(AgentPath);
        if (!Avatar || Avatar->IsHidden())
        {
            continue;
        }

        float Distance = FVector::Dist(ViewLocation, Avatar->GetActorLocation());
        Distance *= LODDistanceScale;

        // Calculate LOD level based on distance
        int32 LODLevel = 0;
        if (Distance > 3000.0f)
        {
            LODLevel = 3;
        }
        else if (Distance > 1500.0f)
        {
            LODLevel = 2;
        }
        else if (Distance > 500.0f)
        {
            LODLevel = 1;
        }

        // Apply bias
        LODLevel = FMath::Clamp(LODLevel + GlobalLODBias, 0, 3);

        // Check min LOD
        if (FAgentRenderPriority* Priority = AgentPriorities.Find(AgentPath))
        {
            LODLevel = FMath::Max(LODLevel, Priority->MinLODLevel);
        }

        if (LODLevel > 0)
        {
            LODReductions++;
        }

        Avatar->SetLODLevel(LODLevel);
    }

    LastReport.LODReductionCount = LODReductions;
}

void UVisualizationPerformanceOptimizer::ApplyAdaptiveOptimization()
{
    if (!IsMeetingTargetFPS())
    {
        // Performance is suffering, apply optimizations
        TArray<FString> OptimizationsApplied;

        float FrameOverrun = AverageFrameTime - CurrentBudget.TargetFrameTimeMs;

        if (FrameOverrun > 10.0f) // Severe performance issue
        {
            // Aggressive optimization
            SetGlobalLODBias(GlobalLODBias + 1);
            SetParticleSystemsEnabled(false);
            SetAuraEffectsEnabled(false);
            OptimizationsApplied.Add(TEXT("Disabled particles and auras"));
            OptimizationsApplied.Add(TEXT("Increased LOD bias"));
        }
        else if (FrameOverrun > 5.0f) // Moderate issue
        {
            // Moderate optimization
            SetGlobalLODBias(GlobalLODBias + 1);
            SetParticleSystemsEnabled(false);
            OptimizationsApplied.Add(TEXT("Disabled particles"));
            OptimizationsApplied.Add(TEXT("Increased LOD bias"));
        }
        else // Minor issue
        {
            // Light optimization
            SetGlobalLODBias(GlobalLODBias + 1);
            OptimizationsApplied.Add(TEXT("Increased LOD bias"));
        }

        LastReport.AppliedOptimizations = OptimizationsApplied;

        UE_LOG(LogTemp, Warning, TEXT("Adaptive optimization applied: Frame time %.2fms > target %.2fms"),
               AverageFrameTime, CurrentBudget.TargetFrameTimeMs);
    }
    else if (AverageFrameTime < CurrentBudget.TargetFrameTimeMs * 0.7f)
    {
        // Performance headroom available, restore quality
        if (GlobalLODBias > 0)
        {
            SetGlobalLODBias(GlobalLODBias - 1);
        }
        else if (!bParticleSystemsEnabled && CurrentPreset != EVisualizationQualityPreset::Performance)
        {
            SetParticleSystemsEnabled(true);
        }
        else if (!bAuraEffectsEnabled && CurrentPreset != EVisualizationQualityPreset::Performance)
        {
            SetAuraEffectsEnabled(true);
        }
    }
}

void UVisualizationPerformanceOptimizer::CheckBudgets()
{
    // Check visualization time budget
    if (LastReport.VisualizationTimeMs > CurrentBudget.VisualizationBudgetMs)
    {
        OnBudgetExceeded.Broadcast(TEXT("VisualizationTime"), LastReport.VisualizationTimeMs);
    }

    // Check agent count budget
    if (LastReport.VisibleAgentCount > CurrentBudget.MaxVisibleAgents)
    {
        OnBudgetExceeded.Broadcast(TEXT("AgentCount"), (float)LastReport.VisibleAgentCount);
    }

    // Check memory budget
    if (LastReport.MemoryUsageMB > CurrentBudget.MemoryBudgetMB)
    {
        OnBudgetExceeded.Broadcast(TEXT("Memory"), LastReport.MemoryUsageMB);
    }
}

void UVisualizationPerformanceOptimizer::CalculateAgentImportance()
{
    if (!VisualizationManager)
    {
        return;
    }

    AgentImportanceScores.Empty();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    FVector ViewLocation = FVector::ZeroVector;
    if (PC)
    {
        FRotator ViewRotation;
        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    TArray<FString> AllAgents = VisualizationManager->GetAllVisualizedAgentPaths();

    for (const FString& AgentPath : AllAgents)
    {
        float Score = CalculateAgentImportanceScore(AgentPath);
        AgentImportanceScores.Add(AgentPath, Score);
    }
}

float UVisualizationPerformanceOptimizer::CalculateAgentImportanceScore(const FString& AgentPath)
{
    float Score = 0.5f; // Base score

    // Add priority bonus
    if (FAgentRenderPriority* Priority = AgentPriorities.Find(AgentPath))
    {
        Score += Priority->Priority * 0.3f;
        if (Priority->bAlwaysRender)
        {
            Score += 1.0f;
        }
    }

    // Get avatar info
    AAgentAvatarActor* Avatar = VisualizationManager->GetAvatarForAgent(AgentPath);
    if (Avatar)
    {
        // Add attention level bonus
        Score += Avatar->GetAttentionLevel() * 0.2f;

        // Add activity level bonus
        Score += Avatar->GetActivityLevel() * 0.1f;

        // Online bonus
        if (Avatar->IsOnline())
        {
            Score += 0.1f;
        }

        // Distance factor (closer = higher priority)
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            FVector ViewLocation;
            FRotator ViewRotation;
            PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

            float Distance = FVector::Dist(ViewLocation, Avatar->GetActorLocation());
            float DistanceFactor = 1.0f - FMath::Clamp(Distance / MaxRenderDistance, 0.0f, 1.0f);
            Score += DistanceFactor * 0.3f;
        }
    }

    // Deep Tree Echo gets priority
    if (AgentPath.Contains(TEXT("deep-tree-echo")))
    {
        Score += 0.5f;
    }

    return FMath::Clamp(Score, 0.0f, 2.0f);
}

TArray<FString> UVisualizationPerformanceOptimizer::GetAgentsSortedByPriority()
{
    TArray<FString> Agents;

    for (auto& Pair : AgentImportanceScores)
    {
        Agents.Add(Pair.Key);
    }

    Agents.Sort([this](const FString& A, const FString& B) {
        return AgentImportanceScores.FindRef(A) > AgentImportanceScores.FindRef(B);
    });

    return Agents;
}

void UVisualizationPerformanceOptimizer::ApplyInstancing()
{
    if (!InstancingConfig.bEnableInstancing || !VisualizationManager)
    {
        return;
    }

    int32 AgentCount = VisualizationManager->GetActiveAgentCount();
    if (AgentCount < InstancingConfig.MinInstanceCount)
    {
        return;
    }

    // Would group similar agents and enable instanced rendering
    UE_LOG(LogTemp, Log, TEXT("Instancing enabled for %d agents"), AgentCount);
}

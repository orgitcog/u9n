//=============================================================================
// AvatarPerformanceMonitor.cpp
// 
// Implementation of performance monitoring and profiling for Deep Tree Echo.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "AvatarPerformanceMonitor.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "Stats/Stats.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UAvatarPerformanceMonitor::UAvatarPerformanceMonitor()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Tick every frame
    
    bIsMonitoring = false;
    TimeSinceLastSample = 0.0f;
    bStatsNeedUpdate = true;
}

void UAvatarPerformanceMonitor::BeginPlay()
{
    Super::BeginPlay();
    
    if (bEnableMonitoring)
    {
        StartMonitoring();
    }
}

void UAvatarPerformanceMonitor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (!bIsMonitoring)
    {
        return;
    }
    
    TimeSinceLastSample += DeltaTime;
    
    if (TimeSinceLastSample >= SamplingInterval)
    {
        CollectPerformanceSample();
        TimeSinceLastSample = 0.0f;
        bStatsNeedUpdate = true;
    }
}

void UAvatarPerformanceMonitor::StartMonitoring()
{
    bIsMonitoring = true;
    TimeSinceLastSample = 0.0f;
    PerformanceSamples.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("AvatarPerformanceMonitor: Monitoring started"));
}

void UAvatarPerformanceMonitor::StopMonitoring()
{
    bIsMonitoring = false;
    
    UE_LOG(LogTemp, Log, TEXT("AvatarPerformanceMonitor: Monitoring stopped"));
}

void UAvatarPerformanceMonitor::ResetStatistics()
{
    PerformanceSamples.Empty();
    bStatsNeedUpdate = true;
    
    UE_LOG(LogTemp, Log, TEXT("AvatarPerformanceMonitor: Statistics reset"));
}

FPerformanceStats UAvatarPerformanceMonitor::GetPerformanceStats() const
{
    if (bStatsNeedUpdate)
    {
        const_cast<UAvatarPerformanceMonitor*>(this)->UpdateStatistics();
    }
    
    return CachedStats;
}

FPerformanceSample UAvatarPerformanceMonitor::GetCurrentSample() const
{
    if (PerformanceSamples.Num() > 0)
    {
        return PerformanceSamples.Last();
    }
    
    return FPerformanceSample();
}

TArray<FPerformanceSample> UAvatarPerformanceMonitor::GetRecentSamples(int32 Count) const
{
    TArray<FPerformanceSample> RecentSamples;
    
    int32 StartIndex = FMath::Max(0, PerformanceSamples.Num() - Count);
    for (int32 i = StartIndex; i < PerformanceSamples.Num(); i++)
    {
        RecentSamples.Add(PerformanceSamples[i]);
    }
    
    return RecentSamples;
}

bool UAvatarPerformanceMonitor::IsWithinBudget() const
{
    if (PerformanceSamples.Num() == 0)
    {
        return true;
    }
    
    const FPerformanceSample& CurrentSample = PerformanceSamples.Last();
    
    bool bWithinBudget = true;
    bWithinBudget &= (CurrentSample.FrameTime <= PerformanceBudget.MaxFrameTimeMS);
    bWithinBudget &= (CurrentSample.CPUTime <= PerformanceBudget.MaxCPUTimeMS);
    bWithinBudget &= (CurrentSample.GPUTime <= PerformanceBudget.MaxGPUTimeMS);
    bWithinBudget &= (CurrentSample.MemoryUsageMB <= PerformanceBudget.MaxMemoryMB);
    bWithinBudget &= (CurrentSample.DrawCalls <= PerformanceBudget.MaxDrawCalls);
    bWithinBudget &= (CurrentSample.TriangleCount <= PerformanceBudget.MaxTriangles);
    
    return bWithinBudget;
}

float UAvatarPerformanceMonitor::GetAverageFPS() const
{
    return GetPerformanceStats().AverageFPS;
}

float UAvatarPerformanceMonitor::GetAverageFrameTime() const
{
    return GetPerformanceStats().AverageFrameTime;
}

void UAvatarPerformanceMonitor::ExportPerformanceReport(const FString& FilePath)
{
    FString Report;
    
    // Header
    Report += TEXT("Deep Tree Echo Avatar - Performance Report\n");
    Report += TEXT("==========================================\n\n");
    
    // Statistics
    FPerformanceStats Stats = GetPerformanceStats();
    Report += TEXT("Performance Statistics:\n");
    Report += FString::Printf(TEXT("  Average FPS: %.2f\n"), Stats.AverageFPS);
    Report += FString::Printf(TEXT("  Min FPS: %.2f\n"), Stats.MinFPS);
    Report += FString::Printf(TEXT("  Max FPS: %.2f\n"), Stats.MaxFPS);
    Report += FString::Printf(TEXT("  Average Frame Time: %.2f ms\n"), Stats.AverageFrameTime);
    Report += FString::Printf(TEXT("  Average CPU Time: %.2f ms\n"), Stats.AverageCPUTime);
    Report += FString::Printf(TEXT("  Average GPU Time: %.2f ms\n"), Stats.AverageGPUTime);
    Report += FString::Printf(TEXT("  Average Memory: %.2f MB\n"), Stats.AverageMemoryMB);
    Report += FString::Printf(TEXT("  Peak Memory: %.2f MB\n\n"), Stats.PeakMemoryMB);
    
    // Budget comparison
    Report += TEXT("Performance Budget:\n");
    Report += FString::Printf(TEXT("  Target FPS: %.2f (Current: %.2f) %s\n"), 
        PerformanceBudget.TargetFPS, Stats.AverageFPS,
        Stats.AverageFPS >= PerformanceBudget.TargetFPS ? TEXT("✓") : TEXT("✗"));
    Report += FString::Printf(TEXT("  Max Frame Time: %.2f ms (Current: %.2f ms) %s\n"),
        PerformanceBudget.MaxFrameTimeMS, Stats.AverageFrameTime,
        Stats.AverageFrameTime <= PerformanceBudget.MaxFrameTimeMS ? TEXT("✓") : TEXT("✗"));
    Report += FString::Printf(TEXT("  Max CPU Time: %.2f ms (Current: %.2f ms) %s\n"),
        PerformanceBudget.MaxCPUTimeMS, Stats.AverageCPUTime,
        Stats.AverageCPUTime <= PerformanceBudget.MaxCPUTimeMS ? TEXT("✓") : TEXT("✗"));
    Report += FString::Printf(TEXT("  Max GPU Time: %.2f ms (Current: %.2f ms) %s\n"),
        PerformanceBudget.MaxGPUTimeMS, Stats.AverageGPUTime,
        Stats.AverageGPUTime <= PerformanceBudget.MaxGPUTimeMS ? TEXT("✓") : TEXT("✗"));
    Report += FString::Printf(TEXT("  Max Memory: %.2f MB (Current: %.2f MB) %s\n\n"),
        PerformanceBudget.MaxMemoryMB, Stats.AverageMemoryMB,
        Stats.AverageMemoryMB <= PerformanceBudget.MaxMemoryMB ? TEXT("✓") : TEXT("✗"));
    
    // Sample data (last 100 samples)
    Report += TEXT("Recent Performance Samples:\n");
    Report += TEXT("Timestamp,FrameTime,CPUTime,GPUTime,Memory,DrawCalls,Triangles\n");
    
    TArray<FPerformanceSample> RecentSamples = GetRecentSamples(100);
    for (const FPerformanceSample& Sample : RecentSamples)
    {
        Report += FString::Printf(TEXT("%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n"),
            Sample.Timestamp,
            Sample.FrameTime,
            Sample.CPUTime,
            Sample.GPUTime,
            Sample.MemoryUsageMB,
            Sample.DrawCalls,
            Sample.TriangleCount);
    }
    
    // Save to file
    FString FullPath = FPaths::ProjectSavedDir() / FilePath;
    if (FFileHelper::SaveStringToFile(Report, *FullPath))
    {
        UE_LOG(LogTemp, Log, TEXT("AvatarPerformanceMonitor: Report exported to %s"), *FullPath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AvatarPerformanceMonitor: Failed to export report"));
    }
}

void UAvatarPerformanceMonitor::CollectPerformanceSample()
{
    FPerformanceSample Sample;
    
    Sample.Timestamp = GetWorld()->GetTimeSeconds();
    Sample.FrameTime = GetCurrentFrameTime();
    Sample.CPUTime = GetCurrentCPUTime();
    Sample.GPUTime = GetCurrentGPUTime();
    Sample.MemoryUsageMB = GetCurrentMemoryUsage();
    Sample.DrawCalls = GetCurrentDrawCalls();
    Sample.TriangleCount = GetCurrentTriangleCount();
    
    PerformanceSamples.Add(Sample);
    
    // Limit sample buffer size
    if (PerformanceSamples.Num() > MaxSamples)
    {
        PerformanceSamples.RemoveAt(0);
    }
    
    // Check performance budget
    if (bEnableWarnings)
    {
        CheckPerformanceBudget(Sample);
    }
}

void UAvatarPerformanceMonitor::UpdateStatistics()
{
    if (PerformanceSamples.Num() == 0)
    {
        CachedStats = FPerformanceStats();
        bStatsNeedUpdate = false;
        return;
    }
    
    float TotalFrameTime = 0.0f;
    float TotalCPUTime = 0.0f;
    float TotalGPUTime = 0.0f;
    float TotalMemory = 0.0f;
    float MinFrameTime = FLT_MAX;
    float MaxFrameTime = 0.0f;
    float PeakMemory = 0.0f;
    
    for (const FPerformanceSample& Sample : PerformanceSamples)
    {
        TotalFrameTime += Sample.FrameTime;
        TotalCPUTime += Sample.CPUTime;
        TotalGPUTime += Sample.GPUTime;
        TotalMemory += Sample.MemoryUsageMB;
        
        MinFrameTime = FMath::Min(MinFrameTime, Sample.FrameTime);
        MaxFrameTime = FMath::Max(MaxFrameTime, Sample.FrameTime);
        PeakMemory = FMath::Max(PeakMemory, Sample.MemoryUsageMB);
    }
    
    int32 Count = PerformanceSamples.Num();
    
    CachedStats.AverageFrameTime = TotalFrameTime / Count;
    CachedStats.AverageCPUTime = TotalCPUTime / Count;
    CachedStats.AverageGPUTime = TotalGPUTime / Count;
    CachedStats.AverageMemoryMB = TotalMemory / Count;
    CachedStats.PeakMemoryMB = PeakMemory;
    
    // Calculate FPS
    CachedStats.AverageFPS = (CachedStats.AverageFrameTime > 0.0f) ? (1000.0f / CachedStats.AverageFrameTime) : 0.0f;
    CachedStats.MinFPS = (MaxFrameTime > 0.0f) ? (1000.0f / MaxFrameTime) : 0.0f;
    CachedStats.MaxFPS = (MinFrameTime > 0.0f) ? (1000.0f / MinFrameTime) : 0.0f;
    
    bStatsNeedUpdate = false;
}

void UAvatarPerformanceMonitor::CheckPerformanceBudget(const FPerformanceSample& Sample)
{
    if (Sample.FrameTime > PerformanceBudget.MaxFrameTimeMS)
    {
        OnPerformanceWarning.Broadcast(EPerformanceMetric::FrameTime, Sample.FrameTime);
    }
    
    if (Sample.CPUTime > PerformanceBudget.MaxCPUTimeMS)
    {
        OnPerformanceWarning.Broadcast(EPerformanceMetric::CPUTime, Sample.CPUTime);
    }
    
    if (Sample.GPUTime > PerformanceBudget.MaxGPUTimeMS)
    {
        OnPerformanceWarning.Broadcast(EPerformanceMetric::GPUTime, Sample.GPUTime);
    }
    
    if (Sample.MemoryUsageMB > PerformanceBudget.MaxMemoryMB)
    {
        OnPerformanceWarning.Broadcast(EPerformanceMetric::MemoryUsage, Sample.MemoryUsageMB);
    }
    
    if (Sample.DrawCalls > PerformanceBudget.MaxDrawCalls)
    {
        OnPerformanceWarning.Broadcast(EPerformanceMetric::DrawCalls, static_cast<float>(Sample.DrawCalls));
    }
    
    if (Sample.TriangleCount > PerformanceBudget.MaxTriangles)
    {
        OnPerformanceWarning.Broadcast(EPerformanceMetric::TriangleCount, static_cast<float>(Sample.TriangleCount));
    }
}

float UAvatarPerformanceMonitor::GetCurrentFrameTime() const
{
    // Get frame time in milliseconds
    return FPlatformTime::ToMilliseconds(GFrameTime) * 1000.0f;
}

float UAvatarPerformanceMonitor::GetCurrentCPUTime() const
{
    // Get CPU time from stats
    // In production, this would query actual CPU profiling data
    return GetCurrentFrameTime() * 0.6f; // Estimate: 60% of frame time
}

float UAvatarPerformanceMonitor::GetCurrentGPUTime() const
{
    // Get GPU time from stats
    // In production, this would query actual GPU profiling data
    return GetCurrentFrameTime() * 0.4f; // Estimate: 40% of frame time
}

float UAvatarPerformanceMonitor::GetCurrentMemoryUsage() const
{
    // Get current memory usage in MB
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    return static_cast<float>(MemStats.UsedPhysical) / (1024.0f * 1024.0f);
}

int32 UAvatarPerformanceMonitor::GetCurrentDrawCalls() const
{
    // Get draw call count from rendering stats
    // In production, this would query actual rendering stats
    return 500; // Placeholder
}

int32 UAvatarPerformanceMonitor::GetCurrentTriangleCount() const
{
    // Get triangle count from rendering stats
    // In production, this would query actual rendering stats
    return 85000; // Placeholder (within 80K-100K budget)
}

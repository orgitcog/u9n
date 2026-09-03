//=============================================================================
// AvatarPerformanceMonitor.h
// 
// Performance monitoring and profiling component for Deep Tree Echo avatar.
// Tracks CPU/GPU usage, memory consumption, and system performance metrics.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AvatarPerformanceMonitor.generated.h"

/**
 * Performance metric types
 */
UENUM(BlueprintType)
enum class EPerformanceMetric : uint8
{
    FrameTime,
    CPUTime,
    GPUTime,
    MemoryUsage,
    DrawCalls,
    TriangleCount,
    TextureMemory,
    AnimationTime,
    PhysicsTime,
    AITime
};

/**
 * Performance sample data
 */
USTRUCT(BlueprintType)
struct FPerformanceSample
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    float Timestamp;
    
    UPROPERTY(BlueprintReadOnly)
    float FrameTime;
    
    UPROPERTY(BlueprintReadOnly)
    float CPUTime;
    
    UPROPERTY(BlueprintReadOnly)
    float GPUTime;
    
    UPROPERTY(BlueprintReadOnly)
    float MemoryUsageMB;
    
    UPROPERTY(BlueprintReadOnly)
    int32 DrawCalls;
    
    UPROPERTY(BlueprintReadOnly)
    int32 TriangleCount;
    
    FPerformanceSample()
        : Timestamp(0.0f)
        , FrameTime(0.0f)
        , CPUTime(0.0f)
        , GPUTime(0.0f)
        , MemoryUsageMB(0.0f)
        , DrawCalls(0)
        , TriangleCount(0)
    {}
};

/**
 * Performance statistics
 */
USTRUCT(BlueprintType)
struct FPerformanceStats
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    float AverageFPS;
    
    UPROPERTY(BlueprintReadOnly)
    float MinFPS;
    
    UPROPERTY(BlueprintReadOnly)
    float MaxFPS;
    
    UPROPERTY(BlueprintReadOnly)
    float AverageFrameTime;
    
    UPROPERTY(BlueprintReadOnly)
    float AverageCPUTime;
    
    UPROPERTY(BlueprintReadOnly)
    float AverageGPUTime;
    
    UPROPERTY(BlueprintReadOnly)
    float AverageMemoryMB;
    
    UPROPERTY(BlueprintReadOnly)
    float PeakMemoryMB;
    
    FPerformanceStats()
        : AverageFPS(0.0f)
        , MinFPS(0.0f)
        , MaxFPS(0.0f)
        , AverageFrameTime(0.0f)
        , AverageCPUTime(0.0f)
        , AverageGPUTime(0.0f)
        , AverageMemoryMB(0.0f)
        , PeakMemoryMB(0.0f)
    {}
};

/**
 * Performance budget thresholds
 */
USTRUCT(BlueprintType)
struct FPerformanceBudget
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TargetFPS = 60.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxFrameTimeMS = 16.67f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxCPUTimeMS = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxGPUTimeMS = 12.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxMemoryMB = 512.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxDrawCalls = 1000;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxTriangles = 100000;
};

/**
 * Performance warning event
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPerformanceWarning, EPerformanceMetric, Metric, float, Value);

/**
 * Performance monitoring component
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAvatarPerformanceMonitor : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarPerformanceMonitor();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    FPerformanceBudget PerformanceBudget;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableMonitoring = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableWarnings = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float SamplingInterval = 0.1f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 MaxSamples = 600; // 60 seconds at 0.1s interval
    
    // Events
    UPROPERTY(BlueprintAssignable, Category = "Performance")
    FOnPerformanceWarning OnPerformanceWarning;
    
    // Monitoring functions
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void StartMonitoring();
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void StopMonitoring();
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void ResetStatistics();
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    FPerformanceStats GetPerformanceStats() const;
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    FPerformanceSample GetCurrentSample() const;
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    TArray<FPerformanceSample> GetRecentSamples(int32 Count) const;
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    bool IsWithinBudget() const;
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    float GetAverageFPS() const;
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    float GetAverageFrameTime() const;
    
    UFUNCTION(BlueprintCallable, Category = "Performance")
    void ExportPerformanceReport(const FString& FilePath);

private:
    // Internal state
    bool bIsMonitoring;
    float TimeSinceLastSample;
    TArray<FPerformanceSample> PerformanceSamples;
    FPerformanceStats CachedStats;
    bool bStatsNeedUpdate;
    
    // Monitoring functions
    void CollectPerformanceSample();
    void UpdateStatistics();
    void CheckPerformanceBudget(const FPerformanceSample& Sample);
    
    // Metric collection
    float GetCurrentFrameTime() const;
    float GetCurrentCPUTime() const;
    float GetCurrentGPUTime() const;
    float GetCurrentMemoryUsage() const;
    int32 GetCurrentDrawCalls() const;
    int32 GetCurrentTriangleCount() const;
};

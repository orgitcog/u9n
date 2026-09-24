#pragma once

/**
 * Reservoir Topology Manager
 *
 * Provides dynamic reservoir configuration for the Deep Tree Echo cognitive
 * architecture. Where the Reservoir Topology Generator (F1.2.3) produces
 * static network topologies, the Manager owns the *lifecycle* of a reservoir
 * topology at runtime:
 *
 * - Apply generated topologies to a live reservoir
 * - Register and switch between named topology presets
 * - Dynamically adjust spectral radius and connectivity without a full rebuild
 * - Validate topologies before they are committed
 * - Notify listeners when the active configuration changes
 *
 * Based on:
 * - Feature F1.2.3: Reservoir Topology Generator
 * - Echo State Network dynamic reconfiguration (Lukosevicius & Jaeger)
 * - Deep Tree Echo: hierarchical reservoir computing
 *
 * Feature ID: F1.2.3 | Phase: 1.2
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReservoirTopologyGenerator.h"
#include "ReservoirTopologyManager.generated.h"

/**
 * Topology Preset - A named, reusable topology configuration
 */
USTRUCT(BlueprintType)
struct FReservoirTopologyPreset
{
    GENERATED_BODY()

    /** Unique preset name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
    FString Name;

    /** Human-readable description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
    FString Description;

    /** Configuration to apply when this preset is activated */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
    FReservoirTopologyConfig Config;
};

/**
 * Topology Validation Result - Outcome of validating a topology
 */
USTRUCT(BlueprintType)
struct FReservoirTopologyValidation
{
    GENERATED_BODY()

    /** Whether the topology passed validation */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    bool bIsValid = false;

    /** Human-readable reason when validation fails */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    FString FailureReason;

    /** Number of nodes checked */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    int32 NumNodes = 0;

    /** Number of edges checked */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    int32 NumEdges = 0;

    /** Spectral radius measured during validation */
    UPROPERTY(BlueprintReadOnly, Category = "Validation")
    float MeasuredSpectralRadius = 0.0f;
};

/**
 * Dynamic Configuration State - Snapshot of the manager's current state
 */
USTRUCT(BlueprintType)
struct FReservoirTopologyManagerState
{
    GENERATED_BODY()

    /** A topology is currently applied */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bHasActiveTopology = false;

    /** Name of the active preset (empty if ad-hoc) */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FString ActivePresetName;

    /** Number of nodes in the active topology */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    int32 NumNodes = 0;

    /** Number of registered presets */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    int32 NumPresets = 0;

    /** Number of times the configuration has changed */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    int32 ConfigurationChangeCount = 0;
};

/** Broadcast when the active topology configuration changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReservoirTopologyChanged, const FReservoirTopologyManagerState&, NewState);

/**
 * Reservoir Topology Manager Component
 * Dynamically configures and manages reservoir topologies at runtime
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UReservoirTopologyManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UReservoirTopologyManager();

    // ========================================
    // PUBLIC API - CONFIGURATION
    // ========================================

    /** Minimum nodes allowed for a valid topology */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manager|Config", meta = (ClampMin = "2", ClampMax = "10000"))
    int32 MinNodes = 10;

    /** Maximum nodes allowed for a valid topology */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manager|Config", meta = (ClampMin = "2", ClampMax = "10000"))
    int32 MaxNodes = 10000;

    /** Acceptable spectral radius deviation from target (fraction) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manager|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpectralRadiusTolerance = 0.05f;

    /** Minimum allowed spectral radius for an applied topology */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manager|Config", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float MinSpectralRadius = 0.1f;

    /** Maximum allowed spectral radius for an applied topology */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Manager|Config", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float MaxSpectralRadius = 1.5f;

    /** Fired whenever the active topology configuration changes */
    UPROPERTY(BlueprintAssignable, Category = "Manager|Events")
    FOnReservoirTopologyChanged OnTopologyChanged;

    // ========================================
    // PUBLIC API - TOPOLOGY LIFECYCLE
    // ========================================

    /** Generate a topology from a config and apply it as the active topology */
    UFUNCTION(BlueprintCallable, Category = "Manager|Topology")
    bool GenerateAndApply(const FReservoirTopologyConfig& Config);

    /** Apply an already-generated topology as the active topology */
    UFUNCTION(BlueprintCallable, Category = "Manager|Topology")
    bool ApplyTopology(const FReservoirTopologyData& TopologyData);

    /** Clear the active topology */
    UFUNCTION(BlueprintCallable, Category = "Manager|Topology")
    void ClearTopology();

    /** Get the currently active topology data */
    UFUNCTION(BlueprintCallable, Category = "Manager|Topology")
    FReservoirTopologyData GetActiveTopology() const;

    /** Whether a topology is currently applied */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Manager|Topology")
    bool HasActiveTopology() const { return bHasActiveTopology; }

    // ========================================
    // PUBLIC API - DYNAMIC ADJUSTMENT
    // ========================================

    /** Dynamically scale the active topology to a new spectral radius */
    UFUNCTION(BlueprintCallable, Category = "Manager|Dynamics")
    bool AdjustSpectralRadius(float TargetSpectralRadius);

    /** Dynamically scale all weights by a factor (adapts effective connectivity) */
    UFUNCTION(BlueprintCallable, Category = "Manager|Dynamics")
    bool ScaleWeights(float ScaleFactor);

    // ========================================
    // PUBLIC API - PRESETS
    // ========================================

    /** Register a named preset for later activation */
    UFUNCTION(BlueprintCallable, Category = "Manager|Presets")
    void RegisterPreset(const FReservoirTopologyPreset& Preset);

    /** Remove a registered preset by name */
    UFUNCTION(BlueprintCallable, Category = "Manager|Presets")
    bool UnregisterPreset(const FString& Name);

    /** Activate a registered preset (generates and applies it) */
    UFUNCTION(BlueprintCallable, Category = "Manager|Presets")
    bool ActivatePreset(const FString& Name);

    /** Get the names of all registered presets */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Manager|Presets")
    TArray<FString> GetPresetNames() const;

    /** Get a registered preset by name */
    UFUNCTION(BlueprintCallable, Category = "Manager|Presets")
    bool GetPreset(const FString& Name, FReservoirTopologyPreset& OutPreset) const;

    // ========================================
    // PUBLIC API - VALIDATION & STATE
    // ========================================

    /** Validate a topology without applying it */
    UFUNCTION(BlueprintCallable, Category = "Manager|Validation")
    FReservoirTopologyValidation ValidateTopology(const FReservoirTopologyData& TopologyData) const;

    /** Get a snapshot of the manager's current state */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Manager|State")
    FReservoirTopologyManagerState GetManagerState() const;

protected:
    virtual void BeginPlay() override;

private:
    /** Generator used to produce topologies for configs/presets */
    UPROPERTY()
    UReservoirTopologyGenerator* Generator = nullptr;

    /** The currently active topology */
    UPROPERTY()
    FReservoirTopologyData ActiveTopology;

    /** Registered presets keyed by name */
    UPROPERTY()
    TMap<FString, FReservoirTopologyPreset> Presets;

    /** Whether a topology is currently applied */
    bool bHasActiveTopology = false;

    /** Name of the currently active preset (empty if ad-hoc) */
    FString ActivePresetName;

    /** Number of configuration changes applied */
    int32 ConfigurationChangeCount = 0;

    /** Ensure the internal generator exists */
    void EnsureGenerator();

    /** Commit a topology as active and broadcast the change */
    void CommitTopology(const FReservoirTopologyData& TopologyData, const FString& PresetName);
};

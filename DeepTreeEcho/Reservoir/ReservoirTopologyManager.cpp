/**
 * @file ReservoirTopologyManager.cpp
 * @brief Implementation of Reservoir Topology Manager
 *
 * Provides dynamic reservoir configuration: applying generated topologies,
 * switching between named presets, and adjusting spectral radius / weight
 * scaling at runtime without a full reservoir rebuild.
 *
 * Feature ID: F1.2.3 | Phase: 1.2
 */

#include "ReservoirTopologyManager.h"

UReservoirTopologyManager::UReservoirTopologyManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UReservoirTopologyManager::BeginPlay()
{
    Super::BeginPlay();
    EnsureGenerator();
}

// ========================================
// INTERNAL HELPERS
// ========================================

void UReservoirTopologyManager::EnsureGenerator()
{
    if (!Generator)
    {
        Generator = NewObject<UReservoirTopologyGenerator>(this);
    }
}

void UReservoirTopologyManager::CommitTopology(const FReservoirTopologyData& TopologyData, const FString& PresetName)
{
    ActiveTopology = TopologyData;
    ActivePresetName = PresetName;
    bHasActiveTopology = true;
    ConfigurationChangeCount++;

    OnTopologyChanged.Broadcast(GetManagerState());
}

// ========================================
// PUBLIC API - TOPOLOGY LIFECYCLE
// ========================================

bool UReservoirTopologyManager::GenerateAndApply(const FReservoirTopologyConfig& Config)
{
    EnsureGenerator();

    FReservoirTopologyData TopologyData = Generator->GenerateTopology(Config);

    FReservoirTopologyValidation Validation = ValidateTopology(TopologyData);
    if (!Validation.bIsValid)
    {
        return false;
    }

    CommitTopology(TopologyData, FString());
    return true;
}

bool UReservoirTopologyManager::ApplyTopology(const FReservoirTopologyData& TopologyData)
{
    FReservoirTopologyValidation Validation = ValidateTopology(TopologyData);
    if (!Validation.bIsValid)
    {
        return false;
    }

    CommitTopology(TopologyData, FString());
    return true;
}

void UReservoirTopologyManager::ClearTopology()
{
    ActiveTopology = FReservoirTopologyData();
    ActivePresetName.Reset();
    bHasActiveTopology = false;
    ConfigurationChangeCount++;

    OnTopologyChanged.Broadcast(GetManagerState());
}

FReservoirTopologyData UReservoirTopologyManager::GetActiveTopology() const
{
    return ActiveTopology;
}

// ========================================
// PUBLIC API - DYNAMIC ADJUSTMENT
// ========================================

bool UReservoirTopologyManager::AdjustSpectralRadius(float TargetSpectralRadius)
{
    if (!bHasActiveTopology)
    {
        return false;
    }

    if (TargetSpectralRadius < MinSpectralRadius || TargetSpectralRadius > MaxSpectralRadius)
    {
        return false;
    }

    EnsureGenerator();

    ActiveTopology.AdjacencyMatrix = Generator->ScaleToSpectralRadius(
        ActiveTopology.AdjacencyMatrix,
        ActiveTopology.Stats.NumNodes,
        TargetSpectralRadius
    );

    ActiveTopology.Stats.ComputedSpectralRadius = Generator->ComputeSpectralRadius(
        ActiveTopology.AdjacencyMatrix,
        ActiveTopology.Stats.NumNodes
    );
    ActiveTopology.Config.SpectralRadius = TargetSpectralRadius;

    ConfigurationChangeCount++;
    OnTopologyChanged.Broadcast(GetManagerState());
    return true;
}

bool UReservoirTopologyManager::ScaleWeights(float ScaleFactor)
{
    if (!bHasActiveTopology)
    {
        return false;
    }

    if (ScaleFactor <= 0.0f)
    {
        return false;
    }

    for (float& Weight : ActiveTopology.AdjacencyMatrix)
    {
        if (Weight != 0.0f)
        {
            Weight *= ScaleFactor;
        }
    }

    EnsureGenerator();
    ActiveTopology.Stats.ComputedSpectralRadius = Generator->ComputeSpectralRadius(
        ActiveTopology.AdjacencyMatrix,
        ActiveTopology.Stats.NumNodes
    );

    ConfigurationChangeCount++;
    OnTopologyChanged.Broadcast(GetManagerState());
    return true;
}

// ========================================
// PUBLIC API - PRESETS
// ========================================

void UReservoirTopologyManager::RegisterPreset(const FReservoirTopologyPreset& Preset)
{
    if (Preset.Name.IsEmpty())
    {
        return;
    }

    Presets.Add(Preset.Name, Preset);
}

bool UReservoirTopologyManager::UnregisterPreset(const FString& Name)
{
    return Presets.Remove(Name) > 0;
}

bool UReservoirTopologyManager::ActivatePreset(const FString& Name)
{
    const FReservoirTopologyPreset* Preset = Presets.Find(Name);
    if (!Preset)
    {
        return false;
    }

    EnsureGenerator();

    FReservoirTopologyData TopologyData = Generator->GenerateTopology(Preset->Config);

    FReservoirTopologyValidation Validation = ValidateTopology(TopologyData);
    if (!Validation.bIsValid)
    {
        return false;
    }

    CommitTopology(TopologyData, Name);
    return true;
}

TArray<FString> UReservoirTopologyManager::GetPresetNames() const
{
    TArray<FString> Names;
    Names.Reserve(Presets.Num());
    for (const auto& Pair : Presets)
    {
        Names.Add(Pair.Key);
    }
    return Names;
}

bool UReservoirTopologyManager::GetPreset(const FString& Name, FReservoirTopologyPreset& OutPreset) const
{
    const FReservoirTopologyPreset* Preset = Presets.Find(Name);
    if (!Preset)
    {
        return false;
    }

    OutPreset = *Preset;
    return true;
}

// ========================================
// PUBLIC API - VALIDATION & STATE
// ========================================

FReservoirTopologyValidation UReservoirTopologyManager::ValidateTopology(const FReservoirTopologyData& TopologyData) const
{
    FReservoirTopologyValidation Result;
    Result.NumNodes = TopologyData.Stats.NumNodes;
    Result.NumEdges = TopologyData.Stats.NumEdges;
    Result.MeasuredSpectralRadius = TopologyData.Stats.ComputedSpectralRadius;

    const int32 NumNodes = TopologyData.Stats.NumNodes;

    if (NumNodes < MinNodes || NumNodes > MaxNodes)
    {
        Result.FailureReason = FString::Printf(
            TEXT("Node count %d out of range [%d, %d]"), NumNodes, MinNodes, MaxNodes);
        return Result;
    }

    // Adjacency matrix must be a non-empty NxN flattened array
    const int32 ExpectedSize = NumNodes * NumNodes;
    if (TopologyData.AdjacencyMatrix.Num() != ExpectedSize)
    {
        Result.FailureReason = FString::Printf(
            TEXT("Adjacency matrix size %d does not match %d x %d"),
            TopologyData.AdjacencyMatrix.Num(), NumNodes, NumNodes);
        return Result;
    }

    // No self-loops permitted
    for (int32 i = 0; i < NumNodes; i++)
    {
        if (TopologyData.AdjacencyMatrix[i * NumNodes + i] != 0.0f)
        {
            Result.FailureReason = FString::Printf(
                TEXT("Self-loop detected at node %d"), i);
            return Result;
        }
    }

    // Spectral radius must sit inside the configured operating window
    const float SR = TopologyData.Stats.ComputedSpectralRadius;
    if (SR < MinSpectralRadius || SR > MaxSpectralRadius)
    {
        Result.FailureReason = FString::Printf(
            TEXT("Spectral radius %f out of range [%f, %f]"), SR, MinSpectralRadius, MaxSpectralRadius);
        return Result;
    }

    Result.bIsValid = true;
    return Result;
}

FReservoirTopologyManagerState UReservoirTopologyManager::GetManagerState() const
{
    FReservoirTopologyManagerState State;
    State.bHasActiveTopology = bHasActiveTopology;
    State.ActivePresetName = ActivePresetName;
    State.NumNodes = bHasActiveTopology ? ActiveTopology.Stats.NumNodes : 0;
    State.NumPresets = Presets.Num();
    State.ConfigurationChangeCount = ConfigurationChangeCount;
    return State;
}

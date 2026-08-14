# Reservoir Topology Generator - Usage Guide

## Overview

The Reservoir Topology Generator (Feature F1.2.3) provides configurable network topology generation for Echo State Networks in the Deep Tree Echo cognitive architecture. It implements multiple topology types optimized for different cognitive processing tasks.

## Quick Start

### Basic Usage in C++

```cpp
#include "DeepTreeEcho/Reservoir/ReservoirTopologyGenerator.h"

// Create the generator component
UReservoirTopologyGenerator* TopologyGen = NewObject<UReservoirTopologyGenerator>();

// Generate a random sparse topology
FReservoirTopologyData Topology = TopologyGen->GenerateRandomSparse(
    100,      // Number of nodes
    0.1f,     // Connectivity (10%)
    0.9f,     // Spectral radius
    42        // Random seed (0 for random)
);

// Access topology data
UE_LOG(LogTemp, Log, TEXT("Generated %d nodes with %d edges"), 
    Topology.Stats.NumNodes, Topology.Stats.NumEdges);
UE_LOG(LogTemp, Log, TEXT("Spectral radius: %f"), Topology.Stats.ComputedSpectralRadius);
```

### Using with Blueprints

1. Add `ReservoirTopologyGenerator` component to your actor
2. In Blueprint, call `GenerateRandomSparse`, `GenerateSmallWorld`, `GenerateScaleFree`, or `GenerateModular`
3. Access topology statistics through the returned `FReservoirTopologyData` struct
4. Apply topology to reservoir using `SetReservoirWeights`

## Topology Types

### 1. Random Sparse

**Best for:** General-purpose reservoir computing, baseline comparisons

```cpp
FReservoirTopologyData Topology = TopologyGen->GenerateRandomSparse(
    100,    // NumNodes
    0.1f,   // Connectivity
    0.9f,   // SpectralRadius
    42      // Seed
);
```

**Properties:**
- Uniform degree distribution
- Fast generation: O(N²)
- No guaranteed connectivity
- Good for most tasks

**Parameters:**
- `NumNodes`: Number of reservoir units (10-10000)
- `Connectivity`: Edge probability (0.0-1.0)
- `SpectralRadius`: Target spectral radius (0.1-2.0)
- `Seed`: Random seed (0 for random)

### 2. Small-World (Watts-Strogatz)

**Best for:** Temporal pattern recognition, memory consolidation

```cpp
FReservoirTopologyData Topology = TopologyGen->GenerateSmallWorld(
    100,    // NumNodes
    6,      // MeanDegree
    0.1f,   // RewiringProbability
    0.9f,   // SpectralRadius
    42      // Seed
);
```

**Properties:**
- High clustering coefficient
- Short average path length
- Combines local and global connectivity
- Excellent for temporal sequences

**Parameters:**
- `MeanDegree`: Initial degree in ring lattice (2-100)
- `RewiringProbability`: Edge rewiring probability (0.0-1.0)
  - Low (0.01-0.05): More regular structure
  - High (0.5-0.9): More random structure

### 3. Scale-Free (Barabási-Albert)

**Best for:** Hierarchical processing, hub-based routing

```cpp
FReservoirTopologyData Topology = TopologyGen->GenerateScaleFree(
    100,    // NumNodes
    2,      // AttachmentEdges
    0.9f,   // SpectralRadius
    42      // Seed
);
```

**Properties:**
- Power-law degree distribution
- Few highly connected "hub" nodes
- Many low-degree nodes
- Robust to random failures

**Parameters:**
- `AttachmentEdges`: Edges per new node (1-20)
  - Higher values = more connectivity

### 4. Modular

**Best for:** Multi-scale processing, community detection

```cpp
FReservoirTopologyData Topology = TopologyGen->GenerateModular(
    100,    // NumNodes
    4,      // NumModules
    0.1f,   // IntraModuleConnectivity
    0.02f,  // InterModuleConnectivity
    0.9f,   // SpectralRadius
    42      // Seed
);
```

**Properties:**
- Clear community structure
- High modularity score
- Hierarchical organization
- Good for parallel processing

**Parameters:**
- `NumModules`: Number of communities (2-20)
- `IntraModuleConnectivity`: Within-module connection probability (0.0-1.0)
- `InterModuleConnectivity`: Between-module connection probability (0.0-1.0)

## Advanced Usage

### Configurable Generation

```cpp
FReservoirTopologyConfig Config;
Config.TopologyType = EReservoirTopologyType::SmallWorld;
Config.NumNodes = 200;
Config.MeanDegree = 8;
Config.RewiringProbability = 0.15f;
Config.SpectralRadius = 0.95f;
Config.RandomSeed = 123;

FReservoirTopologyData Topology = TopologyGen->GenerateTopology(Config);
```

### Analyzing Topologies

```cpp
// Compute statistics for custom topology
FReservoirTopologyStats Stats = TopologyGen->ComputeTopologyStats(
    CustomAdjacencyMatrix,
    NumNodes
);

UE_LOG(LogTemp, Log, TEXT("Average degree: %f"), Stats.AverageDegree);
UE_LOG(LogTemp, Log, TEXT("Clustering coefficient: %f"), Stats.ClusteringCoefficient);
UE_LOG(LogTemp, Log, TEXT("Average path length: %f"), Stats.AveragePathLength);
```

### Spectral Radius Manipulation

```cpp
// Compute spectral radius
float SR = TopologyGen->ComputeSpectralRadius(AdjacencyMatrix, NumNodes);

// Scale to target spectral radius
TArray<float> ScaledMatrix = TopologyGen->ScaleToSpectralRadius(
    AdjacencyMatrix,
    NumNodes,
    0.9f  // Target spectral radius
);
```

## Visualization and Export

### Export to JSON

```cpp
FString JSON = TopologyGen->ExportToJSON(Topology);

// Save to file
TopologyGen->SaveTopologyToFile(
    Topology,
    TEXT("/Game/Topologies/my_reservoir.json"),
    true  // Use JSON format
);
```

**JSON Structure:**
```json
{
  "topology_type": "small_world",
  "num_nodes": 100,
  "num_edges": 300,
  "connectivity": 0.030303,
  "spectral_radius": 0.899234,
  "clustering_coefficient": 0.542123,
  "generation_time_ms": 45.23,
  "nodes": [
    {"id": 0, "module": 0},
    {"id": 1, "module": 0},
    ...
  ],
  "edges": [
    {"source": 0, "target": 1, "weight": 0.234},
    {"source": 0, "target": 2, "weight": -0.456},
    ...
  ]
}
```

### Export to GraphML

```cpp
FString GraphML = TopologyGen->ExportToGraphML(Topology);

// Save to file
TopologyGen->SaveTopologyToFile(
    Topology,
    TEXT("/Game/Topologies/my_reservoir.graphml"),
    false  // Use GraphML format
);
```

**GraphML** can be imported into:
- Gephi (network visualization)
- Cytoscape (biological networks)
- NetworkX (Python)
- igraph (R/Python)

## Integration with DeepTreeEchoReservoir

### Apply Topology to Reservoir

```cpp
// Generate topology
FReservoirTopologyData Topology = TopologyGen->GenerateSmallWorld(100, 6, 0.1f, 0.9f, 42);

// Get reservoir component
UDeepTreeEchoReservoir* Reservoir = GetOwner()->FindComponentByClass<UDeepTreeEchoReservoir>();

// Apply topology (custom method - to be implemented)
Reservoir->SetInternalWeights(Topology.AdjacencyMatrix, Topology.Stats.NumNodes);
```

### Use with ReservoirCpp

```cpp
// Convert to Eigen sparse matrix
Eigen::SparseMatrix<float> W = UReservoirTopologyGenerator::ArrayToEigenSparse(
    Topology.AdjacencyMatrix,
    Topology.Stats.NumNodes
);

// Use with ReservoirCpp ESN
ReservoirCpp::Reservoir reservoir;
reservoir.setWeights(W);
```

## Performance Considerations

### Generation Time Benchmarks

| Network Size | Random Sparse | Small-World | Scale-Free | Modular |
|--------------|---------------|-------------|------------|---------|
| 100 nodes    | <50ms         | <100ms      | <75ms      | <100ms  |
| 500 nodes    | <500ms        | <1s         | <750ms     | <1s     |
| 1000 nodes   | <2s           | <4s         | <3s        | <4s     |

### Memory Usage

| Network Size | Sparse Matrix | Dense Matrix | Statistics |
|--------------|---------------|--------------|------------|
| 100 nodes    | ~10KB         | ~40KB        | <1KB       |
| 500 nodes    | ~250KB        | ~1MB         | <2KB       |
| 1000 nodes   | ~1MB          | ~4MB         | <5KB       |

## Common Patterns

### Hierarchical Reservoir Architecture

```cpp
// Level 1: Small-world for local processing
FReservoirTopologyData L1 = TopologyGen->GenerateSmallWorld(100, 6, 0.1f, 0.9f, 1);

// Level 2: Scale-free for hierarchical integration
FReservoirTopologyData L2 = TopologyGen->GenerateScaleFree(50, 3, 0.95f, 2);

// Level 3: Modular for high-level abstraction
FReservoirTopologyData L3 = TopologyGen->GenerateModular(30, 3, 0.15f, 0.01f, 0.98f, 3);
```

### Reproducible Experiments

```cpp
// Always use the same seed for reproducibility
const int32 ExperimentSeed = 42;

FReservoirTopologyData Topology1 = TopologyGen->GenerateRandomSparse(100, 0.1f, 0.9f, ExperimentSeed);
FReservoirTopologyData Topology2 = TopologyGen->GenerateRandomSparse(100, 0.1f, 0.9f, ExperimentSeed);

// Topology1 and Topology2 will be identical
```

### Parameter Tuning

```cpp
// Test different connectivity levels
for (float Connectivity = 0.05f; Connectivity <= 0.5f; Connectivity += 0.05f)
{
    FReservoirTopologyData Topology = TopologyGen->GenerateRandomSparse(
        100, Connectivity, 0.9f, 42
    );
    
    // Evaluate performance...
}

// Test different rewiring probabilities for small-world
for (float Beta = 0.0f; Beta <= 1.0f; Beta += 0.1f)
{
    FReservoirTopologyData Topology = TopologyGen->GenerateSmallWorld(
        100, 6, Beta, 0.9f, 42
    );
    
    // Evaluate performance...
}
```

## Troubleshooting

### Issue: Spectral radius differs from target

**Solution:** Spectral radius is computed after generation and may not match exactly. Use `ScaleToSpectralRadius` for precise control:

```cpp
TArray<float> ScaledMatrix = TopologyGen->ScaleToSpectralRadius(
    Topology.AdjacencyMatrix,
    Topology.Stats.NumNodes,
    0.9f  // Exact target
);
```

### Issue: Modular topology has imbalanced modules

**Solution:** This is expected behavior. Nodes are distributed as evenly as possible, but some modules may have one extra node:

```cpp
int32 NodesPerModule = NumNodes / NumModules;  // Base size
int32 Remainder = NumNodes % NumModules;       // Extra nodes
// First 'Remainder' modules get +1 node
```

### Issue: Small-world has isolated nodes

**Solution:** Increase `MeanDegree` or decrease `RewiringProbability`:

```cpp
// Better connectivity
FReservoirTopologyData Topology = TopologyGen->GenerateSmallWorld(
    100,
    8,      // Higher mean degree
    0.05f,  // Lower rewiring
    0.9f,
    42
);
```

## Best Practices

1. **Start with Random Sparse** for baseline performance
2. **Use Small-World** for temporal tasks (speech, time series)
3. **Use Scale-Free** for hierarchical tasks (vision, planning)
4. **Use Modular** for multi-task learning
5. **Always set a seed** for reproducible experiments
6. **Monitor spectral radius** - stay in (0.5, 1.2) range
7. **Tune connectivity** based on task complexity
8. **Export topologies** for offline analysis
9. **Test multiple topologies** before committing to one
10. **Document your topology choice** in code comments

## References

- **Watts-Strogatz:** Collective dynamics of 'small-world' networks (1998)
- **Barabási-Albert:** Emergence of scaling in random networks (1999)
- **Newman-Girvan:** Finding and evaluating community structure in networks (2004)
- **Reservoir Computing:** Lukoševičius & Jaeger, A practical guide to applying echo state networks (2012)

## See Also

- Feature F1.2.1: Echo State Network Integration
- Feature F1.2.2: Reservoir Dynamics
- `/DeepTreeEcho/Reservoir/DeepTreeEchoReservoir.h`
- `/.github/agents/u9ci/reservoir-topology-generator.md`

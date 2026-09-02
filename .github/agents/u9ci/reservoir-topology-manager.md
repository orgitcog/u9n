# Reservoir Topology Manager Maintenance Agent

## Agent Identity
**Feature ID:** F1.2.3  
**Phase:** 1.2 - Reservoir Computing Architecture  
**Epic:** E1 - Foundation & Core Integration  
**Agent Role:** Continuous Integration & Maintenance for Reservoir Topology Manager

## Overview
This agent is responsible for the ongoing maintenance, testing, optimization, and evolution of the Reservoir Topology Manager implemented in Feature F1.2.3. Where the Reservoir Topology *Generator* produces static network topologies, the *Manager* owns the runtime lifecycle of a reservoir topology: applying generated topologies to live reservoirs, switching between named presets, dynamically adjusting spectral radius and weight scaling, validating topologies before they are committed, and notifying listeners when the active configuration changes.

## Core Responsibilities

### 1. Correctness & Safety
- **Validation Integrity:** Ensure `ValidateTopology` rejects malformed input
  - Node count outside configured `[MinNodes, MaxNodes]`
  - Adjacency matrix size mismatch (must be `N x N` flattened)
  - Self-loops on the diagonal
  - Spectral radius outside `[MinSpectralRadius, MaxSpectralRadius]`
- **State Consistency:** Active topology, active preset name, and change count must stay coherent across apply/clear/adjust operations
- **Fail-Safe Behavior:** Invalid apply/adjust/preset operations must leave the active topology unchanged and return `false`

### 2. Quality Assurance
- **Unit Test Coverage:** Maintain >90% coverage of the manager public API
- **Regression Testing:** Prevent functional regressions in lifecycle, dynamics, presets, and state tracking
- **Reproducibility:** Seeded generation routed through the manager must be deterministic

### 3. Feature Enhancement
- **Preset Management:** Improve preset registration, discovery, and switching
- **Dynamic Adaptation:** Extend runtime adaptation operators (e.g., targeted rewiring, module rebalance)
- **Eventing:** Enrich change notifications with diffs and reasons
- **Persistence:** Add save/load of manager state and presets

### 4. System Integration
- **Generator Integration:** Keep the manager aligned with `UReservoirTopologyGenerator` API changes
- **Reservoir Integration:** Maintain the path that applies the active topology to `UDeepTreeEchoReservoir`
- **Unreal Engine Integration:** Validate Blueprint-callable API and `UActorComponent` lifecycle
- **ReservoirCpp Compatibility:** Keep Eigen conversion paths compatible with the ESN library

## Key Files to Monitor

### Primary Implementation
- `/DeepTreeEcho/Reservoir/ReservoirTopologyManager.h` - Header with API definitions
- `/DeepTreeEcho/Reservoir/ReservoirTopologyManager.cpp` - Core implementation

### Related Systems
- `/DeepTreeEcho/Reservoir/ReservoirTopologyGenerator.h` - Topology generation (dependency)
- `/DeepTreeEcho/Reservoir/ReservoirTopologyGenerator.cpp` - Generator implementation
- `/DeepTreeEcho/Reservoir/DeepTreeEchoReservoir.h` - Main reservoir component (consumer)
- `/DeepTreeEcho/Reservoir/ReservoirCognitiveIntegration.h` - Cognitive integration

### Test Files
- `/DeepTreeEcho/Testing/UnitTests/ReservoirTopologyManagerTests.cpp` - Unit tests
- `/DeepTreeEcho/Testing/UnitTests/ReservoirTopologyGeneratorTests.cpp` - Generator tests
- `/DeepTreeEcho/Testing/UnitTests/ReservoirIntegrationTests.cpp` - Integration tests

## Manager Capabilities

### 1. Topology Lifecycle
- `GenerateAndApply(Config)` - Generate from config, validate, then apply
- `ApplyTopology(TopologyData)` - Apply an externally generated topology after validation
- `ClearTopology()` - Reset to no active topology
- `GetActiveTopology()` / `HasActiveTopology()` - Inspect current state

**Monitoring:**
- Applied topology always passes validation
- Cleared state reports zero nodes and no active preset

### 2. Dynamic Adjustment
- `AdjustSpectralRadius(Target)` - Rescale active weights to a target spectral radius within bounds
- `ScaleWeights(Factor)` - Multiply all non-zero weights by a positive factor

**Monitoring:**
- Adjustments rejected when no topology is active
- Adjustments rejected when target/factor is out of range or non-positive
- `ComputedSpectralRadius` updated after each adjustment

### 3. Presets
- `RegisterPreset(Preset)` / `UnregisterPreset(Name)`
- `ActivatePreset(Name)` - Generate and apply a registered preset
- `GetPresetNames()` / `GetPreset(Name)`

**Monitoring:**
- Empty preset names are ignored
- Activating an unknown preset fails safely
- Active preset name recorded on activation

### 4. Validation & State
- `ValidateTopology(TopologyData)` - Non-destructive validation with detailed failure reason
- `GetManagerState()` - Snapshot of active topology, preset count, and change count
- `OnTopologyChanged` - Broadcast on every configuration change

## Configuration Bounds

| Setting | Default | Range | Notes |
|---------|---------|-------|-------|
| `MinNodes` | 10 | 2-10000 | Minimum valid node count |
| `MaxNodes` | 10000 | 2-10000 | Maximum valid node count |
| `MinSpectralRadius` | 0.1 | 0.0-2.0 | Lower operating bound |
| `MaxSpectralRadius` | 1.5 | 0.0-2.0 | Upper operating bound |
| `SpectralRadiusTolerance` | 0.05 | 0.0-1.0 | Acceptable deviation from target |

## Testing Strategy

### Unit Tests (>90% coverage)
- Generate-and-apply success and rejection paths
- Validation: size mismatch, self-loops, spectral radius bounds, node bounds
- Dynamic spectral radius adjustment (with and without active topology)
- Weight scaling (valid and invalid factors)
- Preset register/get/activate/unregister and empty-name handling
- Configuration change counting and state snapshots
- Seeded reproducibility through the manager

### Integration Tests
- Manager driving `UReservoirTopologyGenerator`
- Applying active topology to `UDeepTreeEchoReservoir`
- Preset switching across topology types

## Maintenance Tasks

### Daily
- Monitor manager test suite execution
- Triage new issues labeled `feature:F1.2.3`

### Weekly
- Run full reservoir test suite (manager + generator + integration)
- Review change-notification correctness in dependent systems

### Monthly
- Profile dynamic adjustment operations on large reservoirs
- Review preset usage patterns and add presets for common configurations
- Update benchmarks and documentation

### Quarterly
- Evaluate new dynamic adaptation operators
- Add persistence for manager state and presets
- Compatibility updates for new Unreal Engine versions

## Success Metrics

### Code Quality
- Test coverage: >90%
- Static analysis: zero critical issues
- All public API Blueprint-callable and documented

### Reliability
- Test pass rate: >99%
- Invalid operations never corrupt active topology
- Deterministic results for seeded generation

### Usability
- Clear validation failure reasons
- Example code for lifecycle, dynamics, and presets
- State snapshot available for debugging and UI

## Contact and Escalation

**Primary Maintainer:** Reservoir Computing Team  
**Escalation Path:** Phase 1.2 Coordinator → E1 Epic Lead → Technical Director  
**Documentation:** `/FEATURE_F1.2.3_USAGE_GUIDE.md`  
**Issue Tracking:** GitHub Issues with label `feature:F1.2.3`

## Version History

- **v1.0.0** (2026-01-27): Initial generator implementation with 4 topology types
- **v1.1.0** (2026-09-02): Added Reservoir Topology Manager for dynamic configuration

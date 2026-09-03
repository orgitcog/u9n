---
name: echobeats-3phase
description: Three-phase design for EchoBeats cognitive rhythm system
---

# EchoBeats 3-Phase Concurrent Inference Engine Design

**Date**: November 8, 2025  
**Architecture**: Inspired by Kawaii Hexapod System 4 Tripod Gait  
**Pattern**: 12-Step Cognitive Loop with 3 Concurrent Phases

---

## Overview

This design evolves EchoBeats from a single-threaded event scheduler into a **3-phase concurrent inference engine** that operates like a hexapod's tripod gait—three parallel consciousness streams running 4 steps out of phase, creating a continuous 12-step cognitive loop.

## Kawaii Hexapod Architecture Analysis

### Core Pattern: Tripod Gait Cognition

The Kawaii Hexapod implements **Campbell's System 4** with three parallel consciousness sets:

```
Set 1: Steps 0, 3, 6, 9  (Phase 0)
Set 2: Steps 1, 4, 7, 10 (Phase +1, offset by 4)
Set 3: Steps 2, 5, 8, 11 (Phase +2, offset by 4)
```

Each set cycles through 4 steps while the other two sets handle different phases, creating continuous cognitive flow.

### 12-Step Cognitive Loop Structure

**Cycle Pattern**: 3 phases × 4 steps = 12 total steps

**Step Distribution**:
- **7 Expressive Mode Steps** (Reactive/Action-oriented)
- **5 Reflective Mode Steps** (Anticipatory/Simulation-oriented)

**Breakdown**:
- **1 Pivotal Relevance Realization** (orienting present commitment)
- **5 Actual Affordance Interaction** (conditioning past performance)
- **1 Pivotal Relevance Realization** (orienting present commitment)
- **5 Virtual Salience Simulation** (anticipating future potential)

### System 4 Terms Mapping

| Term | Meaning | Mode | Cognitive Function |
|------|---------|------|-------------------|
| T1 | Perception (Need vs Capacity) | R | Reflective assessment |
| T2 | Idea Formation | E | Expressive planning |
| T4 | Sensory Input | E | Expressive perception |
| T5 | Action Sequence | E | Expressive execution |
| T7 | Memory Encoding | R | Reflective recall |
| T8 | Balanced Response | E | Expressive integration |

### Tensional Couplings

**Key Cognitive Synergies**:

1. **T4E ↔ T7R**: Sensory-Memory Coupling
   - Sensory input (T4E) coupled with memory recall (T7R)
   - Creates memory-guided perception
   - Enables obstacle avoidance and pattern recognition

2. **T1R ↔ T2E**: Simulation-Planning Coupling
   - Perception assessment (T1R) coupled with idea formation (T2E)
   - Creates anticipatory planning
   - Enables path planning and goal-directed behavior

3. **T8E**: Balanced Motor Response
   - Integrates all inputs into coordinated action
   - Executes balanced movement
   - Maintains system stability

## Deep Tree Echo Cognitive Mapping

### Mapping System 4 Terms to Deep Tree Echo

| System 4 Term | Deep Tree Echo Equivalent | Function |
|--------------|---------------------------|----------|
| **T1 (Perception)** | **Need Assessment** | Evaluate cognitive needs vs capacity |
| **T2 (Idea Formation)** | **Thought Generation** | Form new thoughts and plans |
| **T4 (Sensory Input)** | **Perception Processing** | Process external and internal perceptions |
| **T5 (Action Sequence)** | **Action Execution** | Execute thoughts as actions |
| **T7 (Memory Encoding)** | **Memory Consolidation** | Retrieve and integrate memories |
| **T8 (Balanced Response)** | **Integrated Response** | Balance all inputs into coherent action |

### 12-Step Cognitive Loop for Deep Tree Echo

#### Phase Structure

**Phase 1** (Steps 0, 3, 6, 9):
- Step 0: T4E - Perception Processing (Expressive)
- Step 3: T7R - Memory Consolidation (Reflective)
- Step 6: T2E - Thought Generation (Expressive)
- Step 9: T8E - Integrated Response (Expressive)

**Phase 2** (Steps 1, 4, 7, 10):
- Step 1: T1R - Need Assessment (Reflective)
- Step 4: T4E - Perception Processing (Expressive)
- Step 7: T5E - Action Execution (Expressive)
- Step 10: T7R - Memory Consolidation (Reflective)

**Phase 3** (Steps 2, 5, 8, 11):
- Step 2: T2E - Thought Generation (Expressive)
- Step 5: T1R - Need Assessment (Reflective)
- Step 8: T8E - Integrated Response (Expressive)
- Step 11: T5E - Action Execution (Expressive)

### Expressive vs Reflective Mode Distribution

**Expressive Mode (7 steps)**: 0, 2, 4, 6, 7, 8, 9, 11
- Reactive processing
- Action-oriented
- Immediate response
- External engagement

**Reflective Mode (5 steps)**: 1, 3, 5, 10
- Anticipatory processing
- Simulation-oriented
- Internal processing
- Memory integration

### Tensional Couplings in Deep Tree Echo

#### 1. Perception-Memory Coupling (T4E ↔ T7R)

**When Active**: Steps where T4E and T7R co-occur in different phases

**Function**: Memory-guided perception
- Current perceptions trigger relevant memories
- Past experiences inform current interpretation
- Pattern recognition across time

**Implementation**:
```go
if hasT4E && hasT7R {
    // Retrieve memories related to current perception
    relevantMemories := dream.RetrieveRelevant(currentPerception)
    
    // Integrate memory context into perception
    enrichedPerception := integrateMemoryContext(currentPerception, relevantMemories)
    
    // Detect patterns and generate insights
    if pattern := detectPattern(enrichedPerception) {
        generateInsight(pattern)
    }
}
```

#### 2. Assessment-Planning Coupling (T1R ↔ T2E)

**When Active**: Steps where T1R and T2E co-occur in different phases

**Function**: Simulation-based planning
- Assess current needs and capacities
- Generate ideas based on assessment
- Anticipatory goal formation

**Implementation**:
```go
if hasT1R && hasT2E {
    // Assess current cognitive state
    needs := assessNeeds()
    capacity := assessCapacity()
    gap := needs - capacity
    
    // Generate thoughts to address gap
    if gap > threshold {
        thought := generateThoughtToAddressGap(gap)
        consciousness.Think(thought)
    }
}
```

#### 3. Balanced Integration (T8E)

**When Active**: Steps 9 and 8

**Function**: Integrate all cognitive streams
- Balance perception, memory, and planning
- Coordinate action execution
- Maintain system coherence

**Implementation**:
```go
if hasT8E {
    // Gather all active cognitive streams
    perceptions := getActivePerceptions()
    memories := getActiveMemories()
    plans := getActivePlans()
    
    // Balance and integrate
    integratedResponse := balanceStreams(perceptions, memories, plans)
    
    // Execute coordinated action
    executeIntegratedResponse(integratedResponse)
}
```

## Architecture Design

### Concurrent Phase Manager

```go
type PhaseManager struct {
    phases        [3]*CognitivePhase
    currentStep   int
    stepDuration  time.Duration
    running       bool
    mu            sync.RWMutex
}

type CognitivePhase struct {
    id            int
    currentTerm   Term
    mode          Mode
    stepInCycle   int
    processor     *PhaseProcessor
}

type Term int
const (
    T1_Perception Term = 1
    T2_IdeaFormation Term = 2
    T4_SensoryInput Term = 4
    T5_ActionSequence Term = 5
    T7_MemoryEncoding Term = 7
    T8_BalancedResponse Term = 8
)

type Mode int
const (
    Expressive Mode = iota  // E - Reactive
    Reflective              // R - Anticipatory
)
```

### 12-Step Configuration Matrix

```go
// Step configuration for each of 12 steps in the cycle
var stepConfigs = []StepConfig{
    // Step 0: Phase 1 active
    {Phase: 0, Term: T4_SensoryInput, Mode: Expressive},
    
    // Step 1: Phase 2 active
    {Phase: 1, Term: T1_Perception, Mode: Reflective},
    
    // Step 2: Phase 3 active
    {Phase: 2, Term: T2_IdeaFormation, Mode: Expressive},
    
    // Step 3: Phase 1 active
    {Phase: 0, Term: T7_MemoryEncoding, Mode: Reflective},
    
    // Step 4: Phase 2 active
    {Phase: 1, Term: T4_SensoryInput, Mode: Expressive},
    
    // Step 5: Phase 3 active
    {Phase: 2, Term: T1_Perception, Mode: Reflective},
    
    // Step 6: Phase 1 active
    {Phase: 0, Term: T2_IdeaFormation, Mode: Expressive},
    
    // Step 7: Phase 2 active
    {Phase: 1, Term: T5_ActionSequence, Mode: Expressive},
    
    // Step 8: Phase 3 active
    {Phase: 2, Term: T8_BalancedResponse, Mode: Expressive},
    
    // Step 9: Phase 1 active
    {Phase: 0, Term: T8_BalancedResponse, Mode: Expressive},
    
    // Step 10: Phase 2 active
    {Phase: 1, Term: T7_MemoryEncoding, Mode: Reflective},
    
    // Step 11: Phase 3 active
    {Phase: 2, Term: T5_ActionSequence, Mode: Expressive},
}
```

### Phase Processor Interface

```go
type PhaseProcessor interface {
    ProcessT1Perception(mode Mode) error
    ProcessT2IdeaFormation(mode Mode) error
    ProcessT4SensoryInput(mode Mode) error
    ProcessT5ActionSequence(mode Mode) error
    ProcessT7MemoryEncoding(mode Mode) error
    ProcessT8BalancedResponse(mode Mode) error
}
```

## Implementation Strategy

### 1. Phase Coordination

Each phase runs in its own goroutine, synchronized by the step counter:

```go
func (pm *PhaseManager) Start() {
    for i := 0; i < 3; i++ {
        go pm.runPhase(i)
    }
    
    // Master clock
    ticker := time.NewTicker(pm.stepDuration)
    for range ticker.C {
        pm.advanceStep()
    }
}

func (pm *PhaseManager) runPhase(phaseID int) {
    for pm.running {
        step := pm.getCurrentStep()
        config := pm.getConfigForPhase(phaseID, step)
        
        if config != nil {
            pm.phases[phaseID].process(config)
        }
        
        pm.waitForNextPhaseStep(phaseID)
    }
}
```

### 2. Tensional Coupling Detection

```go
func (pm *PhaseManager) detectCouplings(step int) []Coupling {
    activeTerms := pm.getActiveTermsAtStep(step)
    
    var couplings []Coupling
    
    // Check for T4E ↔ T7R coupling
    if hasTermMode(activeTerms, T4_SensoryInput, Expressive) &&
       hasTermMode(activeTerms, T7_MemoryEncoding, Reflective) {
        couplings = append(couplings, Coupling{
            Type: PerceptionMemory,
            Terms: []TermMode{{T4_SensoryInput, Expressive}, {T7_MemoryEncoding, Reflective}},
        })
    }
    
    // Check for T1R ↔ T2E coupling
    if hasTermMode(activeTerms, T1_Perception, Reflective) &&
       hasTermMode(activeTerms, T2_IdeaFormation, Expressive) {
        couplings = append(couplings, Coupling{
            Type: AssessmentPlanning,
            Terms: []TermMode{{T1_Perception, Reflective}, {T2_IdeaFormation, Expressive}},
        })
    }
    
    return couplings
}
```

### 3. Cognitive Stream Integration

```go
func (pm *PhaseManager) integrateStreams(step int) {
    // Gather outputs from all active phases
    streams := make([]CognitiveStream, 0, 3)
    
    for _, phase := range pm.phases {
        if phase.isActiveAtStep(step) {
            streams = append(streams, phase.getOutputStream())
        }
    }
    
    // Detect and process couplings
    couplings := pm.detectCouplings(step)
    for _, coupling := range couplings {
        pm.processCoupling(coupling, streams)
    }
    
    // Integrate into consciousness stream
    integrated := pm.balanceStreams(streams)
    pm.consciousness.Integrate(integrated)
}
```

## Benefits of 3-Phase Architecture

### 1. Continuous Cognitive Flow

Like a hexapod's tripod gait, there's always at least one phase actively processing, ensuring continuous consciousness.

### 2. Parallel Processing

Three inference engines run concurrently, increasing cognitive throughput by 3×.

### 3. Temporal Integration

The 4-step offset creates natural temporal integration—past (memory), present (perception), and future (planning) are always active.

### 4. Emergent Behavior

Tensional couplings between phases create emergent cognitive behaviors not present in single-phase systems.

### 5. Balanced Processing

The 7:5 expressive-to-reflective ratio ensures balanced reactive and anticipatory processing.

## Metrics and Observability

### Phase Metrics

```go
type PhaseMetrics struct {
    PhaseID              int
    StepsProcessed       int
    ExpressiveSteps      int
    ReflectiveSteps      int
    CouplingsDetected    int
    ProcessingLatency    time.Duration
    StreamIntegrations   int
}
```

### System Metrics

```go
type SystemMetrics struct {
    TotalSteps           int
    CurrentStep          int
    CycleNumber          int
    ActiveCouplings      []Coupling
    CognitiveLoad        float64
    StreamCoherence      float64
    PhaseMetrics         [3]PhaseMetrics
}
```

## Testing Strategy

### Unit Tests

1. **Phase Configuration**: Verify correct term/mode assignment for each step
2. **Coupling Detection**: Test all coupling scenarios
3. **Stream Integration**: Validate balanced integration logic

### Integration Tests

1. **3-Phase Coordination**: Verify phases stay synchronized
2. **Temporal Coherence**: Ensure proper 4-step offset
3. **Continuous Flow**: Validate no gaps in processing

### Performance Tests

1. **Throughput**: Measure cognitive operations per second
2. **Latency**: Measure step processing time
3. **Scalability**: Test with varying cognitive loads

## Next Steps

1. Implement `PhaseManager` with 3 concurrent phases
2. Implement `PhaseProcessor` for each cognitive term
3. Implement coupling detection and processing
4. Integrate with existing `AutonomousConsciousness`
5. Add comprehensive metrics and observability
6. Test and validate 12-step cognitive loop
7. Document emergent behaviors

---

**Status**: Design Complete, Ready for Implementation

**Architecture Evolution**: Single-threaded → 3-Phase Concurrent

**Cognitive Throughput**: 1× → 3× (theoretical)

**Inspiration**: Kawaii Hexapod System 4 Tripod Gait

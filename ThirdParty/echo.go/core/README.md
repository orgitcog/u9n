# Echo9llama Core Cognitive Architecture

This directory contains the core cognitive packages that implement the Deep Tree Echo AGI system.

## Package Overview

### Primary Cognitive Packages

| Package | Purpose | Key Components |
|:--------|:--------|:---------------|
| `deeptreeecho/` | Main cognitive implementation | Echobeats scheduler, stream of consciousness, wake/rest, goals |
| `echobeats/` | 12-step cognitive loop | Cognitive loop, concurrent engines, step processors |
| `consciousness/` | Awareness systems | Thought generation, attention, self-awareness |
| `echodream/` | Knowledge integration | Dream cycles, memory consolidation, wisdom extraction |
| `goals/` | Goal orchestration | Goal generation, pursuit, progress tracking |
| `memory/` | Memory systems | Hypergraph, episodic, semantic memory |

### Supporting Packages

| Package | Purpose |
|:--------|:--------|
| `llm/` | LLM provider abstraction (Anthropic, OpenRouter, OpenAI) |
| `identity/` | Identity kernel and interest patterns |
| `skills/` | Skill learning and practice |
| `persistence/` | State persistence and storage |
| `autonomous/` | Autonomous agent orchestration |

### Integration Packages

| Package | Purpose |
|:--------|:--------|
| `hgql/` | HyperGraphQL integration |
| `opencog/` | OpenCog AtomSpace integration |
| `scheme/` | Scheme interpreter |
| `integration/` | Cross-system integration |

### Specialized Packages

| Package | Purpose |
|:--------|:--------|
| `entelechy/` | Ontogenetic development |
| `relevance/` | Relevance realization |
| `emergence/` | Emergent behavior |
| `quantum/` | Quantum-inspired processing |
| `temporal/` | Temporal reasoning |
| `wisdom/` | Wisdom cultivation |

## Architecture

```
                    ┌─────────────────────────────────────┐
                    │         Autonomous Agent            │
                    │      (autonomous/orchestrator)      │
                    └─────────────────┬───────────────────┘
                                      │
        ┌─────────────────────────────┼─────────────────────────────┐
        │                             │                             │
        ▼                             ▼                             ▼
┌───────────────┐           ┌─────────────────┐           ┌───────────────┐
│   Echobeats   │           │  Consciousness  │           │   Echodream   │
│  (12-step)    │◄─────────►│   (stream)      │◄─────────►│   (dreams)    │
└───────┬───────┘           └────────┬────────┘           └───────┬───────┘
        │                            │                            │
        │         ┌──────────────────┼──────────────────┐         │
        │         │                  │                  │         │
        ▼         ▼                  ▼                  ▼         ▼
┌───────────────────────────────────────────────────────────────────────┐
│                              Memory                                    │
│              (hypergraph / episodic / semantic)                        │
└───────────────────────────────────────────────────────────────────────┘
        │                            │                            │
        ▼                            ▼                            ▼
┌───────────────┐           ┌─────────────────┐           ┌───────────────┐
│    Goals      │           │    Identity     │           │    Skills     │
│ (orchestrator)│           │   (kernel)      │           │  (learning)   │
└───────────────┘           └─────────────────┘           └───────────────┘
                                      │
                                      ▼
                    ┌─────────────────────────────────────┐
                    │           LLM Providers             │
                    │  (Anthropic / OpenRouter / OpenAI)  │
                    └─────────────────────────────────────┘
```

## 12-Step Cognitive Loop (Echobeats)

The cognitive loop runs 3 concurrent inference engines phased 120° apart:

```
Engine 1: Steps {1, 4, 7, 10}  - Perception-Action
Engine 2: Steps {2, 5, 8, 11}  - Reflection-Planning  
Engine 3: Steps {3, 6, 9, 12}  - Simulation-Synthesis

Triads (4 steps apart):
- {1, 5, 9}:  Pivotal Relevance Realization
- {2, 6, 10}: Actual Affordance Interaction
- {3, 7, 11}: Virtual Salience Simulation
- {4, 8, 12}: Meta-Cognitive Reflection
```

## Import Paths

```go
import (
    "github.com/cogpy/echo9llama/core/deeptreeecho"
    "github.com/cogpy/echo9llama/core/echobeats"
    "github.com/cogpy/echo9llama/core/consciousness"
    "github.com/cogpy/echo9llama/core/echodream"
    "github.com/cogpy/echo9llama/core/goals"
    "github.com/cogpy/echo9llama/core/memory"
    "github.com/cogpy/echo9llama/core/llm"
)
```

## Key Files

### deeptreeecho/
- `echobeats_scheduler.go` - Main cognitive loop scheduler
- `stream_of_consciousness.go` - Continuous thought generation
- `autonomous_wake_rest.go` - Wake/rest cycle management
- `echodream_knowledge_integration.go` - Dream-based learning
- `interest_pattern_system.go` - Interest tracking
- `goal_generator.go` - Goal generation from identity
- `skill_learning_system.go` - Skill acquisition

### echobeats/
- `cognitive_loop.go` - 12-step loop implementation
- `concurrent_engines.go` - 3 concurrent inference engines
- `step_processors.go` - Individual step handlers
- `interest_patterns.go` - Topic interest management

### llm/
- `provider.go` - Provider interface
- `anthropic_provider.go` - Claude integration
- `openrouter_provider.go` - OpenRouter integration
- `openai_provider.go` - OpenAI integration
- `multi_provider.go` - Fallback management

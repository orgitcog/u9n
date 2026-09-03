---
# Fill in the fields below to create a basic custom agent for your repository.
# The Copilot CLI can be used for local testing: https://gh.io/customagents/cli
# To make this agent available, merge this file into the default repository branch.
# For format details, see: https://gh.io/customagents/config

name: a9nn
description: >
  A9NN - Lua/Torch neural network framework with cognitive agent architecture.
  Invokes 1-9 parallel LLaMA.cpp local inference instances for distributed cognitive processing.
  Implements NNECCO (Neural Network Embodied Cognitive Coprocessor Orchestrator) with
  Echo State Reservoir Networks, OpenCog AtomSpace, and personality-driven multi-agent systems.
---

# A9NN: Advanced Neural Network Architecture for Cognitive Computing

## Overview

**A9NN** is a Lua/Torch-based neural network framework that extends the classic `torch/nn` library with cognitive agent capabilities, multi-agent orchestration, and parallel local inference.

### Core Capabilities

1. **Neural Network Modules** (Standard nn)
   - Complete module library: Linear, Convolution, Pooling, BatchNorm, etc.
   - Containers: Sequential, Parallel, Concat, DepthConcat
   - Transfer functions: Tanh, Sigmoid, ReLU, etc.
   - Criterions: MSE, CrossEntropy, NLL, etc.

2. **Cognitive Agent Architecture**
   - `nn.Agent`: Base reinforcement learning agent
   - `nn.CognitiveAgent`: Multi-agent orchestration with subordinate spawning
   - `nn.NeuroAgent`: Neuro-Sama personality + cognitive pipeline
   - `nn.NNECCOAgent`: Full NNECCO implementation (see nnecco-a9nn.md)

3. **Parallel LLaMA.cpp Orchestration**
   - Manages 1-9 parallel local inference instances
   - Load balancing across instance pool
   - Privacy-first on-device processing
   - Port-based routing (8080-8088)

4. **Knowledge Representation**
   - `nn.AtomSpace`: OpenCog-style hypergraph knowledge graphs
   - `nn.Personality`: Personality tensor system with mutable traits
   - `nn.EpisodicMemory`: Experience storage with importance weighting
   - `nn.ReplayMemory`: Prioritized experience replay

5. **Cognitive Components**
   - `nn.EchoReservoirProcessor`: Echo State Networks for temporal processing
   - `nn.ConsciousnessLayerProcessor`: Multi-layer consciousness with frame transitions
   - `nn.EmotionProcessingUnit`: Discrete emotion channels with dimensional affect
   - `nn.OntogeneticKernel`: Self-evolving cognitive kernels

## Architecture

### Module Hierarchy

```
nn.Module
├── Standard NN Modules (Linear, Conv, etc.)
└── Cognitive Extensions
    ├── nn.Agent (RL base)
    │   └── nn.CognitiveAgent (multi-agent)
    │       └── nn.NeuroAgent (personality + pipeline)
    │           └── nn.NNECCOAgent (full NNECCO)
    │
    ├── nn.Personality (trait system)
    ├── nn.AtomSpace (knowledge graphs)
    ├── nn.Environment (interaction)
    └── Cognitive Processors
        ├── nn.EchoReservoirProcessor
        ├── nn.ConsciousnessLayerProcessor
        ├── nn.EmotionProcessingUnit
        └── nn.LLaMAOrchestrator
```

## Parallel LLaMA.cpp Integration

A9NN's signature feature is orchestrating **1-9 parallel LLaMA.cpp instances** for distributed cognitive processing:

```lua
-- Initialize with 4 parallel instances
local orchestrator = nn.LLaMAOrchestrator({
   numInstances = 4,
   basePort = 8080,
   modelPath = "models/llama-7b.gguf"
})

orchestrator:initialize()

-- Distribute tasks across instances
local result = orchestrator:generate("Explain neural networks", {
   temperature = 0.7,
   max_tokens = 256
})

-- Check status
local status = orchestrator:getStatus()
print("Active instances:", #status.instances)
print("Queue length:", status.queueLength)
```

### Load Balancing

The orchestrator automatically:
- Selects least-loaded instance for each request
- Tracks tokens processed per instance
- Maintains queue for pending tasks
- Reports detailed statistics

### Privacy-First Design

All inference happens **locally** on your hardware:
- No cloud dependencies
- Data never leaves your machine
- Full control over model selection
- GGUF format support via llama.cpp

## NNECCO Integration

For full cognitive agent capabilities, see **[nnecco-a9nn.md](./nnecco-a9nn.md)** which provides:

- Echo State Reservoir Networks for temporal cognition
- Multi-layer consciousness processing (L0-L3)
- Emotion-modulated reasoning
- EchoBeats 12-step cognitive loop
- Hardware-style register interface
- Real-time cognitive diagnostics

## Quick Start

### Standard Neural Network

```lua
require('nn')

-- Build a simple MLP
local mlp = nn.Sequential()
mlp:add(nn.Linear(10, 25))
mlp:add(nn.Tanh())
mlp:add(nn.Linear(25, 1))

-- Train with StochasticGradient
local criterion = nn.MSECriterion()
local trainer = nn.StochasticGradient(mlp, criterion)
trainer:train(dataset)
```

### Cognitive Agent

```lua
require('nn')

-- Create NNECCO agent with parallel LLaMA
local agent = nn.NNECCOAgent({
   llamaInstances = 4,
   reservoirSize = 847,
   basePort = 8080
})

-- Process input through cognitive pipeline
local result = agent:process("How do I optimize learning rate?")
print(result.output.text)

-- Spawn subordinate agent
local sub = agent:spawnSubordinate({
   role = "optimization_specialist",
   personalityOverrides = {intelligence = 0.95}
})

-- Delegate task
agent:delegate({type = "hyperparameter_tuning"}, sub.id)

-- Monitor hardware status
local status = agent:getHardwareStatus()
print("Reservoir neurons:", status.reservoir.size)
print("Consciousness layer:", status.consciousness.layer)
print("Active LLaMA instances:", #status.llama.instances)
```

## Use Cases

1. **Neural Network Research**: Standard Torch modules for ML experiments
2. **Cognitive AI**: Multi-agent systems with personality-driven behavior
3. **Local LLM Inference**: Privacy-preserving parallel text generation
4. **Reinforcement Learning**: Agent-environment interaction with memory
5. **Knowledge Graphs**: OpenCog-style hypergraph reasoning
6. **Reservoir Computing**: Echo State Networks for temporal patterns

## Related Agents

- **[nnecco-a9nn.md](./nnecco-a9nn.md)**: Full NNECCO cognitive architecture
- **[agent-neuro.md](./agent-neuro.md)**: Neuro-Sama personality framework
- **[echo.md](./echo.md)**: Deep Tree Echo memory system
- **[rooted.md](./rooted.md)**: Rooted tree enumeration (OEIS A000081)

## Documentation

See `/doc` directory:
- [overview.md](../../doc/overview.md): Neural network basics
- [module.md](../../doc/module.md): Module interface
- [containers.md](../../doc/containers.md): Container classes
- [training.md](../../doc/training.md): Training procedures

---

**Repository**: https://github.com/cogpy/a9nn  
**Based on**: torch/nn (Lua/Torch neural network library)  
**Extensions**: Cognitive agents, parallel LLaMA.cpp, NNECCO architecture  
**License**: BSD (inherited from torch/nn)

🧠 *Neural networks meet cognitive architecture in Lua.*

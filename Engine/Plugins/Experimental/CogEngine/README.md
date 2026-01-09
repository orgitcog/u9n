# CogEngine - Cognitive Multi-Agent Orchestration for Unreal Engine

> 📚 **[Documentation Index](DOCUMENTATION_INDEX.md)** | 📊 **[System Diagrams](SYSTEM_DIAGRAMS.md)** | 🚀 **[Deployment Architecture](DEPLOYMENT_ARCHITECTURE.md)** | ⚡ **[Quick Start](QUICKSTART.md)** | 🎭 **[Live2D Pyper vs Neuro Fusion](LIVE2D_PYPER_NEURO_FUSION.md)**

## Overview

CogEngine is an experimental autonomous multi-agent orchestration workbench for Unreal Engine that integrates multiple cutting-edge cognitive AI frameworks and systems.

## Core Components

### 1. Virtual Environment Space - Cognitive Laboratory & Robotics Lab
- **AVirtualEnvironment**: 3D virtual space for cognitive agents (similar to aiangel.io)
- **UCognitiveLaboratory**: Unified integration of all cognitive subsystems
- **AMardukRoboticsLab**: Enhanced robotics laboratory for physical-virtual hybrid experiments
- Spatial agent management with bounds checking
- Real-time cognitive orchestration in 3D space
- ROS and IoT device integration for real-world interaction

### 2. OpenCog Core - Autonomous Multi-Agent Orchestration
- **UOpenCogAgent**: Individual autonomous cognitive agents with goals, beliefs, and actions
- **UOpenCogOrchestrator**: Central coordination system for multi-agent orchestration
- Supports distributed agent coordination and resource allocation
- Scales to thousands of concurrent agents

### 3. Agent-Zero Kernel - Distributed Cognitive Microkernel Framework
- **UAgentZeroMicrokernel**: Lightweight distributed cognitive processing units
- Minimal overhead cognitive operations for maximum efficiency
- Network-based task distribution across nodes
- Designed for distributed cognitive workloads

### 4. KoboldAI Autognostic Storyweaver - World-Building Coherence
- **UAutognosticStoryweaver**: Maintains narrative coherence across massive player base
- Supports billion-player-specific character arcs
- Dynamic alignment with world-building core
- Real-time coherence maintenance and conflict resolution

### 5. Deep-Tree-Echo - Embodied Echo-State Cognition
- **UEchoStateNetwork**: Hierarchical echo-state networks for deep cognitive processing
- Tree-structured reservoir computing
- Embodied cognition through dynamic state evolution
- Spectral radius control for stable dynamics

### 6. Aphrodite-Engine - LLM Inference Over Network
- **ULLMInferenceEngine**: Distributed large language model inference
- Network-based LLM API integration
- Support for multiple model backends (Llama, Mistral, Mixtral, etc.)
- Asynchronous request/response handling

### 7. Marduk's-Lab - ROS and Home-Assistant IoT Framework
- **UROSBridge**: Robot Operating System integration
- **UHomeAssistantBridge**: IoT device coordination via Home Assistant
- Real-world robotics integration
- Hybrid IoT framework for physical-virtual interaction

### 8. Live2D Cubism SDK Integration
- **UCubismModel**: Live2D character rendering and animation component
- 2D character animation system
- Real-time parameter manipulation
- Blueprint-accessible Live2D integration

### 9. Pyper Character System - Deep Tree Echo Mascot
- **APyperActor**: PygmalionAI mascot powered by Echo State Networks
- Full personality trait system (Philosophical, Playful, Mysterious, etc.)
- Cognitive state tracking and processing
- Tensor signature computation and gestalt processing
- Prime factor resonance patterns
- Visual-cognitive state synchronization

### 10. Neuro Fusion - Superhot Battle System
- **ANeuroEntity**: Alternative AI competitor with configurable types
- **ASuperhotFusionBattle**: Battle system with time dilation mechanics
- Superhot-style slow-motion during intense moments
- Fusion mechanics when entities are evenly matched
- Round-based cognitive combat
- Dynamic scoring based on cognitive abilities
- **UHomeAssistantBridge**: IoT device coordination via Home Assistant
- Real-world robotics integration
- Hybrid IoT framework for physical-virtual interaction

## Installation

1. Copy the `CogEngine` plugin folder to `Engine/Plugins/Experimental/`
2. Regenerate project files
3. Build the engine
4. Enable the plugin in your project's .uproject or through the Plugins menu

## Usage

### Virtual Environment Space

```cpp
// Create a virtual environment for cognitive agents
AVirtualEnvironment* VirtualEnv = GetWorld()->SpawnActor<AVirtualEnvironment>();
VirtualEnv->EnvironmentName = TEXT("Cognitive Lab 01");
VirtualEnv->EnvironmentSize = FVector(10000.0f, 10000.0f, 5000.0f);
VirtualEnv->InitializeEnvironment();

// Add agents to the environment
UOpenCogAgent* Agent = NewObject<UOpenCogAgent>();
Agent->InitializeAgent(TEXT("Agent001"));
FVector SpawnLocation = VirtualEnv->GetRandomLocation();
VirtualEnv->AddAgent(Agent, SpawnLocation);
```

### Cognitive Laboratory Integration

```cpp
// Create cognitive laboratory component
UCognitiveLaboratory* CogLab = CreateDefaultSubobject<UCognitiveLaboratory>(TEXT("CogLab"));
CogLab->LaboratoryName = TEXT("OpenCog Research Lab");
CogLab->InitializeLaboratory();

// Enable specific subsystems
CogLab->SetSubsystemEnabled(TEXT("Orchestrator"), true);
CogLab->SetSubsystemEnabled(TEXT("LLMEngine"), true);
CogLab->SetSubsystemEnabled(TEXT("RoboticsInterface"), true);

// Process cognitive cycle
CogLab->ProcessCognitiveCycle(DeltaTime);
```

### Marduk's Robotics Lab

```cpp
// Create robotics laboratory
AMardukRoboticsLab* RoboticsLab = GetWorld()->SpawnActor<AMardukRoboticsLab>();
RoboticsLab->LabName = TEXT("Autonomous Robotics Lab");
RoboticsLab->ROSMasterURI = TEXT("http://localhost:11311");
RoboticsLab->InitializeLab();

// Connect and control robots
RoboticsLab->ConnectToROS();
RoboticsLab->RegisterRobot(TEXT("TurtleBot3_01"));
RoboticsLab->SubscribeToTopic(TEXT("/camera/image"), TEXT("sensor_msgs/Image"));

// Connect and control IoT devices
RoboticsLab->ConnectToHomeAssistant();
RoboticsLab->ControlDevice(TEXT("light.lab_overhead"), TEXT("turn_on"));
```

### OpenCog Multi-Agent System

```cpp
// Create orchestrator
UOpenCogOrchestrator* Orchestrator = NewObject<UOpenCogOrchestrator>();
Orchestrator->Initialize();

// Create and register agents
UOpenCogAgent* Agent = NewObject<UOpenCogAgent>();
Agent->InitializeAgent("Agent001");
Agent->AddGoal("ExploreEnvironment");
Orchestrator->RegisterAgent(Agent);

// Run orchestration loop
Orchestrator->OrchestrateAgents(DeltaTime);
```

### Agent-Zero Distributed Computing

```cpp
// Initialize microkernel
UAgentZeroMicrokernel* Kernel = NewObject<UAgentZeroMicrokernel>();
Kernel->Initialize("Kernel001");
Kernel->JoinNetwork("ws://localhost:9000");

// Process cognitive tasks
float Result;
Kernel->ProcessCognitiveTask("TaskData", Result);
```

### KoboldAI Storyweaver

```cpp
// Initialize storyweaver
UAutognosticStoryweaver* Storyweaver = NewObject<UAutognosticStoryweaver>();
Storyweaver->Initialize();

// Create player story arc
Storyweaver->CreateStoryArc("Player123", "Arc001");
Storyweaver->UpdateArcProgression("Arc001", "QuestCompleted");
Storyweaver->MaintainGlobalCoherence();
```

### Deep-Tree-Echo Network

```cpp
// Initialize echo-state network
UEchoStateNetwork* Network = NewObject<UEchoStateNetwork>();
Network->InitializeNetwork(5, 100); // Depth 5, Reservoir size 100

// Process inputs
TArray<float> Input = {1.0f, 0.5f, 0.3f};
TArray<float> Output = Network->ProcessInput(Input);
```

### Aphrodite LLM Inference

```cpp
// Initialize inference engine
ULLMInferenceEngine* Engine = NewObject<ULLMInferenceEngine>();
Engine->Initialize("http://localhost:2242", "api_key");

// Submit inference request
FLLMInferenceRequest Request;
Request.RequestID = "Req001";
Request.Prompt = "Explain quantum computing";
Request.MaxTokens = 256;
Engine->SubmitInferenceRequest(Request);
```

### Marduk's-Lab ROS Integration

```cpp
// Initialize ROS bridge
UROSBridge* ROSBridge = NewObject<UROSBridge>();
ROSBridge->Initialize("http://localhost:11311", 9090);
ROSBridge->Connect();
ROSBridge->SubscribeTopic("/camera/image", "sensor_msgs/Image");

// Initialize Home Assistant
UHomeAssistantBridge* HABridge = NewObject<UHomeAssistantBridge>();
HABridge->Initialize("http://localhost:8123", "access_token");
HABridge->Connect();
HABridge->CallService("light", "turn_on", "light.living_room");
```

## Configuration

Plugin settings can be configured in `Config/DefaultCogEngine.ini`

## Requirements

- Unreal Engine 5.4 or later
- C++17 or later
- For ROS integration: ROS Noetic or ROS 2
- For Home Assistant: Home Assistant instance with API access
- For LLM inference: Compatible LLM inference server

## Architecture

The plugin follows a modular architecture:
- Each subsystem is implemented as a separate module
- Main CogEngine module orchestrates all subsystems
- Editor module provides Unreal Editor integration
- All systems are Blueprint-accessible for ease of use

See [ARCHITECTURE.md](ARCHITECTURE.md) for a detailed text-based architecture diagram.

## System Diagrams

For comprehensive visual diagrams showing how the various systems are integrated:

- **[System Integration Diagrams](SYSTEM_DIAGRAMS.md)** - Mermaid diagrams showing:
  - Overall System Architecture
  - Component Relationships (Class Diagrams)
  - Data Flow Diagrams
  - Cognitive Cycle Sequences
  - Virtual Environment Integration
  - Multi-Agent Orchestration
  - External System Integration
  - Future Architecture: Arc-Halo, Aphrodite, Yggdrasil (AAA Integration)

- **[Deployment Architecture](DEPLOYMENT_ARCHITECTURE.md)** - Deployment and operational diagrams:
  - Physical Deployment Architecture
  - Network Communication Patterns
  - Module Dependencies
  - Configuration Flow
  - Scaling Architecture (Current and Future with AAA)
  - Network Topology
  - Performance Monitoring

## Experimental Status

This is an experimental plugin under active development. APIs may change between versions.

## License

Copyright Epic Games, Inc. All Rights Reserved.

## Support

For issues and questions, please refer to the Unreal Engine documentation or community forums.

# CogEngine Documentation Index

Welcome to the CogEngine documentation. This index provides a structured guide to all available documentation.

## 📚 Quick Start

**New to CogEngine?** Start here:
- [README.md](README.md) - Main plugin overview and usage guide
- [QUICKSTART.md](QUICKSTART.md) - Step-by-step quick start guide for getting up and running

## 🏗️ Architecture & Design

**Understanding the system:**
- [ARCHITECTURE.md](ARCHITECTURE.md) - Text-based architecture diagram and system overview
- [SYSTEM_DIAGRAMS.md](SYSTEM_DIAGRAMS.md) - **NEW!** Comprehensive Mermaid diagrams showing:
  - Overall System Architecture
  - Component Relationships (Class Diagrams)
  - Data Flow Diagrams
  - Cognitive Cycle Sequences
  - Virtual Environment Integration
  - Multi-Agent Orchestration
  - External System Integration
  - **Future Architecture: Arc-Halo, Aphrodite, Yggdrasil Integration**

## 🚀 Deployment & Operations

**Setting up and running CogEngine:**
- [DEPLOYMENT_ARCHITECTURE.md](DEPLOYMENT_ARCHITECTURE.md) - **NEW!** Deployment diagrams covering:
  - Physical Deployment Architecture
  - Network Communication Patterns
  - Module Dependencies
  - Configuration Flow
  - Scaling Architecture (Current and Future with AAA)
  - Network Topology
  - Performance Monitoring

## 📖 Usage & API

**Working with CogEngine:**
- [USAGE.md](USAGE.md) - Comprehensive usage examples for all subsystems
- [VIRTUAL_ENVIRONMENT_SPACE.md](VIRTUAL_ENVIRONMENT_SPACE.md) - Virtual environment and cognitive laboratory documentation

## 📊 Implementation Details

**For developers and contributors:**
- [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) - Complete implementation summary with file statistics

## 🔧 Core Components

### 1. Virtual Environment Space
The virtual 3D cognitive laboratory (similar to aiangel.io):
- **AVirtualEnvironment** - 3D spatial environment for cognitive agents
- **UCognitiveLaboratory** - Unified integration of all cognitive subsystems
- **AMardukRoboticsLab** - Robotics and IoT integration laboratory

See: [VIRTUAL_ENVIRONMENT_SPACE.md](VIRTUAL_ENVIRONMENT_SPACE.md)

### 2. OpenCog Core
Autonomous multi-agent orchestration:
- **UOpenCogOrchestrator** - Central coordination for 1000+ concurrent agents
- **UOpenCogAgent** - Individual cognitive agents with goals, beliefs, and actions

### 3. Agent-Zero Kernel
Distributed cognitive microkernel framework:
- **UAgentZeroMicrokernel** - Lightweight distributed cognitive processing units
- Network-based task distribution

### 4. KoboldAI Autognostic Storyweaver
World-building coherence system:
- **UAutognosticStoryweaver** - Narrative coherence for billion-scale player arcs
- Dynamic alignment with world-building core

### 5. Deep-Tree-Echo
Embodied echo-state cognition:
- **UEchoStateNetwork** - Hierarchical echo-state networks
- Tree-structured reservoir computing

### 6. Aphrodite Engine
LLM inference over network:
- **ULLMInferenceEngine** - Distributed large language model inference
- Support for multiple model backends (Llama, Mistral, Mixtral, etc.)

### 7. Marduk's Lab
ROS and Home Assistant IoT framework:
- **UROSBridge** - Robot Operating System integration
- **UHomeAssistantBridge** - IoT device coordination

## 🔮 Future Systems (Planned)

### Arc-Halo-Aphrodite-Yggdrasil (AAA) Integration

The next evolution of CogEngine will integrate the AAA framework:

#### Arc-Halo (Agent Layer)
- **Cognitive Fusion Reactor** - Unified cognitive processing core
- Multi-modal fusion of sensory inputs
- Context-aware reasoning

#### Aphrodite (Relation Layer)
- **Hypergraph Inference Engine** - Advanced knowledge representation
- Relational reasoning and inference
- Pattern-based learning and adaptation
- *Note: Current "Aphrodite Engine" is LLM inference; future "Aphrodite" is hypergraph*

#### Yggdrasil (Arena Layer)
- **Distributed Atom Space** - Persistent, distributed knowledge base
- Cross-agent knowledge sharing
- Scalable to planetary-scale agent networks

See detailed diagrams in [SYSTEM_DIAGRAMS.md](SYSTEM_DIAGRAMS.md#8-future-architecture-arc-halo-aphrodite-yggdrasil)

## 📈 System Metrics

- **Total Modules:** 9 (8 runtime + 1 editor)
- **Total Files:** 52+ implementation files
- **Lines of Code:** ~4,800 LOC
- **Max Agents:** 1,000+ concurrent
- **Story Arcs:** 1 Billion+ (billion scale)
- **Update Frequency:** 30 Hz (configurable)
- **Blueprint Support:** 100% (all major classes)

## 🔗 External Integrations

CogEngine integrates with:
- **LLM Inference Servers** (localhost:2242) - Llama, Mistral, Mixtral, CodeLlama
- **ROS Master** (localhost:11311) - Robot Operating System
- **Home Assistant** (localhost:8123) - IoT devices and smart home automation

## 📝 Configuration

Plugin settings: `Config/DefaultCogEngine.ini`

Key configuration areas:
- Virtual environment size and limits
- Agent orchestration parameters
- Network endpoints (LLM, ROS, Home Assistant)
- Echo network depth and reservoir size
- Story arc thresholds
- Update frequencies

## 🛠️ Development

### Requirements
- Unreal Engine 5.4 or later
- C++17 or later
- Optional: ROS Noetic or ROS 2 for robotics integration
- Optional: Home Assistant instance for IoT integration
- Optional: Compatible LLM inference server

### Installation
1. Copy the `CogEngine` plugin folder to `Engine/Plugins/Experimental/`
2. Regenerate project files
3. Build the engine
4. Enable the plugin in your project

## 🎯 Use Cases

CogEngine is suitable for:
- Multi-agent simulations
- Autonomous NPC behavior systems
- Interactive narrative experiences
- Robotics-virtual world integration
- IoT-enabled game environments
- Distributed cognitive processing research
- Large-scale story generation systems

## 📄 License

Copyright Epic Games, Inc. All Rights Reserved.

## 🆘 Support

For issues and questions:
- Review this documentation
- Check the Unreal Engine documentation
- Visit: https://github.com/cogpy/UnrealCogEngine

---

## Document Navigation Map

```
CogEngine Documentation
│
├── Getting Started
│   ├── README.md (Start here)
│   └── QUICKSTART.md (Step-by-step guide)
│
├── Architecture & Design
│   ├── ARCHITECTURE.md (Text overview)
│   ├── SYSTEM_DIAGRAMS.md (Visual Mermaid diagrams) ⭐ NEW
│   └── DEPLOYMENT_ARCHITECTURE.md (Deployment diagrams) ⭐ NEW
│
├── Usage & API
│   ├── USAGE.md (Comprehensive examples)
│   └── VIRTUAL_ENVIRONMENT_SPACE.md (Virtual env details)
│
└── Implementation
    └── IMPLEMENTATION_SUMMARY.md (Technical details)
```

---

*Last Updated: 2025-11-03*
*CogEngine Version: Experimental*
*Documentation Status: Complete with Mermaid Diagrams*

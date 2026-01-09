# Virtual Environment Space - OpenCog Cognitive Laboratory

## Overview

The Virtual Environment Space module provides a 3D virtual environment similar to [aiangel.io](https://www.aiangel.io/) where various OpenCog subsystems can be integrated into a unified cognitive laboratory and Marduk's Robotics Lab. This module enables autonomous orchestration of cognitive agents in a spatially-aware virtual world.

## Key Features

### 1. Virtual Environment Actor (AVirtualEnvironment)

A 3D spatial environment where cognitive agents can operate with full spatial awareness:

- **Bounded 3D Space**: Configurable environment size with automatic bounds checking
- **Agent Spatial Management**: Agents positioned and tracked in 3D space
- **OpenCog Integration**: Seamless integration with OpenCog orchestrator
- **Real-time Updates**: Continuous cognitive cycle processing at configurable rates

### 2. Cognitive Laboratory Component (UCognitiveLaboratory)

Unified integration point for all cognitive subsystems:

- **Multi-Subsystem Coordination**: Manages all 7 cognitive subsystems in one place
- **Dynamic Enable/Disable**: Control individual subsystems at runtime
- **Status Monitoring**: Real-time status reporting for all subsystems
- **Cognitive Cycle Processing**: Coordinated processing across all active systems

### 3. Marduk's Robotics Lab Actor (AMardukRoboticsLab)

Enhanced physical-virtual hybrid robotics laboratory:

- **ROS Integration**: Connect to ROS master and subscribe/publish to topics
- **IoT Device Control**: Control Home Assistant devices from within Unreal
- **Robot Registration**: Manage fleet of connected robots
- **Agent-Robot Coordination**: Link cognitive agents with physical/simulated robots

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                   Virtual Environment Space                      │
│                                                                  │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────┐  │
│  │  Virtual         │  │  Cognitive       │  │  Marduk's    │  │
│  │  Environment     │  │  Laboratory      │  │  Robotics    │  │
│  │                  │  │                  │  │  Lab         │  │
│  │ • 3D Space       │  │ • Orchestrator   │  │ • ROS        │  │
│  │ • Agent Mgmt     │  │ • Echo Network   │  │ • IoT        │  │
│  │ • Bounds Check   │  │ • Storyweaver    │  │ • Robots     │  │
│  │ • Spatial Track  │  │ • LLM Engine     │  │ • Devices    │  │
│  └──────────────────┘  └──────────────────┘  └──────────────┘  │
│           │                     │                      │         │
│           └─────────────────────┴──────────────────────┘         │
│                              │                                   │
└──────────────────────────────┼───────────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
            ┌───────▼────────┐    ┌──────▼────────┐
            │  OpenCog Core  │    │  MardukLab    │
            │  Subsystems    │    │  Integration  │
            └────────────────┘    └───────────────┘
```

## Usage Examples

### Creating a Virtual Environment

```cpp
// Spawn a virtual environment in your level
AVirtualEnvironment* VirtualEnv = GetWorld()->SpawnActor<AVirtualEnvironment>();
VirtualEnv->EnvironmentName = TEXT("My Cognitive Lab");
VirtualEnv->EnvironmentSize = FVector(10000.0f, 10000.0f, 5000.0f);
VirtualEnv->InitializeEnvironment();

// Add cognitive agents to the environment
UOpenCogAgent* Agent = NewObject<UOpenCogAgent>();
Agent->InitializeAgent(TEXT("Agent001"));
FVector SpawnLocation = VirtualEnv->GetRandomLocation();
VirtualEnv->AddAgent(Agent, SpawnLocation);
```

### Setting Up a Cognitive Laboratory

```cpp
// Add cognitive laboratory component to your actor
UCognitiveLaboratory* CogLab = CreateDefaultSubobject<UCognitiveLaboratory>(TEXT("CognitiveLab"));
CogLab->LaboratoryName = TEXT("OpenCog Research Lab");
CogLab->InitializeLaboratory();

// Enable/disable specific subsystems
CogLab->SetSubsystemEnabled(TEXT("LLMEngine"), true);
CogLab->SetSubsystemEnabled(TEXT("RoboticsInterface"), false);

// Get status of all subsystems
TMap<FString, bool> Status = CogLab->GetSubsystemStatus();
for (const auto& Pair : Status)
{
    UE_LOG(LogTemp, Log, TEXT("Subsystem %s: %s"), 
        *Pair.Key, Pair.Value ? TEXT("Active") : TEXT("Inactive"));
}
```

### Creating Marduk's Robotics Lab

```cpp
// Spawn robotics lab
AMardukRoboticsLab* RoboticsLab = GetWorld()->SpawnActor<AMardukRoboticsLab>();
RoboticsLab->LabName = TEXT("Autonomous Robotics Research Lab");
RoboticsLab->ROSMasterURI = TEXT("http://192.168.1.100:11311");
RoboticsLab->HomeAssistantURL = TEXT("http://192.168.1.50:8123");
RoboticsLab->InitializeLab();

// Connect to ROS
if (RoboticsLab->ConnectToROS())
{
    // Subscribe to robot sensor data
    RoboticsLab->SubscribeToTopic(TEXT("/robot/camera/image"), TEXT("sensor_msgs/Image"));
    
    // Register robots
    RoboticsLab->RegisterRobot(TEXT("TurtleBot3_01"));
    RoboticsLab->RegisterRobot(TEXT("UR5_Arm_01"));
}

// Connect to Home Assistant and control IoT devices
if (RoboticsLab->ConnectToHomeAssistant())
{
    RoboticsLab->RegisterDevice(TEXT("light.lab_overhead"));
    RoboticsLab->ControlDevice(TEXT("light.lab_overhead"), TEXT("turn_on"));
}

// Add cognitive agents to coordinate robots
UOpenCogAgent* RobotAgent = NewObject<UOpenCogAgent>();
RobotAgent->InitializeAgent(TEXT("RobotController_01"));
RoboticsLab->AddLabAgent(RobotAgent);
```

### Blueprint Usage

All classes are fully Blueprint-accessible:

1. **Virtual Environment**: Drag `AVirtualEnvironment` into your level from the Place Actors panel
2. **Cognitive Laboratory**: Add `UCognitiveLaboratory` component to any actor
3. **Robotics Lab**: Spawn `AMardukRoboticsLab` using Blueprint spawn node

## Configuration

### Virtual Environment Settings

- `EnvironmentName`: Identifier for the environment
- `EnvironmentSize`: 3D bounds (X, Y, Z) in Unreal units
- `Description`: Purpose description

### Cognitive Laboratory Settings

- `LaboratoryName`: Identifier for the laboratory
- Subsystems are auto-initialized but can be enabled/disabled at runtime

### Marduk's Robotics Lab Settings

- `LabName`: Identifier for the lab
- `ROSMasterURI`: ROS master address (default: http://localhost:11311)
- `ROSBridgePort`: WebSocket port (default: 9090)
- `HomeAssistantURL`: Home Assistant address (default: http://localhost:8123)
- `HomeAssistantToken`: Access token for Home Assistant API

## Integration with Existing Systems

The Virtual Environment Space seamlessly integrates with all existing CogEngine modules:

- **OpenCogCore**: Agent orchestration within spatial bounds
- **AgentZeroKernel**: Distributed cognitive processing
- **KoboldAIStoryweaver**: Narrative coherence for agent interactions
- **DeepTreeEcho**: Embodied cognition with spatial awareness
- **AphroditeEngine**: LLM reasoning for agent decision-making
- **MardukLab**: ROS and IoT physical-virtual integration

## Comparison with aiangel.io

Like aiangel.io, the Virtual Environment Space provides:

- ✅ Virtual 3D environment for AI agents
- ✅ Multi-agent orchestration
- ✅ Real-time cognitive processing
- ✅ Extensible subsystem integration
- ✅ Physical-virtual hybrid support (via ROS/IoT)
- ✅ Spatial awareness and bounds management

Additional features unique to Unreal implementation:

- 🎮 Full Unreal Engine 5 rendering and physics
- 🎯 Blueprint visual scripting support
- 🌍 Scalable to massive open worlds
- 🤖 Deep ROS integration for real robotics
- 🏠 Home Assistant IoT device control
- 📊 Performance optimized for real-time operation

## Performance Considerations

- Virtual environments support thousands of concurrent agents (limited by MaxAgents)
- Cognitive cycle runs at 30 Hz by default (configurable)
- Spatial queries use efficient bounds checking
- All major operations are Blueprint-callable with minimal overhead

## Future Enhancements

Potential future additions:

- Multi-environment networking and federation
- Advanced spatial partitioning for massive agent counts
- Visual debugging tools for agent cognition
- VR/AR support for immersive cognitive lab interaction
- Cloud-based distributed processing
- Integration with more robotics platforms (MoveIt, Nav2, etc.)

## License

Copyright Epic Games, Inc. All Rights Reserved.

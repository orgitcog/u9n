# Quick Start Guide - Virtual Environment Space

## Overview

This guide will help you get started with the Virtual Environment Space module for OpenCog autonomous orchestration in Unreal Engine.

## Prerequisites

- Unreal Engine 5.4 or later
- CogEngine plugin enabled in your project
- Basic familiarity with Unreal Engine and C++ or Blueprints

## Installation

The VirtualEnvironmentSpace module is included with the CogEngine plugin. Simply enable the CogEngine plugin in your project:

1. Open your project in Unreal Engine
2. Go to Edit → Plugins
3. Search for "CogEngine"
4. Enable the plugin
5. Restart the editor

## Scenario 1: Creating a Basic Virtual Environment

### In C++

```cpp
#include "VirtualEnvironment.h"
#include "OpenCogAgent.h"

void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // Spawn virtual environment
    AVirtualEnvironment* Environment = GetWorld()->SpawnActor<AVirtualEnvironment>();
    Environment->EnvironmentName = TEXT("My First Cognitive Lab");
    Environment->EnvironmentSize = FVector(10000.0f, 10000.0f, 5000.0f);
    Environment->InitializeEnvironment();
    
    // Create and add an agent
    UOpenCogAgent* Agent = NewObject<UOpenCogAgent>();
    Agent->InitializeAgent(TEXT("Explorer_001"));
    Agent->AddGoal(TEXT("ExploreEnvironment"));
    
    FVector SpawnPos = Environment->GetRandomLocation();
    Environment->AddAgent(Agent, SpawnPos);
}
```

### In Blueprints

1. Open your level Blueprint or create a new Actor Blueprint
2. Add a "Spawn Actor from Class" node
3. Select `AVirtualEnvironment` as the class
4. Connect to BeginPlay
5. Get a reference to the spawned environment
6. Call "Initialize Environment" on it

## Scenario 2: Setting Up a Cognitive Laboratory

### In C++

```cpp
#include "CognitiveLaboratory.h"

// In your Actor header
UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
UCognitiveLaboratory* CognitiveLab;

// In your Actor constructor
CognitiveLab = CreateDefaultSubobject<UCognitiveLaboratory>(TEXT("CognitiveLab"));

// In BeginPlay
void AMyActor::BeginPlay()
{
    Super::BeginPlay();
    
    CognitiveLab->LaboratoryName = TEXT("Research Lab Alpha");
    CognitiveLab->InitializeLaboratory();
    
    // Enable specific subsystems
    CognitiveLab->SetSubsystemEnabled(TEXT("Orchestrator"), true);
    CognitiveLab->SetSubsystemEnabled(TEXT("LLMEngine"), true);
    CognitiveLab->SetSubsystemEnabled(TEXT("EchoNetwork"), true);
    
    // Check status
    TMap<FString, bool> Status = CognitiveLab->GetSubsystemStatus();
    for (const auto& Pair : Status)
    {
        UE_LOG(LogTemp, Log, TEXT("%s: %s"), 
            *Pair.Key, Pair.Value ? TEXT("Active") : TEXT("Inactive"));
    }
}

// In Tick
void AMyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (CognitiveLab && CognitiveLab->bIsLaboratoryActive)
    {
        CognitiveLab->ProcessCognitiveCycle(DeltaTime);
    }
}
```

### In Blueprints

1. Create a new Actor Blueprint
2. Add a "CognitiveLaboratory" component in the Components panel
3. In the Event Graph:
   - On BeginPlay: Call "Initialize Laboratory"
   - On Tick: Call "Process Cognitive Cycle" with Delta Time
4. Configure subsystems using "Set Subsystem Enabled" nodes

## Scenario 3: Creating Marduk's Robotics Lab

### In C++

```cpp
#include "MardukRoboticsLab.h"

void AMyGameMode::SetupRoboticsLab()
{
    // Spawn robotics lab
    AMardukRoboticsLab* Lab = GetWorld()->SpawnActor<AMardukRoboticsLab>();
    Lab->LabName = TEXT("Autonomous Robotics Research Facility");
    Lab->ROSMasterURI = TEXT("http://192.168.1.100:11311");
    Lab->ROSBridgePort = 9090;
    Lab->HomeAssistantURL = TEXT("http://192.168.1.50:8123");
    Lab->HomeAssistantToken = TEXT("your_access_token_here");
    
    Lab->InitializeLab();
    
    // Connect to ROS
    if (Lab->ConnectToROS())
    {
        // Subscribe to robot topics
        Lab->SubscribeToTopic(TEXT("/robot/odom"), TEXT("nav_msgs/Odometry"));
        Lab->SubscribeToTopic(TEXT("/camera/image"), TEXT("sensor_msgs/Image"));
        
        // Register robots
        Lab->RegisterRobot(TEXT("TurtleBot3_Alpha"));
        Lab->RegisterRobot(TEXT("TurtleBot3_Beta"));
    }
    
    // Connect to Home Assistant
    if (Lab->ConnectToHomeAssistant())
    {
        // Register and control IoT devices
        Lab->RegisterDevice(TEXT("light.lab_ceiling"));
        Lab->RegisterDevice(TEXT("switch.robot_charger"));
        
        Lab->ControlDevice(TEXT("light.lab_ceiling"), TEXT("turn_on"));
    }
    
    // Check lab status
    FString Status = Lab->GetLabStatus();
    UE_LOG(LogTemp, Log, TEXT("Lab Status:\n%s"), *Status);
}
```

### Publishing ROS Messages

```cpp
// Publish robot command
FString CommandMessage = TEXT("{\"linear\": {\"x\": 0.5}, \"angular\": {\"z\": 0.0}}");
Lab->PublishToTopic(TEXT("/cmd_vel"), CommandMessage);
```

### Controlling IoT Devices

```cpp
// Turn on lab lights
Lab->ControlDevice(TEXT("light.lab_overhead"), TEXT("turn_on"));

// Adjust brightness
Lab->ControlDevice(TEXT("light.lab_overhead"), TEXT("set_brightness"));

// Control robot charger
Lab->ControlDevice(TEXT("switch.robot_charger"), TEXT("turn_on"));
```

## Scenario 4: Complete Integrated System

### Combining All Components

```cpp
#include "VirtualEnvironment.h"
#include "CognitiveLaboratory.h"
#include "MardukRoboticsLab.h"
#include "OpenCogAgent.h"

void AMyGameMode::CreateIntegratedCognitiveSystem()
{
    // 1. Create virtual environment
    AVirtualEnvironment* VirtEnv = GetWorld()->SpawnActor<AVirtualEnvironment>();
    VirtEnv->EnvironmentName = TEXT("Integrated Cognitive Space");
    VirtEnv->EnvironmentSize = FVector(20000.0f, 20000.0f, 10000.0f);
    VirtEnv->InitializeEnvironment();
    
    // 2. Create robotics lab
    AMardukRoboticsLab* RobotLab = GetWorld()->SpawnActor<AMardukRoboticsLab>();
    RobotLab->InitializeLab();
    RobotLab->ConnectToROS();
    RobotLab->ConnectToHomeAssistant();
    
    // 3. Create cognitive laboratory component
    UCognitiveLaboratory* CogLab = NewObject<UCognitiveLaboratory>(this);
    CogLab->RegisterComponent();
    CogLab->InitializeLaboratory();
    
    // 4. Create cognitive agents
    for (int32 i = 0; i < 10; i++)
    {
        UOpenCogAgent* Agent = NewObject<UOpenCogAgent>();
        Agent->InitializeAgent(FString::Printf(TEXT("Agent_%03d"), i));
        
        // Add goals
        Agent->AddGoal(TEXT("ExploreEnvironment"));
        Agent->AddGoal(TEXT("LearnFromSensors"));
        
        // Add to virtual environment
        FVector SpawnPos = VirtEnv->GetRandomLocation();
        VirtEnv->AddAgent(Agent, SpawnPos);
        
        // Add to robotics lab
        RobotLab->AddLabAgent(Agent);
    }
    
    // 5. Set up update loop
    GetWorld()->GetTimerManager().SetTimer(
        UpdateTimerHandle,
        [VirtEnv, CogLab]() {
            float DeltaTime = 1.0f / 30.0f; // 30 Hz
            VirtEnv->UpdateEnvironment(DeltaTime);
            CogLab->ProcessCognitiveCycle(DeltaTime);
        },
        1.0f / 30.0f,
        true
    );
}

FTimerHandle UpdateTimerHandle;
```

## Configuration

### DefaultCogEngine.ini Settings

Located at: `Engine/Plugins/Experimental/CogEngine/Config/DefaultCogEngine.ini`

```ini
[VirtualEnvironmentSpace]
; Virtual Environment Configuration
DefaultEnvironmentSizeX=10000.0
DefaultEnvironmentSizeY=10000.0
DefaultEnvironmentSizeZ=5000.0
MaxConcurrentEnvironments=10

; Cognitive Laboratory Configuration
EnableAllSubsystemsByDefault=true
DefaultLaboratoryName=OpenCog Cognitive Laboratory

; Marduk's Robotics Lab Configuration
MaxConcurrentRobots=100
MaxConcurrentDevices=1000
DefaultRoboticsLabName=Marduk's Robotics Laboratory
```

## Common Patterns

### Pattern 1: Agent Spawning in Bounds

```cpp
// Get random valid location
FVector SpawnLocation = VirtualEnv->GetRandomLocation();

// Check if specific location is valid
if (VirtualEnv->IsLocationInBounds(MyLocation))
{
    VirtualEnv->AddAgent(Agent, MyLocation);
}
```

### Pattern 2: Subsystem Monitoring

```cpp
TMap<FString, bool> Status = CognitiveLab->GetSubsystemStatus();
int32 ActiveCount = 0;
for (const auto& Pair : Status)
{
    if (Pair.Value) ActiveCount++;
}
UE_LOG(LogTemp, Log, TEXT("Active Subsystems: %d/%d"), ActiveCount, Status.Num());
```

### Pattern 3: Lab Status Reporting

```cpp
FString LabStatus = RoboticsLab->GetLabStatus();
// Use for debugging or UI display
UE_LOG(LogTemp, Log, TEXT("%s"), *LabStatus);
```

## Troubleshooting

### Issue: Agents not being added to environment
**Solution**: Check that the environment is initialized and the spawn location is within bounds

```cpp
if (!VirtualEnv->bIsActive)
{
    UE_LOG(LogTemp, Error, TEXT("Environment not initialized!"));
    return;
}
if (!VirtualEnv->IsLocationInBounds(SpawnPos))
{
    UE_LOG(LogTemp, Warning, TEXT("Spawn position out of bounds!"));
    SpawnPos = VirtualEnv->GetRandomLocation();
}
```

### Issue: ROS connection fails
**Solution**: Verify ROS master is running and accessible

```bash
# On your ROS system
roscore  # Start ROS master
rosbridge_server  # Start WebSocket bridge
```

### Issue: Home Assistant connection fails
**Solution**: Check Home Assistant is accessible and token is valid

```bash
# Test Home Assistant connection
curl -X GET \
  http://localhost:8123/api/ \
  -H "Authorization: Bearer YOUR_TOKEN"
```

### Issue: Subsystems not initializing
**Solution**: Check module dependencies are loaded

```cpp
// Verify subsystem is not null before using
if (CognitiveLab->Orchestrator)
{
    // Safe to use
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Orchestrator failed to initialize!"));
}
```

## Next Steps

- Explore the full API documentation in VIRTUAL_ENVIRONMENT_SPACE.md
- Review the architecture diagram in ARCHITECTURE.md
- Check out example blueprints in the Content/CogEngine folder (if available)
- Experiment with different configurations in DefaultCogEngine.ini

## Support

For issues and questions:
- Review the main README.md
- Check the Unreal Engine documentation
- Visit the CogEngine repository: https://github.com/cogpy/UnrealCogEngine

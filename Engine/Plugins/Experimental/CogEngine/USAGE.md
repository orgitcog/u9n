# CogEngine Usage Examples

## Virtual Environment Space - New Features

### 1. Creating a Virtual Environment

```cpp
// C++ Example - Create a 3D virtual space for cognitive agents
AVirtualEnvironment* VirtualEnv = GetWorld()->SpawnActor<AVirtualEnvironment>();
VirtualEnv->EnvironmentName = TEXT("Cognitive Laboratory 01");
VirtualEnv->EnvironmentSize = FVector(10000.0f, 10000.0f, 5000.0f);
VirtualEnv->InitializeEnvironment();

// Add agents to the virtual space
UOpenCogAgent* Agent = NewObject<UOpenCogAgent>();
Agent->InitializeAgent(TEXT("Explorer_001"));
FVector SpawnLocation = VirtualEnv->GetRandomLocation();
VirtualEnv->AddAgent(Agent, SpawnLocation);
```

### 2. Setting Up a Cognitive Laboratory

```cpp
// Create a unified cognitive laboratory component
UCognitiveLaboratory* CogLab = CreateDefaultSubobject<UCognitiveLaboratory>(TEXT("CogLab"));
CogLab->LaboratoryName = TEXT("OpenCog Research Lab");
CogLab->InitializeLaboratory();

// Enable specific subsystems
CogLab->SetSubsystemEnabled(TEXT("Orchestrator"), true);
CogLab->SetSubsystemEnabled(TEXT("LLMEngine"), true);
CogLab->SetSubsystemEnabled(TEXT("RoboticsInterface"), true);

// Get subsystem status
TMap<FString, bool> Status = CogLab->GetSubsystemStatus();
```

### 3. Creating Marduk's Robotics Lab

```cpp
// Create enhanced robotics laboratory
AMardukRoboticsLab* RoboticsLab = GetWorld()->SpawnActor<AMardukRoboticsLab>();
RoboticsLab->LabName = TEXT("Autonomous Robotics Lab");
RoboticsLab->ROSMasterURI = TEXT("http://localhost:11311");
RoboticsLab->HomeAssistantURL = TEXT("http://localhost:8123");
RoboticsLab->InitializeLab();

// Connect to ROS
RoboticsLab->ConnectToROS();
RoboticsLab->RegisterRobot(TEXT("TurtleBot3_01"));
RoboticsLab->SubscribeToTopic(TEXT("/camera/image"), TEXT("sensor_msgs/Image"));

// Connect to IoT devices
RoboticsLab->ConnectToHomeAssistant();
RoboticsLab->RegisterDevice(TEXT("light.lab_overhead"));
RoboticsLab->ControlDevice(TEXT("light.lab_overhead"), TEXT("turn_on"));
```

### 4. Integrated Virtual Environment System

```cpp
// Complete integrated cognitive system
void SetupIntegratedSystem()
{
    // Create virtual environment
    AVirtualEnvironment* VirtEnv = GetWorld()->SpawnActor<AVirtualEnvironment>();
    VirtEnv->EnvironmentSize = FVector(20000.0f, 20000.0f, 10000.0f);
    VirtEnv->InitializeEnvironment();
    
    // Create robotics lab
    AMardukRoboticsLab* Lab = GetWorld()->SpawnActor<AMardukRoboticsLab>();
    Lab->InitializeLab();
    Lab->ConnectToROS();
    
    // Create cognitive laboratory
    UCognitiveLaboratory* CogLab = NewObject<UCognitiveLaboratory>(this);
    CogLab->InitializeLaboratory();
    
    // Create agents and add to both virtual environment and robotics lab
    for (int32 i = 0; i < 10; i++)
    {
        UOpenCogAgent* Agent = NewObject<UOpenCogAgent>();
        Agent->InitializeAgent(FString::Printf(TEXT("Agent_%03d"), i));
        
        FVector SpawnPos = VirtEnv->GetRandomLocation();
        VirtEnv->AddAgent(Agent, SpawnPos);
        Lab->AddLabAgent(Agent);
    }
}
```

## Getting Started

### 1. Basic Setup in Blueprint

1. Create a new Blueprint Actor class
2. Add a `CogEngineManager` component
3. Call `InitializeAllSystems()` in BeginPlay
4. The manager will automatically process cognitive cycles

### 2. Creating Autonomous Agents

```cpp
// C++ Example
ACogEngineManager* Manager = GetWorld()->SpawnActor<ACogEngineManager>();
UOpenCogAgent* Agent = Manager->CreateCognitiveAgent("Agent001");
Agent->AddGoal("ExploreWorld");
Agent->UpdateBelief("Curiosity", 0.8f);
```

### 3. World-Building with KoboldAI

```cpp
// Create story arcs for players
UAutognosticStoryweaver* Storyweaver = Manager->Storyweaver;
Storyweaver->CreateStoryArc("Player123", "HeroJourney");
Storyweaver->UpdateArcProgression("HeroJourney", "MeetingMentor");
```

### 4. Distributed Cognitive Processing

```cpp
// Use Agent-Zero for distributed tasks
UAgentZeroMicrokernel* Kernel = Manager->DistributedKernel;
Kernel->JoinNetwork("ws://cognitive-cluster:9000");

float Result;
Kernel->ProcessCognitiveTask("ComplexDecision", Result);
```

### 5. Echo-State Network Integration

```cpp
// Process sensory input through embodied cognition
UEchoStateNetwork* Network = Manager->EchoNetwork;

TArray<float> SensoryInput = {1.0f, 0.5f, 0.3f, 0.8f};
TArray<float> CognitiveOutput = Network->ProcessInput(SensoryInput);
```

### 6. LLM-Powered Agent Reasoning

```cpp
// Submit reasoning requests to LLM
Manager->SubmitAgentReasoningRequest("Agent001", 
    "Should I help the player or follow my own goals?");

// Check for response later
FLLMInferenceResponse Response;
if (Manager->LLMEngine->GetInferenceResponse(RequestID, Response))
{
    UE_LOG(LogTemp, Log, TEXT("Agent Decision: %s"), *Response.GeneratedText);
}
```

### 7. Robotics Integration with ROS

```cpp
// Connect to ROS master
UROSBridge* ROS = Manager->RoboticsInterface;
ROS->Connect();
ROS->SubscribeTopic("/robot/odometry", "nav_msgs/Odometry");

// Publish commands
FROSMessage Command;
Command.Topic = "/robot/cmd_vel";
Command.MessageType = "geometry_msgs/Twist";
Command.Data = "{\"linear\":{\"x\":0.5}}";
ROS->PublishMessage(Command);
```

### 8. IoT Device Control

```cpp
// Control smart home devices
UHomeAssistantBridge* IoT = Manager->IoTInterface;
IoT->Connect();

// Turn on lights based on agent behavior
IoT->CallService("light", "turn_on", "light.game_room");

// Check sensor states
FString Temperature;
IoT->GetDeviceState("sensor.temperature", Temperature);
```

## Advanced Integration Examples

### Multi-Agent Collaborative Story Generation

```cpp
void AGameMode::SpawnPlayerAgent(APlayerController* Player)
{
    // Create dedicated agent for player
    UOpenCogAgent* PlayerAgent = CogManager->CreateCognitiveAgent(
        FString::Printf(TEXT("Player_%s"), *Player->GetName())
    );
    
    // Create player's story arc
    FString ArcID = FString::Printf(TEXT("Arc_%s"), *Player->GetName());
    CogManager->Storyweaver->CreateStoryArc(Player->GetName(), ArcID);
    
    // Initialize agent goals based on story
    PlayerAgent->AddGoal("DiscoverOrigin");
    PlayerAgent->AddGoal("BuildAlliances");
    
    // Connect to LLM for dynamic narrative
    CogManager->SubmitAgentReasoningRequest(
        PlayerAgent->AgentID,
        "Generate opening narrative for hero's journey"
    );
}
```

### Distributed Multi-Agent Simulation

```cpp
void ACogEngineManager::SpawnAgentSwarm(int32 NumAgents)
{
    for (int32 i = 0; i < NumAgents; ++i)
    {
        FString AgentID = FString::Printf(TEXT("SwarmAgent_%d"), i);
        UOpenCogAgent* Agent = CreateCognitiveAgent(AgentID);
        
        // Distribute cognitive load across kernels
        UAgentZeroMicrokernel* Kernel = NewObject<UAgentZeroMicrokernel>();
        Kernel->Initialize(AgentID);
        Kernel->JoinNetwork("ws://localhost:9000");
        
        // Each agent has echo-state network for decision making
        UEchoStateNetwork* Network = NewObject<UEchoStateNetwork>();
        Network->InitializeNetwork(3, 50); // Smaller network per agent
    }
}
```

### Robot-Virtual World Synchronization

```cpp
void ARobotAvatar::SyncWithPhysicalRobot()
{
    UROSBridge* ROS = CogManager->RoboticsInterface;
    
    // Subscribe to robot pose
    ROS->SubscribeTopic("/robot/pose", "geometry_msgs/PoseStamped");
    
    FROSMessage PoseMsg;
    if (ROS->GetLatestMessage("/robot/pose", PoseMsg))
    {
        // Update virtual avatar position based on real robot
        UpdateVirtualPosition(PoseMsg.Data);
    }
    
    // Send virtual world state to robot
    FROSMessage WorldState;
    WorldState.Topic = "/virtual_world/obstacles";
    WorldState.MessageType = "sensor_msgs/PointCloud2";
    WorldState.Data = GenerateObstacleCloud();
    ROS->PublishMessage(WorldState);
}
```

### Adaptive Difficulty with Coherent Narratives

```cpp
void AGameplayManager::UpdateDifficulty()
{
    // Use storyweaver to maintain narrative coherence
    float PlayerProgression = CalculatePlayerProgress();
    
    FString CurrentPhase = PlayerProgression < 0.3f ? "Beginning" :
                          PlayerProgression < 0.7f ? "RisingAction" : "Climax";
    
    CogManager->Storyweaver->UpdateArcProgression(PlayerArcID, CurrentPhase);
    
    // Align difficulty with story
    float CoherenceScore = CogManager->Storyweaver->AlignArcWithWorldCore(PlayerArcID);
    
    // Use LLM to generate contextual challenges
    FString Context = FString::Printf(
        TEXT("Player in %s phase, coherence: %f. Generate appropriate challenge."),
        *CurrentPhase, CoherenceScore
    );
    CogManager->SubmitAgentReasoningRequest("DifficultyAgent", Context);
}
```

## Blueprint Usage

### Creating Agents in Blueprint

1. Add Event BeginPlay node
2. Get reference to CogEngineManager
3. Call "Create Cognitive Agent" with desired Agent ID
4. Call "Add Goal" on the returned agent
5. The agent will automatically participate in orchestration

### Monitoring System Status

1. Add Event Tick node
2. Call "Get System Status" on CogEngineManager
3. Print String to display status
4. Use for debugging and monitoring

### Responding to LLM Inference

1. Create Custom Event for handling LLM responses
2. In Tick, call "Get Inference Response" with Request ID
3. On success, process the generated text
4. Update agent behavior based on LLM reasoning

## Performance Considerations

### Agent Count
- Start with 10-100 agents for testing
- Scale up to thousands for production
- Monitor CPU usage and adjust update frequency

### Story Arc Management
- Designed for billion-scale arcs
- Use coherence threshold to prioritize active arcs
- Archive inactive arcs to save memory

### Echo-State Networks
- Network depth affects computation time
- Larger reservoirs = more memory, better performance
- Balance based on available resources

### LLM Inference
- Async request/response model prevents blocking
- Queue requests during high load
- Cache common responses

### ROS/IoT Integration
- WebSocket connections are persistent
- Limit topic subscriptions to essential data
- Use message buffering for reliability

## Troubleshooting

### Agents Not Responding
- Check if orchestrator is initialized
- Verify agents are registered
- Ensure cognitive cycle is being called

### LLM Not Responding
- Verify server URL is correct
- Check API key if required
- Monitor network connectivity

### ROS Connection Failed
- Verify ROS master is running
- Check WebSocket port is open
- Ensure rosbridge_server is installed

### IoT Devices Not Found
- Verify Home Assistant URL
- Check access token
- Ensure devices are configured in Home Assistant

# CogEngine Deployment Architecture

This document contains Mermaid diagrams showing the deployment architecture and operational aspects of CogEngine.

## Table of Contents
1. [Deployment Architecture](#1-deployment-architecture)
2. [Network Communication](#2-network-communication)
3. [Module Dependencies](#3-module-dependencies)
4. [Configuration Flow](#4-configuration-flow)
5. [Scaling Architecture](#5-scaling-architecture)

---

## 1. Deployment Architecture

```mermaid
graph TB
    subgraph UnrealEngine["Unreal Engine Deployment"]
        subgraph GameServer["Game Server Instance"]
            UE["UE5 Runtime"]
            CogE["CogEngine Plugin"]
            BP["Blueprint Logic"]
            
            subgraph Modules["CogEngine Modules"]
                M1["VirtualEnvironmentSpace"]
                M2["OpenCogCore"]
                M3["AgentZeroKernel"]
                M4["KoboldAIStoryweaver"]
                M5["DeepTreeEcho"]
                M6["AphroditeEngine"]
                M7["MardukLab"]
                M8["CogEngine Core"]
            end
        end
        
        subgraph Editor["Editor Instance"]
            UEE["UE5 Editor"]
            CogEE["CogEngineEditor"]
        end
    end
    
    subgraph AIInfra["AI Infrastructure"]
        subgraph LLMCluster["LLM Inference Cluster"]
            LLM1["Llama-3-70B<br/>GPU Server 1"]
            LLM2["Mistral-7B<br/>GPU Server 2"]
            LLM3["Mixtral-8x7B<br/>GPU Server 3"]
            LB["Load Balancer<br/>:2242"]
        end
    end
    
    subgraph RoboticsInfra["Robotics Infrastructure"]
        subgraph ROSNetwork["ROS Network"]
            ROSM["ROS Master<br/>:11311"]
            ROSB["rosbridge_server<br/>:9090"]
            Robots["Physical Robots<br/>- TurtleBot3<br/>- UR5 Arms"]
        end
    end
    
    subgraph IoTInfra["IoT Infrastructure"]
        subgraph HANetwork["Home Assistant Network"]
            HAS["Home Assistant<br/>:8123"]
            MQTT["MQTT Broker<br/>:1883"]
            Devices["Smart Devices<br/>- Lights<br/>- Sensors<br/>- Switches"]
        end
    end
    
    subgraph DistCompute["Distributed Computing (Future)"]
        subgraph AAA["Arc-Halo-Aphrodite-Yggdrasil"]
            AH_Nodes["Arc-Halo Nodes<br/>Cognitive Fusion"]
            APH_Nodes["Aphrodite Nodes<br/>Hypergraph Engine"]
            YGG_Nodes["Yggdrasil Nodes<br/>Distributed Atom Space"]
        end
    end
    
    CogE --> M1
    CogE --> M2
    CogE --> M3
    CogE --> M4
    CogE --> M5
    CogE --> M6
    CogE --> M7
    CogE --> M8
    
    M6 -->|HTTP/REST| LB
    LB --> LLM1
    LB --> LLM2
    LB --> LLM3
    
    M7 -->|WebSocket| ROSB
    ROSB --> ROSM
    ROSM --> Robots
    
    M7 -->|HTTP API| HAS
    HAS --> MQTT
    MQTT --> Devices
    
    M3 -.->|Future| AH_Nodes
    M6 -.->|Future| APH_Nodes
    M2 -.->|Future| YGG_Nodes
    
    CogEE -.->|Development| CogE
    
    style CogE fill:#4a9eff
    style LB fill:#ffcc66
    style ROSM fill:#ff9999
    style HAS fill:#9cf49c
    style AAA fill:#ff69b4
```

---

## 2. Network Communication

```mermaid
sequenceDiagram
    participant Game as Game Instance
    participant CogEngine as CogEngine Plugin
    participant LLM as LLM Load Balancer<br/>:2242
    participant ROS as ROS Bridge<br/>:9090
    participant HA as Home Assistant<br/>:8123
    
    Note over Game,HA: Initialization Phase
    Game->>CogEngine: Initialize All Systems
    CogEngine->>LLM: Health Check
    LLM-->>CogEngine: Status: OK
    CogEngine->>ROS: WebSocket Connect
    ROS-->>CogEngine: Connected
    CogEngine->>HA: API Health Check
    HA-->>CogEngine: Authenticated
    
    Note over Game,HA: Runtime Operation
    
    par LLM Inference
        CogEngine->>LLM: POST /v1/completions<br/>{prompt, model, params}
        LLM->>LLM: Queue Request
        LLM->>LLM: Process on GPU
        LLM-->>CogEngine: {completion, tokens, timing}
    end
    
    par ROS Communication
        CogEngine->>ROS: {op: "subscribe", topic: "/odom"}
        ROS->>ROS: Register Subscription
        loop Topic Updates
            ROS-->>CogEngine: {topic: "/odom", msg: {...}}
        end
        CogEngine->>ROS: {op: "publish", topic: "/cmd_vel"}
        ROS->>ROS: Forward to ROS Network
    end
    
    par IoT Control
        CogEngine->>HA: POST /api/services/light/turn_on<br/>{entity_id: "light.lab"}
        HA->>HA: Execute Service
        HA-->>CogEngine: {success: true}
        loop State Updates
            HA-->>CogEngine: State Change Event
        end
    end
    
    Note over Game,HA: Shutdown Phase
    Game->>CogEngine: Shutdown All Systems
    CogEngine->>ROS: Close WebSocket
    CogEngine->>LLM: Cancel Pending Requests
    CogEngine->>HA: Disconnect
```

---

## 3. Module Dependencies

```mermaid
graph LR
    subgraph UnrealCore["Unreal Engine Core"]
        CoreUObject["CoreUObject"]
        Engine["Engine"]
        HTTP["HTTP"]
        WebSockets["WebSockets"]
        Json["Json"]
    end
    
    subgraph CogEngineModules["CogEngine Modules"]
        CogCore["CogEngine<br/>(Core Module)"]
        VES["VirtualEnvironmentSpace"]
        OCO["OpenCogCore"]
        AZK["AgentZeroKernel"]
        KAS["KoboldAIStoryweaver"]
        DTE["DeepTreeEcho"]
        APH["AphroditeEngine"]
        MRL["MardukLab"]
        CogEditor["CogEngineEditor"]
    end
    
    subgraph External["External Dependencies"]
        LLMServer["LLM Servers"]
        ROSSystem["ROS System"]
        HASystem["Home Assistant"]
    end
    
    %% Unreal dependencies
    CogCore --> CoreUObject
    CogCore --> Engine
    VES --> CoreUObject
    VES --> Engine
    OCO --> CoreUObject
    AZK --> CoreUObject
    KAS --> CoreUObject
    DTE --> CoreUObject
    APH --> CoreUObject
    APH --> HTTP
    APH --> Json
    MRL --> CoreUObject
    MRL --> HTTP
    MRL --> WebSockets
    MRL --> Json
    
    %% Inter-module dependencies
    CogCore --> VES
    CogCore --> OCO
    CogCore --> AZK
    CogCore --> KAS
    CogCore --> DTE
    CogCore --> APH
    CogCore --> MRL
    
    VES --> OCO
    OCO --> AZK
    OCO --> DTE
    OCO --> KAS
    
    CogEditor --> CogCore
    
    %% External dependencies
    APH -.-> LLMServer
    MRL -.-> ROSSystem
    MRL -.-> HASystem
    
    style CogCore fill:#4a9eff
    style VES fill:#9cf49c
    style APH fill:#ffcc66
    style MRL fill:#ff9999
```

---

## 4. Configuration Flow

```mermaid
flowchart TD
    Start([Engine Startup]) --> LoadPlugin[Load CogEngine Plugin]
    LoadPlugin --> ReadConfig[Read DefaultCogEngine.ini]
    
    ReadConfig --> ParseVES[Parse VirtualEnvironmentSpace Config]
    ReadConfig --> ParseOCO[Parse OpenCogCore Config]
    ReadConfig --> ParseAZK[Parse AgentZeroKernel Config]
    ReadConfig --> ParseKAS[Parse KoboldAI Config]
    ReadConfig --> ParseDTE[Parse DeepTreeEcho Config]
    ReadConfig --> ParseAPH[Parse AphroditeEngine Config]
    ReadConfig --> ParseMRL[Parse MardukLab Config]
    
    ParseVES --> SetVES{Valid Config?}
    ParseOCO --> SetOCO{Valid Config?}
    ParseAZK --> SetAZK{Valid Config?}
    ParseKAS --> SetKAS{Valid Config?}
    ParseDTE --> SetDTE{Valid Config?}
    ParseAPH --> SetAPH{Valid Config?}
    ParseMRL --> SetMRL{Valid Config?}
    
    SetVES -->|Yes| InitVES[Initialize Virtual Env<br/>Size: 10000x10000x5000 units]
    SetVES -->|No| DefaultVES[Use Default Values]
    DefaultVES --> InitVES
    
    SetOCO -->|Yes| InitOCO[Initialize Orchestrator<br/>MaxAgents: 1000]
    SetOCO -->|No| DefaultOCO[Use Default Values]
    DefaultOCO --> InitOCO
    
    SetAZK -->|Yes| InitAZK[Initialize Kernel<br/>SyncInterval: 1.0s]
    SetAZK -->|No| DefaultAZK[Use Default Values]
    DefaultAZK --> InitAZK
    
    SetKAS -->|Yes| InitKAS[Initialize Storyweaver<br/>MaxArcs: 1B]
    SetKAS -->|No| DefaultKAS[Use Default Values]
    DefaultKAS --> InitKAS
    
    SetDTE -->|Yes| InitDTE[Initialize Echo Network<br/>Depth: 5, Size: 100]
    SetDTE -->|No| DefaultDTE[Use Default Values]
    DefaultDTE --> InitDTE
    
    SetAPH -->|Yes| InitAPH[Initialize LLM Engine<br/>URL: localhost:2242]
    SetAPH -->|No| DefaultAPH[Use Default Values]
    DefaultAPH --> InitAPH
    
    SetMRL -->|Yes| InitMRL[Initialize Marduk Lab<br/>ROS: 11311, HA: 8123]
    SetMRL -->|No| DefaultMRL[Use Default Values]
    DefaultMRL --> InitMRL
    
    InitVES --> AllInit{All Systems<br/>Initialized?}
    InitOCO --> AllInit
    InitAZK --> AllInit
    InitKAS --> AllInit
    InitDTE --> AllInit
    InitAPH --> AllInit
    InitMRL --> AllInit
    
    AllInit -->|Yes| Ready[System Ready]
    AllInit -->|No| Error[Log Errors]
    Error --> PartialReady[Partial System Ready]
    
    Ready --> StartCycle[Start Cognitive Cycle<br/>30 Hz]
    PartialReady --> StartCycle
    
    style ReadConfig fill:#4a9eff
    style Ready fill:#9cf49c
    style Error fill:#ff9999
```

---

## 5. Scaling Architecture

### Current Scale (Single Instance)

```mermaid
graph TB
    subgraph SingleInstance["Single Game Server"]
        GM["Game Instance"]
        CE["CogEngine"]
        
        subgraph Limits["Resource Limits"]
            A["Max Agents: 1,000"]
            S["Story Arcs: 1B (in-memory)"]
            U["Update Rate: 30 Hz"]
            M["Memory: ~4-8 GB"]
        end
        
        GM --> CE
        CE --> Limits
    end
    
    subgraph Services["External Services"]
        LLM["LLM Servers<br/>Horizontally Scalable"]
        ROS["ROS Network<br/>Multiple Robots"]
        HA["Home Assistant<br/>1000+ Devices"]
    end
    
    CE --> LLM
    CE --> ROS
    CE --> HA
    
    style CE fill:#4a9eff
```

### Future Scale (Distributed with AAA)

```mermaid
graph TB
    subgraph Cluster["Distributed CogEngine Cluster"]
        subgraph GS1["Game Server 1"]
            CE1["CogEngine Instance"]
            Agents1["Agents 1-1000"]
        end
        
        subgraph GS2["Game Server 2"]
            CE2["CogEngine Instance"]
            Agents2["Agents 1001-2000"]
        end
        
        subgraph GS3["Game Server N"]
            CE3["CogEngine Instance"]
            AgentsN["Agents (N*1000)+"]
        end
    end
    
    subgraph AAALayer["AAA Integration Layer"]
        subgraph ArcHalo["Arc-Halo Cluster"]
            AH1["Fusion Node 1"]
            AH2["Fusion Node 2"]
            AH3["Fusion Node N"]
        end
        
        subgraph Aphrodite["Aphrodite Cluster"]
            APH1["Hypergraph Node 1"]
            APH2["Hypergraph Node 2"]
            APH3["Hypergraph Node N"]
        end
        
        subgraph Yggdrasil["Yggdrasil Cluster"]
            YGG1["Atom Space Node 1"]
            YGG2["Atom Space Node 2"]
            YGG3["Atom Space Node N"]
        end
    end
    
    subgraph SharedServices["Shared Services"]
        LLM["LLM Farm<br/>GPU Cluster"]
        ROS["ROS Federation<br/>Multi-Site"]
        HA["HA Network<br/>Distributed"]
        Storage["Persistent Storage<br/>Distributed DB"]
    end
    
    CE1 --> AH1
    CE2 --> AH2
    CE3 --> AH3
    
    AH1 --> APH1
    AH2 --> APH2
    AH3 --> APH3
    
    APH1 --> YGG1
    APH2 --> YGG2
    APH3 --> YGG3
    
    YGG1 <--> YGG2
    YGG2 <--> YGG3
    YGG3 <--> YGG1
    
    Cluster --> LLM
    Cluster --> ROS
    Cluster --> HA
    AAALayer --> Storage
    
    style CE1 fill:#4a9eff
    style CE2 fill:#4a9eff
    style CE3 fill:#4a9eff
    style ArcHalo fill:#ff6b6b
    style Aphrodite fill:#9b59b6
    style Yggdrasil fill:#2ecc71
```

### Scaling Metrics

```mermaid
graph LR
    subgraph Current["Current Capacity"]
        C1["Agents: 1,000<br/>per instance"]
        C2["Story Arcs: 1B<br/>in-memory"]
        C3["Update Rate: 30 Hz"]
        C4["Latency: <33ms"]
    end
    
    subgraph WithAAA["With AAA Integration"]
        A1["Agents: Unlimited<br/>(distributed)"]
        A2["Story Arcs: Unlimited<br/>(persistent)"]
        A3["Update Rate: 30 Hz<br/>(per node)"]
        A4["Latency: <50ms<br/>(with network)"]
    end
    
    Current -.->|"Evolution<br/>Arc-Halo<br/>Aphrodite<br/>Yggdrasil"| WithAAA
    
    style Current fill:#ffcc66
    style WithAAA fill:#ff69b4
```

---

## Network Topology

```mermaid
graph TB
    subgraph Internet["Internet / WAN"]
        Players["Game Clients"]
    end
    
    subgraph DMZ["DMZ / Edge Network"]
        GameLB["Game Load Balancer"]
        APIGW["API Gateway"]
    end
    
    subgraph GameNetwork["Game Server Network (Private)"]
        subgraph GameServers["Game Servers"]
            GS1["Server 1<br/>CogEngine"]
            GS2["Server 2<br/>CogEngine"]
            GS3["Server N<br/>CogEngine"]
        end
        
        SessionDB["Session Database"]
        SharedCache["Shared Cache"]
    end
    
    subgraph AINetwork["AI Services Network (Private)"]
        LLMLB["LLM Load Balancer"]
        
        subgraph LLMServers["LLM GPU Servers"]
            GPU1["GPU Server 1<br/>Llama-3-70B"]
            GPU2["GPU Server 2<br/>Mistral-7B"]
            GPU3["GPU Server N<br/>Mixtral-8x7B"]
        end
    end
    
    subgraph RoboticsNetwork["Robotics Network (Private/VPN)"]
        ROSM["ROS Master"]
        ROSB["ROS Bridge"]
        Robots["Physical Robots"]
    end
    
    subgraph IoTNetwork["IoT Network (Private/Isolated)"]
        HAMQTT["HA + MQTT"]
        Devices["Smart Devices"]
    end
    
    subgraph AAANetwork["AAA Network (Future - Private)"]
        AAACluster["Arc-Halo-Aphrodite-Yggdrasil<br/>Cluster"]
        DistDB["Distributed Database"]
    end
    
    Players --> GameLB
    GameLB --> GS1
    GameLB --> GS2
    GameLB --> GS3
    
    GS1 --> SessionDB
    GS2 --> SessionDB
    GS3 --> SessionDB
    
    GS1 --> SharedCache
    GS2 --> SharedCache
    GS3 --> SharedCache
    
    GS1 --> LLMLB
    GS2 --> LLMLB
    GS3 --> LLMLB
    
    LLMLB --> GPU1
    LLMLB --> GPU2
    LLMLB --> GPU3
    
    GS1 --> ROSB
    GS2 --> ROSB
    GS3 --> ROSB
    
    ROSB --> ROSM
    ROSM --> Robots
    
    GS1 --> HAMQTT
    GS2 --> HAMQTT
    GS3 --> HAMQTT
    
    HAMQTT --> Devices
    
    GS1 -.->|Future| AAACluster
    GS2 -.->|Future| AAACluster
    GS3 -.->|Future| AAACluster
    
    AAACluster -.-> DistDB
    
    style GameNetwork fill:#e8f4f8
    style AINetwork fill:#fff4e6
    style RoboticsNetwork fill:#ffe6e6
    style IoTNetwork fill:#e6ffe6
    style AAANetwork fill:#ffe6ff
```

---

## Performance Monitoring

```mermaid
graph TD
    subgraph Metrics["Key Performance Metrics"]
        CPU["CPU Usage<br/>Per Module"]
        Memory["Memory Usage<br/>Per Subsystem"]
        Network["Network I/O<br/>To External Services"]
        Latency["Response Latency<br/>LLM/ROS/IoT"]
        Agents["Active Agents<br/>Count & Status"]
        Cycles["Cognitive Cycles<br/>per Second"]
    end
    
    subgraph Monitoring["Monitoring Systems"]
        UEStats["UE Stats System"]
        CustomLog["Custom Logging"]
        Profiler["UE Profiler"]
    end
    
    subgraph Alerts["Alert Conditions"]
        HighCPU["CPU > 80%"]
        HighMem["Memory > 6GB"]
        SlowLLM["LLM > 5s"]
        SlowROS["ROS > 1s"]
        AgentLimit["Agents > 900"]
        LowCycle["Cycles < 25Hz"]
    end
    
    CPU --> UEStats
    Memory --> UEStats
    Network --> CustomLog
    Latency --> CustomLog
    Agents --> UEStats
    Cycles --> Profiler
    
    UEStats --> HighCPU
    UEStats --> HighMem
    CustomLog --> SlowLLM
    CustomLog --> SlowROS
    UEStats --> AgentLimit
    Profiler --> LowCycle
    
    HighCPU --> Action[Take Action:<br/>- Scale Down Agents<br/>- Optimize Processing]
    HighMem --> Action
    SlowLLM --> Action
    SlowROS --> Action
    AgentLimit --> Action
    LowCycle --> Action
    
    style Alerts fill:#ff9999
    style Action fill:#ffcc66
```

---

*Last Updated: 2025-11-03*
*CogEngine Deployment Version: Experimental*
